/**
 * @file heap.c
 * @brief Per-process userspace heap (first-fit) backing the `malloc`/`free`
 *        system calls; each process gets a 4-page arena.
 */
#include <heap.h>
#include <mm.h>
#include <paging.h>
#include <sched.h>
#include <proc.h>
#include <printf.h>
#include <lib/string.h>
#include <spinlock.h>

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
void heap_init(vmm_addr_t *addr, size_t bytes) {
    heap_info_t *heap_info = (heap_info_t *) addr;
    uint8_t *base = (uint8_t *) addr + sizeof(heap_info_t);
    size_t total = bytes - sizeof(heap_info_t);

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
    return 0;   /* exhausted - umalloc_sys decides whether to grow and retry */
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

/** Ceiling on one process's heap arena. */
#define PROC_HEAP_MAX (64u * 1024u * 1024u)

/**
 * @brief Grow the current process's heap by enough (zeroed) pages to satisfy a
 *        @p need byte allocation, at least 16, and splice a free block onto the
 *        end of the block list.
 *
 * New pages are contiguous with the old arena, so the appended block coalesces
 * with the previous one when that was free. @return non-zero if it grew.
 */
static int heap_grow(thread_t *t, page_dir_t *pdir, size_t need) {
    heap_info_t *hi = (heap_info_t *) t->heap;

    size_t want = need + sizeof(heap_header_t) + PAGE_SIZE;
    size_t pages = (want + PAGE_SIZE - 1) / PAGE_SIZE;
    if(pages < 16)
        pages = 16;
    if((t->heap_limit - t->heap) + pages * PAGE_SIZE > PROC_HEAP_MAX)
        return 0;

    vmm_addr_t base = t->heap_limit;
    for(size_t i = 0; i < pages; i++) {
        vmm_addr_t va = base + i * PAGE_SIZE;
        if(!vmm_map(pdir, va, PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
            while(i-- > 0)
                vmm_unmap(pdir, base + i * PAGE_SIZE);
            return 0;
        }
        /* No TLB shootdown: `va` sits above the old heap limit and has never
         * been mapped in this address space, so no CPU can hold a stale
         * translation for it (x86 does not cache not-present entries). The
         * write below faults it straight in on this CPU. Broadcasting a
         * shootdown per page here was the bulk of the SMP TLB-IPI storm. */
        memset((void *) va, 0, PAGE_SIZE);
    }

    heap_header_t *last = hi->first_header;
    while(last->next)
        last = last->next;

    heap_header_t *nb = (heap_header_t *) base;   /* == old arena end */
    nb->magic = HEAP_MAGIC;
    nb->size = pages * PAGE_SIZE - sizeof(heap_header_t);
    nb->is_free = 1;
    nb->next = 0;
    last->next = nb;

    hi->size += pages * PAGE_SIZE;
    t->heap_limit = base + pages * PAGE_SIZE;

    if(last->is_free) {   /* coalesce with the old trailing free block */
        last->size += sizeof(heap_header_t) + nb->size;
        last->next = 0;
    }
    return 1;
}

/**
 * @brief Try to satisfy a realloc without moving the block.
 *
 * The block keeps its address and grows into the free blocks that follow it,
 * or gives back the tail when it shrinks. @return non-zero if it worked.
 *
 * This is not an optimisation. A caller that grows an array one element at a
 * time -- which the MIDI reader in apps/doom does, once per event -- turns
 * allocate-copy-free into a staircase of holes, each one element smaller
 * than the block that would fit in it, because @ref ufree only coalesces
 * forwards. Loading one Doom track that way asked for tens of megabytes and
 * hit @ref PROC_HEAP_MAX. Growing in place costs nothing and the pattern
 * becomes linear.
 *
 * Caller holds @c uheap_lock.
 */
static int urealloc_inplace(heap_info_t *hi, heap_header_t *h, size_t len) {
    len = heap_align(len);
    size_t was = h->size;

    if (len > h->size) {
        /* What would be there if the free blocks after this one were taken
         * over? Measure first: a partial absorb that then fails would leave
         * the list shorter for nothing. */
        size_t avail = h->size;
        int absorbed = 0;
        heap_header_t *n = h->next;

        while (n != 0 && n->magic == HEAP_MAGIC && n->is_free && avail < len) {
            avail += sizeof(heap_header_t) + n->size;
            absorbed++;
            n = n->next;
        }
        if (avail < len)
            return 0;

        h->size = avail;
        h->next = n;
        hi->used -= (size_t) absorbed * sizeof(heap_header_t);
    }

    /* Hand back the tail, if what is left of it could hold a block. */
    if (h->size >= len + sizeof(heap_header_t) + 4) {
        heap_header_t *tail = (heap_header_t *)
            ((uint8_t *) h + sizeof(heap_header_t) + len);
        tail->magic   = HEAP_MAGIC;
        tail->size    = h->size - len - sizeof(heap_header_t);
        tail->is_free = 1;
        tail->next    = h->next;
        h->next  = tail;
        h->size  = len;
        hi->used += sizeof(heap_header_t);
    }

    hi->used += h->size - was;
    return 1;
}

/*
 * A user process's heap free list is a shared mutable structure reached from
 * more than one kernel entry point: the process's own malloc/free/realloc
 * syscalls and vfs.c, which umalloc()s the `file` handle a fopen() returns into
 * that same heap. int 0x72 is a trap gate, so those paths are preemptible and,
 * on SMP, can be resumed on another CPU. uheap_lock serialises every user-heap
 * mutation (and, because spin_lock() clears IF, pins the thread for it).
 */
void *umalloc_locked(size_t len, struct thread *t, page_dir_t *pdir) {
    uint32_t f = spin_lock(&uheap_lock);
    void *p = umalloc(len, (vmm_addr_t *) t->heap);
    if(!p && heap_grow(t, pdir, len))
        p = umalloc(len, (vmm_addr_t *) t->heap);
    spin_unlock(&uheap_lock, f);
    return p;
}

void ufree_locked(void *ptr, struct thread *t) {
    uint32_t f = spin_lock(&uheap_lock);
    ufree(ptr, (vmm_addr_t *) t->heap);
    spin_unlock(&uheap_lock, f);
}

/**
 * @brief The process on whose behalf the running user-heap syscall executes.
 *
 * A syscall enters through a trap gate (interrupts stay on) and can be re-picked
 * on another CPU mid-flight; there is a narrow window in the SMP scheduler where
 * the per-CPU @c current_proc still points at a foreign (kernel/idle) thread,
 * whose heap is 0. Trusting that would make heap_grow() map pages over virtual
 * address 0, or hand back a pointer that is not mapped in the address space the
 * caller will return to.
 *
 * CR3 is the ground truth: the trap gate does not switch address spaces, so the
 * live page directory *is* the caller's. Take the fast path when @c current_proc
 * already agrees with it; otherwise recover the real caller from the run queue
 * by matching CR3. Give up (rather than corrupt memory) only if nothing matches.
 */
process_t *current_user_proc(void) {
    uint32_t cr3 = get_pdbr();

    process_t *cur = get_cur_proc();
    if (!(cur && cur->pdir && (uint32_t) (uintptr_t) cur->pdir == cr3))
        cur = proc_by_cr3(cr3);

    if (cur && cur->thread_list && cur->pdir != get_kern_directory() &&
        cur->thread_list->heap >= 0x400000)
        return cur;
    return 0;
}

void *umalloc_sys(size_t len) {
    process_t *cur = current_user_proc();
    if(!cur)
        return 0;
    return umalloc_locked(len, cur->thread_list, cur->pdir);
}

void ufree_sys(void *ptr) {
    process_t *cur = current_user_proc();
    if(cur)
        ufree_locked(ptr, cur->thread_list);
}

void *urealloc_sys(void *ptr, size_t nsize) {
    process_t *cur = current_user_proc();
    if(!cur)
        return 0;
    if(!ptr)
        return umalloc_sys(nsize);
    if(nsize == 0) {
        ufree_sys(ptr);
        return 0;
    }

    heap_header_t *h = (heap_header_t *) ((uint8_t *) ptr - sizeof(heap_header_t));
    if(h->magic != HEAP_MAGIC)
        return 0;

    uint32_t f = spin_lock(&uheap_lock);
    int grown = urealloc_inplace((heap_info_t *) cur->thread_list->heap, h, nsize);
    spin_unlock(&uheap_lock, f);
    if(grown)
        return ptr;

    size_t old = h->size;

    void *np = umalloc_sys(nsize);
    if(!np)
        return 0;
    memcpy(np, ptr, old < nsize ? old : nsize);
    ufree_sys(ptr);
    return np;
}
