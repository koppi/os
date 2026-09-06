/**
 * @file mm.c
 * @brief Physical memory manager: a bitmap of 4 KiB frames, plus the tiny
 *        assembly wrappers for the paging control registers.
 *
 * One bit per frame; 1 = used. The map is populated by marking everything used
 * (@ref pmm_init), freeing the RAM regions from the E820 map
 * (@ref pmm_init_reg) and finally reserving the low identity region
 * (@ref pmm_init2).
 */
#include <lib/string.h>
#include <mm.h>
#include <apic.h>
#include <spinlock.h>

/** Physical-memory-manager state. */
mem_info_t pmm;

/** 32 KiB static bitmap — enough to describe 4 GiB of 4 KiB frames. */
static uint32_t bitmap[BITMAP_LEN] __attribute__((aligned(BLOCKS_LEN)));

/**
 * @brief Reset the frame bitmap with every frame marked used.
 * @param mem_size Total physical memory in KiB.
 */
void pmm_init(uint32_t mem_size) {
    // Get the blocks number (mem_size is in KiB, one block per 4 KiB frame).
    uint32_t blocks = mem_size / 4;
    // Never describe more frames than the static bitmap can hold: real
    // machines report several GiB of RAM and the old code walked
    // pmm.map[max_blocks/32] straight off the end of .bss.
    if (blocks > PMM_MAX_FRAMES)
        blocks = PMM_MAX_FRAMES;
    pmm.max_blocks = pmm.used_blocks = blocks;
    // Set the address for the memory map
    pmm.map = bitmap;
    // Set every frame used. memset takes a byte count: the old code passed
    // BITMAP_LEN (the word count), clearing only the first quarter of the map
    // and leaving frames above 1 GiB reading as free .bss zeros.
    memset(pmm.map, BYTE_SET, sizeof(bitmap));
}

/**
 * @brief Reserve the identity-mapped low region and compute the free total.
 */
void pmm_init2(void) {
    pmm_deinit_reg(0x0, KERNEL_SPACE_END);
    pmm.size = (pmm.max_blocks - pmm.used_blocks) * 4;
}

/** @brief Mark frame @p bit used. */
void pmm_set_bit(int bit) {
    pmm.map[bit / 32] |= (1 << (bit % 32));
}

/** @brief Mark frame @p bit free. */
void pmm_unset_bit(int bit) {
    pmm.map[bit / 32] &= ~(1 << (bit % 32));
}

/**
 * @brief Find the lowest-numbered free frame.
 * @return Frame index, or -1 if none is free.
 */
int pmm_first_free() {
    uint32_t i;
    int j;

    for(i = 0; i < pmm.max_blocks / 32; i++) {
        if(pmm.map[i] != BYTE_SET) {
            for(j = 0; j < 32; j++) {
                if(!(pmm.map[i] & (1 << j))) {
                    return (i * 32) + j;
                }
            }
        }
    }
    return -1;
}

/**
 * @brief Mark the frames covering [@p addr, @p addr + @p size) as free RAM.
 * @param addr Region base address.
 * @param size Region size in bytes.
 */
void pmm_init_reg(mm_addr_t addr, uint32_t size) {
    uint32_t align = addr / BLOCKS_LEN;
    uint32_t blocks = size / BLOCKS_LEN;
    /* Clamp to what the bitmap can hold: a RAM range that runs past the 4 GiB
     * the 32-bit PMM tracks (common on real hardware) must not walk the map
     * off the end of .bss. */
    if (align >= pmm.max_blocks)
        return;
    if (blocks > pmm.max_blocks - align)
        blocks = pmm.max_blocks - align;
    for(uint32_t i = 0; i < blocks; i++) {
        if (pmm.map[align / 32] & (1u << (align % 32)))
            pmm.used_blocks--;
        pmm_unset_bit(align++);
    }
    pmm_set_bit(0);
}

/**
 * @brief Mark the frames covering [@p addr, @p addr + @p size) as reserved.
 * @param addr Region base address.
 * @param size Region size in bytes.
 */
void pmm_deinit_reg(mm_addr_t addr, uint32_t size) {
    uint32_t align = (addr == 0) ? 0 : addr / BLOCKS_LEN;
    uint32_t blocks = (size + BLOCKS_LEN - 1) / BLOCKS_LEN;
    if (align >= pmm.max_blocks)
        return;
    if (blocks > pmm.max_blocks - align)
        blocks = pmm.max_blocks - align;
    for(uint32_t i = 0; i < blocks; i++) {
        if (!(pmm.map[align / 32] & (1u << (align % 32))))
            pmm.used_blocks++;
        pmm_set_bit(align++);
    }
}

/* pmm_lock (spinlock.h) serialises the frame bitmap across CPUs. */

/**
 * @brief Allocate one physical frame.
 * @return Physical address of the frame, or 0 if none is free.
 */
void *pmm_malloc() {
    uint32_t f = spin_lock(&pmm_lock);
    int p = pmm_first_free();
    if(p <= 0) {
        spin_unlock(&pmm_lock, f);
        return 0;
    }
    pmm_set_bit(p);
    pmm.used_blocks++;
    spin_unlock(&pmm_lock, f);
    return (void *) (BLOCKS_LEN * p);
}

/**
 * @brief Release a frame. Frames below @ref KERNEL_SPACE_END are ignored so
 *        the identity region and the kernel heap stay reserved.
 * @param addr Physical frame address.
 */
void pmm_free(mm_addr_t *addr) {
    if((uint32_t) addr < KERNEL_SPACE_END)
        return;
    uint32_t f = spin_lock(&pmm_lock);
    pmm_unset_bit((uint32_t) addr / BLOCKS_LEN);
    pmm.used_blocks--;
    spin_unlock(&pmm_lock, f);
}

/** @return The frame bitmap. */
mm_addr_t *get_mem_map() {
    return pmm.map;
}

/** @return Free physical memory in KiB. */
uint32_t get_mem_size() {
    return pmm.size;
}

/** @return Number of frames currently marked used. */
uint32_t get_used_blocks() {
    return pmm.used_blocks;
}

/** @return Total number of frames. */
uint32_t get_max_blocks() {
    return pmm.max_blocks;
}

/** @brief Set CR0.PG to turn paging on. */
void enable_paging() {
    uint32_t reg;
    // Enable paging
    asm volatile("mov %%cr0, %0" : "=r" (reg));
    reg |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "r" (reg));
}

/**
 * @brief Load CR3 (the page-directory base register).
 * @param addr Physical address of a page directory.
 */
void load_pdbr(mm_addr_t addr) {
    asm volatile("mov %0, %%cr3" : : "r" (addr));
}

/** @return The current CR3 value. */
mm_addr_t get_pdbr() {
    mm_addr_t ret;
    asm volatile("mov %%cr3, %0" : "=r" (ret));
    return ret;
}

/**
 * @brief Invalidate one TLB entry on every CPU.
 *
 * On SMP a mapping change on one CPU leaves stale entries in the others' TLBs,
 * so this broadcasts a shootdown IPI and waits for the acks (see
 * @ref tlb_shootdown). Degrades to a local @c invlpg when only one CPU is up,
 * which is the state during early boot.
 */
void flush_tlb(vmm_addr_t addr) {
    tlb_shootdown((uint32_t) addr);
}

/** @return The current CR0 value. */
int get_cr0() {
    int ret;
    asm volatile("mov %%cr0, %0" : "=r" (ret));
    return ret;
}

/** @return CR2 — the linear address of the most recent page fault. */
int get_cr2() {
    int ret;
    asm volatile("mov %%cr2, %0" : "=r" (ret));
    return ret;
}
