#include <types.h>
#include <stdlib.h>


static int compare_wrapper(const void* a, const void* b, void* arg)
{
	return ((int (*)(const void*, const void*)) (uintptr_t) arg)(a, b);
}

void qsort(void* base_ptr,
           size_t num_elements,
           size_t element_size,
           int (*compare)(const void*, const void*))
{
	qsort_r(base_ptr, num_elements, element_size, compare_wrapper,
	        (void *)(uintptr_t) compare);
}
