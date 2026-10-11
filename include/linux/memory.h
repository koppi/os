/**
 * @file memory.h
 * @brief Umbrella header pulling in the whole memory-management stack: the
 *        kernel heap, the physical memory manager and the paging/VMM layer.
 */
#pragma once

#include <linux/kheap.h>
#include <linux/mm.h>
#include <asm/paging.h>
