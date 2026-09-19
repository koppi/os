/**
 * @file paging.c
 * @brief Storage allocator for page tables and page directories.
 *
 * Page-table structures live in a fixed window that starts just past the end of
 * the kernel image (@ref paging_init) — it used to be a hard-coded 0x200000,
 * which overlapped the kernel's own @c .data/.bss once the linked-in font blob
 * pushed the image past 2 MiB, so @c page_table_malloc was quietly zeroing the
 * GDT / IDT / per-CPU tables. Tracked by a small bitmap (one bit per 4 KiB).
 */
#include <lib/string.h>
#include <memory.h>
#include <proc.h>
#include <spinlock.h>
#include <log.h>

/** Number of 4 KiB blocks in the storage window (256 KiB). */
#define MAX_BLOCKS 64

/** The whole storage window must stay inside map_kernel()'s identity map — a
 *  block above it would #PF the moment page_table_malloc() zeroed it. The 4 MiB
 *  map only ever made 4 of the 64 blocks usable, but boot needs 6 (map_kernel
 *  slots 0+1, the heap window, two initrd tables at 128 MiB and the framebuffer
 *  at 0xFE000000); the 5th allocation silently returned NULL and the initrd top
 *  half / framebuffer never got mapped — the #PF at the first access after
 *  paging came up triple-faulted the MBA while QEMU/OVMF happened to fit. */
#define IDMAP_LIMIT 0x800000u

/** 2 words = 64 bits, one per 4 KiB block in the storage window. */
static uint32_t bitmap[2];
/** Base of the page-table storage window (set by @ref paging_init). */
static uint32_t page_start;
/** Blocks actually safe to hand out (<= MAX_BLOCKS): those below IDMAP_LIMIT. */
static int usable_blocks = MAX_BLOCKS;
/** Count of blocks currently handed out (diagnostic only). */
static int used_blocks = 0;

/* pgtbl_lock (spinlock.h) serialises the storage-window bitmap across CPUs. */

/*
 * Ranges the window must not hand out, recorded before it has a position.
 *
 * The window sits immediately after the kernel image, and GRUB puts its
 * information structure wherever it likes in low memory -- so as the image
 * grows the two eventually meet. They did, and the first page table erased
 * the ACPI RSDP that multiboot2.c was pointing at: no MADT, and the machine
 * came up on one core instead of four, with nothing to show for it but a
 * warning line. The RSDP is copied out now, but the collision itself is
 * still there and the next field someone reads late would go the same way.
 *
 * Four is room for the multiboot 1 parser's four separate pieces (the
 * structure, the command line, the module list and the memory map); the
 * multiboot 2 structure declares its own total size and needs one.
 */
#define RESERVE_MAX 4
static struct { uint32_t start, end; } reserved[RESERVE_MAX];
static int reserved_n;

void paging_reserve_range(uint32_t start, uint32_t end) {
    if (end <= start)
        return;
    if (reserved_n >= RESERVE_MAX) {
        klogf(LOG_WARNING, "paging: no room to reserve 0x%x..0x%x\n", start, end);
        return;
    }
    reserved[reserved_n].start = start;
    reserved[reserved_n].end   = end;
    reserved_n++;
}

/** @brief Mark the window blocks covering [@p start, @p end) used.
 *         @return How many blocks that took. */
static int reserve_blocks(uint32_t start, uint32_t end) {
    if (usable_blocks <= 0)
        return 0;                       /* no window to protect */

    uint32_t limit = page_start + (uint32_t) usable_blocks * BLOCKS_LEN;
    if (end <= page_start || start >= limit)
        return 0;                       /* nowhere near the window */

    uint32_t first = start > page_start ? (start - page_start) / BLOCKS_LEN : 0;
    uint32_t last  = (end - 1 - page_start) / BLOCKS_LEN;
    if (last >= (uint32_t) usable_blocks)
        last = (uint32_t) usable_blocks - 1;

    int taken = 0;
    for (uint32_t b = first; b <= last; b++) {
        if (bitmap[b / 32] & (1u << (b % 32)))
            continue;                   /* already spoken for */
        paging_set_bit((int) b);
        used_blocks++;
        taken++;
    }
    return taken;
}

/**
 * @brief Place the page-table storage window at @p start (page-aligned up).
 * @return The first byte past the window (where the kernel heap begins).
 */
