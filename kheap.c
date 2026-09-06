/**
 * @file kheap.c
 * @brief Kernel heap — one first-fit free list over the dedicated identity-
 *        mapped window at @ref KHEAP_BASE (mapped by vmm_init, reserved by
 *        main.c). It used to be squeezed between the page-table window and the
 *        4 MiB line, which starved as the kernel image grew.
 */
#include <kheap.h>
#include <mm.h>
#include <paging.h>
#include <printf.h>
#include <spinlock.h>

heap_info_t heap_info;

/**
 * Init the kernel heap memory
 */
void kheap_init() {
    uint8_t *base = (uint8_t *) KHEAP_BASE;
    size_t total = KHEAP_SIZE;

    heap_info.start = (vmm_addr_t *) base;
    heap_info.size = total;
    heap_info.used = sizeof(heap_header_t);
    heap_info.first_header = (heap_header_t *) base;
    heap_info.first_header->magic = HEAP_MAGIC;
    heap_info.first_header->size = total - sizeof(heap_header_t);
    heap_info.first_header->is_free = 1;
    heap_info.first_header->next = 0;
}

/* kheap_lock (spinlock.h) serialises the free list across CPUs. */

void *kmalloc(size_t len) {
    uint32_t f = spin_lock(&kheap_lock);
    void *p = first_free(len);
    spin_unlock(&kheap_lock, f);
    return p;
}

void kfree(void *ptr) {
    if(!ptr)
        return;

    heap_header_t *head = (heap_header_t *) ((uint8_t *) ptr - sizeof(heap_header_t));

    uint32_t f = spin_lock(&kheap_lock);
    if((head->magic != HEAP_MAGIC) || head->is_free) {
        spin_unlock(&kheap_lock, f);
        return;
    }

    head->is_free = 1;
    heap_info.used -= head->size;

    // Merge contiguous free sections that follow this one.
    heap_header_t *app = head->next;
    while((app != 0) && (app->magic == HEAP_MAGIC) && (app->is_free == 1)) {
        head->size += app->size + sizeof(heap_header_t);
        head->next = app->next;
        heap_info.used -= sizeof(heap_header_t);
        app = app->next;
    }
    spin_unlock(&kheap_lock, f);
}

void *first_free(size_t len) {
    heap_header_t *head = heap_info.first_header;

    len = (len + 3u) & ~((size_t) 3u);

    while(head != 0) {
        if(head->magic != HEAP_MAGIC) {
            printf("\nkmalloc: heap corruption\n");
            return 0;
        }
        if(head->is_free && head->size >= len) {
            /* Split only if the tail can still hold a header plus a little
             * payload; the old code used pointer arithmetic scaled by
             * sizeof(heap_header_t), so the split header landed far outside
             * the block and the free list marched off the end of the heap. */
            if(head->size >= len + sizeof(heap_header_t) + 4) {
                heap_header_t *split = (heap_header_t *)
                    ((uint8_t *) head + sizeof(heap_header_t) + len);
                split->magic = HEAP_MAGIC;
                split->size = head->size - len - sizeof(heap_header_t);
                split->is_free = 1;
                split->next = head->next;
                head->next = split;
                head->size = len;
                heap_info.used += sizeof(heap_header_t);
            }
            head->is_free = 0;
            heap_info.used += head->size;
            return (uint8_t *) head + sizeof(heap_header_t);
        }
        head = head->next;
    }
    printf("\nkmalloc: out of memory\n");
    return 0;
}

int get_heap_size() {
    return heap_info.size;
}

int get_used_heap() {
    return heap_info.used;
}

void print_header(heap_header_t *head) {
    printf("Size: %d Is free: %d Next: %u\n", head->size, head->is_free, (uint32_t)head->next);
}

