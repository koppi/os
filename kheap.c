/**
 * @file kheap.c
 * @brief Kernel heap — one first-fit free list spanning from the end of the
 *        kernel image to the top of the identity-mapped low 4 MiB.
 */
#include <kheap.h>
#include <mm.h>
#include <paging.h>
#include <printf.h>
#include <spinlock.h>

extern uint32_t kernel_start;
extern uint32_t kernel_end;

/*
 * The kernel heap sits between the page-table storage window (which starts just
 * past the kernel image — see paging_init) and the top of the identity-mapped
 * low 4 MB. pmm_init2() reserves 0..KERNEL_SPACE_END, so nothing else hands
 * these frames out.
 */
#define HEAP_END 0x400000

heap_info_t heap_info;

/**
 * Init the kernel heap memory
 */
void kheap_init() {
    uint8_t *base = (uint8_t *) paging_window_end();
    size_t total = HEAP_END - (uint32_t) base;

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

