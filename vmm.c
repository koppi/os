/**
 * @file vmm.c
 * @brief Virtual memory manager — page-directory/table maps and per-process
 *        address spaces, layered on the physical frame allocator (mm.c) and
 *        the page-table storage allocator (paging.c).
 *
 * SMP: every page-table mutation runs under @ref vmm_lock, and any change that
 * removes or replaces an existing mapping is followed by a cross-CPU
 * @ref tlb_shootdown. The "current" page directory is per-CPU (each CPU's
 * @ref cpu_t::current_dir), set by @ref change_page_directory alongside CR3.
 */
#include <io.h>
#include <memory.h>
#include <lib/string.h>
#include <printf.h>
#include <proc.h>
#include <percpu.h>
#include <apic.h>
#include <spinlock.h>
#include <initrd.h>
#include <bfb.h>

/*
 * |------------------------------------------------|
 * | 0x0 - 0x400000 -> identity mapped kernel space |
 * | kernel_end - 0x200000 -> kernel heap           |
 * | 0x200000 - 0x400000 -> paging structures       |
 * |------------------------------------------------|
 * | 0x400000 - 0x401000 -> common space            |
 * |------------------------------------------------|
 * | 0x401000 - 0x700000 -> free space              |
 * |------------------------------------------------|
 * | 0x700000 - 0x800000 -> elf loading space       |
 * |------------------------------------------------|
 * | 0x800000 - end -> programs address space       |
 * |------------------------------------------------|
 */

page_dir_t kern_dir[1024] __attribute__((aligned(4096)));

extern uint32_t kernel_start;
extern uint32_t kernel_end;

/* vmm_lock (spinlock.h) serialises every page-table mutation across CPUs. */

/**
 * Initializes the Virtual Memory Manager
 */
void vmm_init() {
    memset(kern_dir, 0, PAGEDIR_SIZE);
    /* Put the page-table storage window just past the kernel image, before
     * map_kernel() allocates the first table from it. */
    paging_init((uint32_t) &kernel_end);
    map_kernel(kern_dir);
    initrd_map();   /* identity-map the relocated boot RAM disk (kern_dir only) */
    change_page_directory(kern_dir);
    enable_paging();
}

/**
 * Maps the kernel in the given page directory
 */
