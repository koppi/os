#include <heap.h>
#include <mm.h>
#include <paging.h>
#include <sched.h>
#include <proc.h>
#include <printf.h>

/** Round an allocation size up so headers stay 4-byte aligned. */
static size_t heap_align(size_t len) {
    return (len + 3u) & ~((size_t) 3u);
}

/**
 * @brief Initialise a user process heap in the 4 pages starting at @p addr.
 *
 * The heap_info_t bookkeeping struct lives at the base, immediately followed by
 * a single free block spanning the rest of the region.
 *
 * @param addr Page-aligned base of the 4-page (PAGE_SIZE * 4) heap region.
 */
void heap_init(vmm_addr_t *addr) {
    heap_info_t *heap_info = (heap_info_t *) addr;
    uint8_t *base = (uint8_t *) addr + sizeof(heap_info_t);
    size_t total = (PAGE_SIZE * 4) - sizeof(heap_info_t);

    heap_info->start = (vmm_addr_t *) base;
    heap_info->size = total;
    heap_info->used = sizeof(heap_header_t);
    heap_info->first_header = (heap_header_t *) base;
    heap_info->first_header->magic = HEAP_MAGIC;
    heap_info->first_header->size = total - sizeof(heap_header_t);
    heap_info->first_header->is_free = 1;
    heap_info->first_header->next = 0;
}

/**
 * @brief First-fit allocator over a user process heap.
 *
 * @param len  Requested payload size in bytes.
 * @param heap Base of the heap region (its heap_info_t).
 * @return Pointer to @p len usable bytes, or NULL if the heap is exhausted or
 *         its header chain is corrupt.
 */
void *umalloc(size_t len, vmm_addr_t *heap) {
    heap_info_t *heap_info = (heap_info_t *) heap;
    heap_header_t *head = heap_info->first_header;

    len = heap_align(len);

    while(head != 0) {
        if(head->magic != HEAP_MAGIC) {
            printf("\numalloc: heap corruption\n");
            return 0;
        }
        if(head->is_free && head->size >= len) {
            /* Split the block only if the tail can hold a header plus a
             * minimal payload; otherwise hand out the whole block. */
            if(head->size >= len + sizeof(heap_header_t) + 4) {
                heap_header_t *split = (heap_header_t *)
                    ((uint8_t *) head + sizeof(heap_header_t) + len);
                split->magic = HEAP_MAGIC;
                split->size = head->size - len - sizeof(heap_header_t);
                split->is_free = 1;
                split->next = head->next;
                head->next = split;
                head->size = len;
                heap_info->used += sizeof(heap_header_t);
            }
            head->is_free = 0;
            heap_info->used += head->size;
            return (uint8_t *) head + sizeof(heap_header_t);
        }
        head = head->next;
    }
    printf("\nOut of memory\n");
    return 0;
}

/**
 * @brief Release a block obtained from umalloc() and coalesce free neighbours.
 *
 * @param ptr  Pointer previously returned by umalloc() (NULL is ignored).
 * @param heap Base of the heap region the block belongs to.
 */
void ufree(void *ptr, vmm_addr_t *heap) {
    if(!ptr)
        return;

    heap_info_t *heap_info = (heap_info_t *) heap;
    heap_header_t *head = (heap_header_t *) ((uint8_t *) ptr - sizeof(heap_header_t));

    if((head->magic != HEAP_MAGIC) || head->is_free)
        return;

    head->is_free = 1;
    heap_info->used -= head->size;

    // Merge contiguous free sections that follow this one.
    heap_header_t *app = head->next;
    while((app != 0) && (app->magic == HEAP_MAGIC) && (app->is_free == 1)) {
        head->size += app->size + sizeof(heap_header_t);
        head->next = app->next;
        heap_info->used -= sizeof(heap_header_t);
        app = app->next;
    }
}

void *umalloc_sys(size_t len) {
    process_t *cur = get_cur_proc();
    if(cur && cur->thread_list) {
        return umalloc(len, (vmm_addr_t *) cur->thread_list->heap);
    }
    return 0;
}

void ufree_sys(void *ptr) {
    process_t *cur = get_cur_proc();
    if(cur && cur->thread_list) {
        ufree(ptr, (vmm_addr_t *) cur->thread_list->heap);
    }
}
