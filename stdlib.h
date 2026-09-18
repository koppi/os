/**
 * @file stdlib.h
 * @brief Declarations for the in-tree @c qsort / @c qsort_r.
 */
#pragma once

#include <types.h>

/** @brief Sort @c nmemb elements of @c size bytes with comparator @c cmp. */
void qsort(void*, size_t, size_t, int (*)(const void*, const void*));
/** @brief Like qsort() but the comparator takes an extra context pointer. */
void qsort_r(void*, size_t, size_t, int (*)(const void*, const void*, void*), void*);

