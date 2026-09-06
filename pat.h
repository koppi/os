/**
 * @file pat.h
 * @brief Page Attribute Table: a write-combining memory type for the
 *        linear framebuffer.
 *
 * Map a page with @ref PAGE_WC (once @ref pat_init has run) to get
 * write-combining instead of the strong-uncacheable @c PAGE_PCD|PAGE_PWT.
 * WC lets the CPU batch a cache line of pixels into one burst write, which
 * is the difference between a smooth desktop and a crawling one on real
 * hardware where the framebuffer lives across the display link.
 */
#pragma once

/**
 * @brief Reprogram this CPU's IA32_PAT so slot 4 (@ref PAGE_WC) is
 *        write-combining. Idempotent. Must run on every CPU: the BSP before
 *        @ref vbe_init maps the framebuffer, each AP at the top of @c ap_main.
 */
void pat_init(void);

/** @return non-zero once @ref pat_init has installed the WC slot (CPU has PAT). */
int pat_available(void);
