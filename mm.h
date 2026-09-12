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
#define BITMAP_LEN 0x8000          /**< Frame bitmap: uint32_t words (covers 4 GiB). */

/** Frames the static bitmap can describe (@ref BITMAP_LEN words * 32 bits).
 *  A real machine may report more RAM than fits the low 4 GiB a 32-bit kernel
 *  can address; @ref pmm_init clamps to this and ignores the rest. */
#define PMM_MAX_FRAMES (BITMAP_LEN * 32u)
/** Highest physical address the 32-bit PMM tracks (one past the last frame). */
#define PMM_PHYS_LIMIT 0xFFFFF000u

/** Top of the identity-mapped low memory; also the pmm's reserved ceiling. */
#define KERNEL_SPACE_END 0x800000

/**
 * The kernel heap. It used to be wedged between the page-table window and the
 * 4 MiB line, which left ~100 KiB that shrank every time the kernel grew (and
 * hit zero once the AHCI/xHCI drivers landed). It now lives in its own
 * identity-mapped window at 96 MiB: above the highest address a user process
 * reaches (image at 7 MiB + a 64 MiB PROC_HEAP_MAX ceiling), below the boot
 * RAM disk at 128 MiB. Reserved in the PMM and shared into every address space
 * (a syscall path does kmalloc on the caller's CR3).
 */
#define KHEAP_BASE 0x06000000u
#define KHEAP_SIZE 0x00400000u   /* 4 MiB */

/**
 * The window the kernel maps its own thread stacks into: the console thread at
 * @ref KERNEL_SPACE_END (sched.c) and one 0x4000 slot per kernel process
 * (proc.c). These are virtual addresses backed by whatever frame pmm_malloc()
 * happens to return, but they start exactly where the pmm's reserved ceiling
 * ends -- so they collide numerically with the first frames it hands out.
 *
 * A driver that allocates a frame and identity-maps it (VA == PA, as the xHCI
 * scratchpad array and the EHCI periodic list do) would otherwise repoint one
 * of these VAs at its own DMA buffer, zero it, and let the controller write
 * over a running thread's stack. Reserved in the PMM so that cannot happen.
 */
#define KSTACK_BASE KERNEL_SPACE_END
#define KSTACK_SIZE 0x00080000u   /* 512 KiB */

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