void map_kernel(page_dir_t *pdir) {
    vmm_addr_t virt = 0x00000000;
    mm_addr_t phys = 0x0;

    // Identity map first 4MB
    for(int i = 0; i < 1024; i++, virt += PAGE_SIZE, phys += PAGE_SIZE) {
        if(pdir[virt >> 22] == 0) {
            if(!vmm_create_page_table(pdir, virt, PAGE_PRESENT | PAGE_RW)) {
                printf("Error creating page table");
                return;
            }
        }
        ((uint32_t *) (pdir[virt >> 22] & ~0xFFF))[virt << 10 >> 10 >> 12] = phys | PAGE_PRESENT | PAGE_RW;
    }
    // Space for RETURN_ADDR
    uint32_t ret_addr = (uint32_t) RETURN_ADDR;
    if(!vmm_create_page_table(pdir, ret_addr, PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
        printf("Error creating page table");
        return;
    }
    ((uint32_t *) (pdir[ret_addr >> 22] & ~0xFFF))[ret_addr << 10 >> 10 >> 12] = ret_addr | PAGE_PRESENT | PAGE_RW | PAGE_USER;
}

/**
 * Switches the calling CPU's page directory to the given one
 */
void change_page_directory(page_dir_t *p) {
    this_cpu()->current_dir = p;
    load_pdbr((mm_addr_t) p);
}

page_dir_t *get_page_directory() {
    return this_cpu()->current_dir;
}

page_dir_t *get_kern_directory() {
    return kern_dir;
}

/**
 * Creates a page table for the given virtual address (caller holds vmm_lock).
 */
int vmm_create_page_table(page_dir_t *pdir, vmm_addr_t virt, uint32_t flags) {
    void *pt = page_table_malloc();
    if(!pt)
        return 0;
    pdir[virt >> 22] = ((uint32_t) pt) | flags;
    return 1;
}

/** @return The current PTE for @p virt in @p pdir, or 0 if no page table. */
static uint32_t pte_of(page_dir_t *pdir, vmm_addr_t virt) {
    if(pdir[virt >> 22] == 0)
        return 0;
    return ((uint32_t *) (pdir[virt >> 22] & ~0xFFF))[virt << 10 >> 10 >> 12];
}

/**
 * Allocates a chunk of memory and maps it to the virtual address
 */
int vmm_map(page_dir_t *pdir, vmm_addr_t virt, uint32_t flags) {
    mm_addr_t phys = (mm_addr_t) pmm_malloc();
    if(!phys) {
        printf("VMM: Failed allocating memory %x\n", phys);
        return 0;
    }

    uint32_t lf = spin_lock(&vmm_lock);
    uint32_t old = pte_of(pdir, virt);

    if(!pdir[virt >> 22]) {
        if(!vmm_create_page_table(pdir, virt, flags)) {
            spin_unlock(&vmm_lock, lf);
            return 0;
        }
    } else {
        pdir[virt >> 22] |= (flags & (PAGE_PRESENT | PAGE_RW | PAGE_USER));
    }
    ((uint32_t *) (pdir[virt >> 22] & ~0xFFF))[virt << 10 >> 10 >> 12] = phys | flags;

    if(old & PAGE_PRESENT)
        tlb_shootdown(virt);
    spin_unlock(&vmm_lock, lf);
    return 1;
}

/**
 * Maps the physical address to the virtual one
 */
int vmm_map_phys(page_dir_t *pdir, vmm_addr_t virt, mm_addr_t phys, uint32_t flags) {
    uint32_t lf = spin_lock(&vmm_lock);
    uint32_t old = pte_of(pdir, virt);

    if(pdir[virt >> 22] == 0) {
        if(!vmm_create_page_table(pdir, virt, flags)) {
            spin_unlock(&vmm_lock, lf);
            return 0;
        }
    } else {
        pdir[virt >> 22] |= (flags & (PAGE_PRESENT | PAGE_RW | PAGE_USER));
    }
    ((uint32_t *) (pdir[virt >> 22] & ~0xFFF))[virt << 10 >> 10 >> 12] = phys | flags;

    if(old & PAGE_PRESENT)
        tlb_shootdown(virt);
    spin_unlock(&vmm_lock, lf);
    return 1;
}

/**
 * Gets the physical address from the given virtual address (lock-free read).
 */
void *get_phys_addr(page_dir_t *pdir, vmm_addr_t virt) {
    if(pdir[virt >> 22] == 0)
        return 0;
    return (void *) (((uint32_t *) (pdir[virt >> 22] & ~0xFFF))[virt << 10 >> 10 >> 12] >> 12 << 12);
}

/* Page-directory slots a driver has asked to be visible from every process's
 * ring-0 context (device MMIO touched on the caller's CR3 by a syscall path,
 * e.g. AHCI / xHCI from the VFS). */
#define VMM_SHARED_MAX 8
static struct { int lo, hi; } shared_pde[VMM_SHARED_MAX];
static int shared_pde_n;

/**
 * @brief Make [@p va, @p va+@p span) reachable from every address space.
 *        Call after mapping the range into kern_dir and before any user
 *        process that needs it is created.
 */
void vmm_share_kernel_range(uint32_t va, uint32_t span) {
    if (shared_pde_n >= VMM_SHARED_MAX || span == 0)
        return;
    shared_pde[shared_pde_n].lo = (int) (va >> 22);
    shared_pde[shared_pde_n].hi = (int) ((va + span - 1) >> 22);
    shared_pde_n++;
}

/** Directory slots the kernel needs reachable from a process's ring-0 context:
 *  0 = identity map (kernel code/data/stacks/heap/page-tables, all < 4 MiB),
 *  1 = RETURN_ADDR stub + kernel-thread stacks,
 *  LAPIC slot = the MMIO this_cpu() reads on every scheduler tick,
 *  initrd slots = the boot RAM disk, so a `run`/`open` syscall can read /rd
 *  on the calling process's own CR3 (initrd_pde_* return -1 when absent),
 *  framebuffer slots = so a panic while a user process is current can still
 *  paint the screen (bfb_pde_* return -1 when there is no framebuffer),
 *  shared_pde = driver MMIO registered via vmm_share_kernel_range(). */
static int is_kernel_slot(int i) {
    if (i == 0 || i == 1 || i == (int) ((uint32_t) 0xFEE00000 >> 22))
        return 1;
    if (i >= initrd_pde_lo() && i <= initrd_pde_hi())
        return 1;
    if (i >= bfb_pde_lo() && i <= bfb_pde_hi())
        return 1;
    for (int k = 0; k < shared_pde_n; k++)
        if (i >= shared_pde[k].lo && i <= shared_pde[k].hi)
            return 1;
    return 0;
}

/**
 * Creates a page directory to be used with a process.
 *
 * Only the handful of kernel directory slots a process's ring-0 code actually
 * touches are cloned (each with its own copy of the page table). Everything
 * else the process builds itself. This keeps page-table storage use to a few
 * blocks per process; the old "clone every present entry" grew the window into
 * the kernel's own .bss.
 */
page_dir_t *create_address_space() {
    uint32_t lf = spin_lock(&vmm_lock);
    page_dir_t *pdir = (page_dir_t *) page_table_malloc();
    if(!pdir) {
        spin_unlock(&vmm_lock, lf);
        return 0;
    }
    for(int i = 0; i < PAGEDIR_SIZE; i++) {
        if(!is_kernel_slot(i) || !(kern_dir[i] & PAGE_PRESENT))
            continue;
        if(!vmm_create_page_table(pdir, (vmm_addr_t) i << 22, kern_dir[i] & 0xFFF)) {
            spin_unlock(&vmm_lock, lf);
            return 0;
        }
        memcpy((void *) (pdir[i] & ~0xFFF), (void *) (kern_dir[i] & ~0xFFF), PAGE_SIZE);
    }
    spin_unlock(&vmm_lock, lf);
    return pdir;
}

/**
 * Frees every page table a process directory points at, then the directory.
 * Every table in a process directory is that process's own (kernel slots are
 * deep copies, not shared), so this is safe once no CPU still runs on @p pdir.
 */
void delete_address_space(page_dir_t *pdir) {
    uint32_t lf = spin_lock(&vmm_lock);
    for(int i = 0; i < PAGEDIR_SIZE; i++) {
        if(pdir[i] & PAGE_PRESENT) {
            page_table_free((void *) (pdir[i] & PAGE_FRAME_MASK));
            pdir[i] = 0;
        }
    }
    page_table_free(pdir);
    spin_unlock(&vmm_lock, lf);
    tlb_shootdown(0);
}

/**
 * Unmaps the page table and frees the memory block
 */
void vmm_unmap_page_table(page_dir_t *pdir, vmm_addr_t virt) {
    uint32_t lf = spin_lock(&vmm_lock);
    void *frame = (void *) (pdir[virt >> 22] & PAGE_FRAME_MASK);
    page_table_free(frame);
    pdir[virt >> 22] = 0;
    tlb_shootdown(0);
    spin_unlock(&vmm_lock, lf);
}

/**
 * Unmaps the physical address from the virtual and deallocates memory
 */
void vmm_unmap(page_dir_t *pdir, vmm_addr_t virt) {
    uint32_t lf = spin_lock(&vmm_lock);
    if(pdir[virt >> 22] != 0) {
        void *addr = get_phys_addr(pdir, virt);
        if(addr) {
            ((uint32_t *) (pdir[virt >> 22] & ~0xFFF))[virt << 10 >> 10 >> 12] = 0;
            tlb_shootdown(virt);
            pmm_free(addr);
        } else {
            printf("Error unmapping memory\n");
        }
    }
    spin_unlock(&vmm_lock, lf);
}

/**
 * Unmaps a physical address from the virtual
 */
void vmm_unmap_phys(page_dir_t *pdir, vmm_addr_t virt) {
    uint32_t lf = spin_lock(&vmm_lock);
    if(pdir[virt >> 22] != 0) {
        ((uint32_t *) (pdir[virt >> 22] & ~0xFFF))[virt << 10 >> 10 >> 12] = 0;
        tlb_shootdown(virt);
    }
    spin_unlock(&vmm_lock, lf);
}