uint32_t paging_init(uint32_t start) {
    page_start = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1u);
    memset(bitmap, 0, sizeof(bitmap));
    used_blocks = 0;

    usable_blocks = MAX_BLOCKS;
    if (page_start < IDMAP_LIMIT) {
        uint32_t fit = (IDMAP_LIMIT - page_start) / BLOCKS_LEN;
        if (fit < (uint32_t) usable_blocks)
            usable_blocks = (int) fit;
    } else {
        usable_blocks = 0;   /* window past the identity map -- unusable */
    }

    /* Reserve RETURN_ADDR's own frame (block 4 here) so the storage window
     * never hands out the trampoline page the bootstrap copied into place. */
    uint32_t ret_blk = ((uint32_t) RETURN_ADDR - page_start) / BLOCKS_LEN;
    if(ret_blk < (uint32_t) usable_blocks) {
        paging_set_bit((int) ret_blk);
        used_blocks++;
    }
    for (int i = 0; i < reserved_n; i++) {
        int taken = reserve_blocks(reserved[i].start, reserved[i].end);
        if (taken)
            klogf(LOG_INFO, "paging: 0x%x..0x%x is inside the window, "
                            "holding %d block(s) back\n",
                  reserved[i].start, reserved[i].end, taken);
    }

    klogf(LOG_INFO, "paging: page_start=0x%x usable=%d used=%d\n",
          page_start, usable_blocks, used_blocks);

    /* Boot needs somewhere north of a dozen blocks (the kernel map, the heap
     * window, the initrd's two tables at 128 MiB, the framebuffer). Running
     * out is not survivable and not obvious from the far side, so say so
     * here rather than at the page fault. */
    if (usable_blocks - used_blocks < 16)
        klogf(LOG_WARNING, "paging: only %d storage block(s) left\n",
              usable_blocks - used_blocks);

    return page_start + (uint32_t) MAX_BLOCKS * BLOCKS_LEN;
}

/** @return The first byte past the storage window (kernel-heap base). */
uint32_t paging_window_end(void) {
    return page_start + (uint32_t) MAX_BLOCKS * BLOCKS_LEN;
}

/**
 * @brief Allocate and zero one 4 KiB block for a page table or directory.
 * @return Block address in the storage window, or 0 if the window is full.
 */
void *page_table_malloc() {
    uint32_t f = spin_lock(&pgtbl_lock);
    int p = paging_first_free();
    if(p == -1) {
        spin_unlock(&pgtbl_lock, f);
        klogf(LOG_ERR, "page_table_malloc: OUT OF BLOCKS used=%d usable=%d\n", used_blocks, usable_blocks);
        return 0;
    }
    paging_set_bit(p);
    used_blocks++;
    spin_unlock(&pgtbl_lock, f);
    void *addr = (void *) ((BLOCKS_LEN * p) + page_start);
    memset(addr, 0, PAGE_SIZE);
    return addr;
}

/** @brief Mark storage block @p bit used. */
void paging_set_bit(int bit) {
    bitmap[bit / 32] |= (1u << (bit % 32));
}

/** @brief Mark storage block @p bit free. */
void paging_unset_bit(int bit) {
    bitmap[bit / 32] &= ~(1u << (bit % 32));
}

/**
 * @brief Find the first free storage block.
 * @return Block index, or -1 if the window is full.
 */
int paging_first_free() {
    for(int i = 0; i < MAX_BLOCKS / 32; i++) {
        if(bitmap[i] != BYTE_SET) {
            for(int j = 0; j < 32; j++) {
                int blk = (i * 32) + j;
                if(blk >= usable_blocks)
                    return -1;   /* rest of the window is outside the identity map */
                if(!(bitmap[i] & (1u << j)))
                    return blk;
            }
        }
    }
    return -1;
}

/**
 * @brief Release a page-table storage block.
 * @param addr Address previously returned by @ref page_table_malloc.
 *
 * Ignores an address outside the window (e.g. a stale 0 from an already-cleared
 * directory slot).
 */
void page_table_free(void *addr) {
    uint32_t a = (uint32_t) addr;
    if(a < page_start || a >= page_start + (uint32_t) MAX_BLOCKS * BLOCKS_LEN)
        return;
    uint32_t f = spin_lock(&pgtbl_lock);
    paging_unset_bit((int) ((a - page_start) / BLOCKS_LEN));
    used_blocks--;
    spin_unlock(&pgtbl_lock, f);
}

/** @return The storage-window allocation bitmap. */
uint32_t *get_page_table_bitmap() {
    return bitmap;
}
