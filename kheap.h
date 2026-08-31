/**
 * @file kheap.h
 * @brief Kernel heap — a first-fit free list between the kernel image and the
 *        top of the identity-mapped low memory.
 */
#pragma once

#include <heap.h>

/** @brief Set up the single kernel heap arena. */
void kheap_init();

/**
 * @brief Allocate @p len bytes from the kernel heap.
 * @return Pointer to the block, or NULL if out of memory / heap corrupt.
 */
void *kmalloc(size_t len);

/** @brief Release a block obtained from kmalloc() and coalesce free neighbours. */
void kfree(void *ptr);

/** @brief First-fit allocation core used by kmalloc(). */
void *first_free(size_t len);

/** @return Total heap size in bytes. */
int get_heap_size();
/** @return Bytes currently allocated (including block headers). */
int get_used_heap();

/** @brief Debug: print one heap block header. */
void print_header(heap_header_t *head);
