/**
 * @file paging.c
 * @brief Storage allocator for page tables and page directories.
 *
 * Page-table structures live in a fixed 2 MiB window starting at
 * @ref PAGE_START, tracked by a small bitmap (one bit per 4 KiB block).
 */
#include <lib/string.h>
#include <memory.h>

#define MAX_BLOCKS 512            /**< Number of 4 KiB blocks in the window. */
#define PAGE_START 0x200000       /**< Base of the page-table storage window. */

/** 16 words = 512 bits, one per 4 KiB block in the storage window. */
static uint32_t bitmap[0x10];
/** Count of blocks currently handed out (diagnostic only). */
static int used_blocks = 0;

/**
 * @brief Allocate and zero one 4 KiB block for a page table or directory.
 * @return Block address in the storage window, or 0 if the window is full.
 */
void *page_table_malloc() {
    int p = paging_first_free();
    if(p == -1)
        return 0;
    paging_set_bit(p);
    used_blocks++;
    void *addr = (void *) ((BLOCKS_LEN * p) + PAGE_START);
    memset(addr, 0, PAGE_SIZE);
    return addr;
}

/** @brief Mark storage block @p bit used. */
void paging_set_bit(int bit) {
    bitmap[bit / 32] |= (1 << (bit % 32));
}

/** @brief Mark storage block @p bit free. */
void paging_unset_bit(int bit) {
    bitmap[bit / 32] &= ~(1 << (bit % 32));
}

/**
 * @brief Find the first free storage block.
 * @return Block index, or -1 if the window is full.
 */
int paging_first_free() {
    uint32_t i;
    int j;
    
    for(i = 0; i < MAX_BLOCKS / 32; i++) {
        if(bitmap[i] != BYTE_SET) {
            for(j = 0; j < 32; j++) {
                if(!(bitmap[i] & (1 << j))) {
                    return (i * 32) + j;
                }
            }
        }
    }
    return -1;
}

/**
 * @brief Release a page-table storage block.
 * @param addr Address previously returned by @ref page_table_malloc.
 */
void page_table_free(void *addr) {
    paging_unset_bit(((uint32_t) addr / BLOCKS_LEN) - PAGE_START);
    used_blocks--;
}

/** @return The storage-window allocation bitmap. */
uint32_t *get_page_table_bitmap() {
    return bitmap;
}
