/**
 * @file mm.h
 * @brief Physical memory manager (a bitmap of 4 KiB frames) and the small
 *        assembly helpers for CR0/CR2/CR3/TLB.
 */
#pragma once

#include <types.h>

#define BLOCKS_PER_BYTE 8          /**< Frames tracked per bitmap byte. */
#define BLOCKS_LEN 4096            /**< Frame size in bytes. */
#define BYTE_SET 0xFFFFFFFF        /**< All-ones word (frame group fully used). */
#define BITMAP_LEN 0x8000          /**< Bitmap size in bytes (covers up to 4 GiB). */

/** Top of the identity-mapped low memory; also the pmm's reserved ceiling. */
#define KERNEL_SPACE_END 0x401000

typedef uint32_t mm_addr_t;   /**< A physical address. */
typedef uint32_t vmm_addr_t;  /**< A virtual address. */

/** Physical-memory-manager bookkeeping. */
typedef struct mem_info {
    uint32_t size;         /**< Free memory in KiB. */
    uint32_t used_blocks;  /**< Frames currently marked used. */
    uint32_t max_blocks;   /**< Total frames. */
    mm_addr_t *map;        /**< The frame bitmap. */
} mem_info_t;

/** A BIOS memory-map region (multiboot 1 layout). */
typedef struct memory_region {
    uint32_t size;
    uint32_t addr_low;
    uint32_t addr_high;
    uint32_t len_low;
    uint32_t len_high;
    uint32_t type;
} __attribute__((__packed__)) mem_region_t;

/** @brief Mark every frame used; @p mem_size is total memory in KiB. */
void pmm_init(uint32_t mem_size);
/** @brief Reserve 0..KERNEL_SPACE_END and finalise the free count. */
void pmm_init2();
/** @brief Mark frame @p bit used. */
void pmm_set_bit(int bit);
/** @brief Mark frame @p bit free. */
void pmm_unset_bit(int bit);
/** @brief Index of the first free frame, or -1. */
int pmm_first_free();
/** @brief Mark the frames in [addr, addr+size) as usable RAM. */
void pmm_init_reg(mm_addr_t addr, uint32_t size);
/** @brief Mark the frames in [addr, addr+size) as reserved. */
void pmm_deinit_reg(mm_addr_t addr, uint32_t size);
/** @brief Allocate one physical frame. @return frame address, or 0. */
void *pmm_malloc();
/** @brief Release the frame at @p frame. */
void pmm_free(mm_addr_t *frame);

mm_addr_t *get_mem_map();     /**< @return The frame bitmap. */
uint32_t get_mem_size();      /**< @return Free memory in KiB. */
uint32_t get_used_blocks();   /**< @return Frames in use. */
uint32_t get_max_blocks();    /**< @return Total frames. */

void enable_paging();               /**< Set CR0.PG (asm). */
void load_pdbr(mm_addr_t addr);     /**< Load CR3 with @p addr (asm). */
mm_addr_t get_pdbr();               /**< @return CR3 (asm). */
void flush_tlb(vmm_addr_t addr);    /**< @c invlpg for @p addr (asm). */
int get_cr0();                      /**< @return CR0 (asm). */
int get_cr2();                      /**< @return CR2, the last fault address (asm). */

