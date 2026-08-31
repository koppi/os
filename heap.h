/**
 * @file heap.h
 * @brief Shared first-fit heap used by both the kernel heap (kheap.c) and the
 *        per-process userspace heaps (heap.c).
 */
#pragma once

/** Sanity marker stamped into every block header. */
#define HEAP_MAGIC      0xA0B0C0

#include <types.h>
#include <paging.h>

/** Header prepended to every heap block; blocks form a singly-linked list. */
typedef struct heap_header {
    int magic;                  /**< @ref HEAP_MAGIC. */
    size_t size;                /**< Payload size in bytes. */
    int is_free;                /**< Non-zero if the block is available. */
    struct heap_header *next;   /**< Next block, or NULL. */
} heap_header_t;

/** Per-arena bookkeeping, stored at the base of the arena. */
typedef struct {
    size_t size;                    /**< Total arena size. */
    vmm_addr_t *start;              /**< Arena base (== first_header). */
    size_t used;                    /**< Bytes handed out plus headers. */
    heap_header_t *first_header;    /**< Head of the block list. */
} heap_info_t;

/** @brief Initialise a per-process heap over the 4 pages at @p addr. */
void heap_init(vmm_addr_t *addr);
/** @brief Allocate @p len bytes from the process heap at @p heap. */
void *umalloc(size_t len, vmm_addr_t *heap);
/** @brief Free a block from the process heap at @p heap. */
void ufree(void *ptr, vmm_addr_t *heap);

/** @brief `malloc` syscall: allocate from the current process's heap. */
void *umalloc_sys(size_t len);
/** @brief `free` syscall: release into the current process's heap. */
void ufree_sys(void *ptr);
