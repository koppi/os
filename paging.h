/**
 * @file paging.h
 * @brief Virtual memory manager: page-directory/table maps, per-process
 *        address spaces and the small allocator for page-table storage.
 */
#pragma once

#include <mm.h>
#include <types.h>

#define PAGE_SIZE           4096  /**< Page size in bytes. */

#define PAGE_PRESENT        0x1        /**< PTE/PDE present bit. */
#define PAGE_RW             0x2        /**< Writable. */
#define PAGE_USER           0x4        /**< Ring-3 accessible. */
#define PAGE_PWT            0x8        /**< Write-through (page-level). */
#define PAGE_PCD            0x10       /**< Cache-disable (page-level) — for MMIO. */
#define PAGE_ACCESSED       0x20       /**< Accessed. */
#define PAGE_PAT            0x80       /**< PTE PAT bit — selects IA32_PAT slot 4..7. */
#define PAGE_WC             PAGE_PAT   /**< Write-combining (once pat_init has run). */
#define PAGE_FRAME_MASK     0x7FFFF000 /**< Frame-address field of a PTE/PDE. */

typedef uint32_t page_dir_t; /**< A page-directory (or page-table) entry. */

#define PAGEDIR_SIZE        1024  /**< Entries per page directory / table. */
#define PTE_IDX(virt)       (((virt) >> 12) & 0x3FF)

/** @brief Build the kernel directory, enable paging, switch to it. */
void vmm_init();

/** @brief Map the identity region and the RETURN_ADDR page into @p pdir. */
void map_kernel(page_dir_t *pdir);

/** @brief Load @p p into CR3 and record it as the current directory. */
void change_page_directory(page_dir_t *p);
/** @return The currently active page directory. */
page_dir_t *get_page_directory();
/** @return The kernel page directory. */
page_dir_t *get_kern_directory();

/** @brief Allocate a page table for @p virt in @p pdir with @p flags. */
int vmm_create_page_table(page_dir_t *pdir, vmm_addr_t virt, uint32_t flags);
/** @brief Map @p virt to a freshly allocated frame with @p flags. */
int vmm_map(page_dir_t *pdir, vmm_addr_t virt, uint32_t flags);
/** @brief Map @p virt to the given physical frame @p phys with @p flags. */
int vmm_map_phys(page_dir_t *pdir, vmm_addr_t virt, mm_addr_t phys, uint32_t flags);
/** @return The physical frame @p virt maps to in @p pdir, or 0. */
void *get_phys_addr(page_dir_t *pdir, vmm_addr_t virt);
/** @brief Make [virt, virt+span) visible from every process address space
 *         (device MMIO a syscall path touches on the caller's CR3). Call
 *         after mapping it into kern_dir, before the first process starts. */
void vmm_share_kernel_range(uint32_t virt, uint32_t span);
/** @brief Clone the kernel mappings into a new directory for a process. */
page_dir_t *create_address_space();
/** @brief Free the page-table storage of a process directory. */
void delete_address_space(page_dir_t *pdir);
/** @brief Free a page table and clear its directory entry. */
void vmm_unmap_page_table(page_dir_t *pdir, vmm_addr_t virt);
/** @brief Unmap @p virt and release the backing frame. */
void vmm_unmap(page_dir_t *pdir, vmm_addr_t virt);
/** @brief Clear the PTE for @p virt without freeing the frame. */
void vmm_unmap_phys(page_dir_t *pdir, vmm_addr_t virt);

/**
 * @brief Keep [@p start, @p end) out of the page-table storage window.
 *
 * For memory that has to survive boot but is not part of the kernel image,
 * so the window can land on it: the loader's information structure, which
 * GRUB places wherever it likes in low memory. Call before @ref paging_init
 * — the ranges are held until then and applied once the window's position is
 * known. A range outside the window costs nothing.
 */
void paging_reserve_range(uint32_t start, uint32_t end);

/**
 * @brief Position the page-table storage window at @p start (rounded up to a
 *        page). Call once from @ref vmm_init with the end of the kernel image.
 * @return The first byte past the window — where the kernel heap starts.
 */
uint32_t paging_init(uint32_t start);
/** @return The first byte past the page-table window (kernel-heap base). */
uint32_t paging_window_end(void);

/** @brief Allocate a zeroed 4 KiB block for a page table. */
void *page_table_malloc();
void paging_set_bit(int bit);        /**< Mark page-table block @p bit used. */
void paging_unset_bit(int bit);      /**< Mark page-table block @p bit free. */
int paging_first_free();             /**< First free page-table block, or -1. */
void page_table_free(void *addr);    /**< Release a page-table block. */
uint32_t *get_page_table_bitmap();   /**< @return The page-table allocation bitmap. */

