/**
 * @file multiboot.c
 * @brief Multiboot 1 information parsing: memory sizes, the E820 map and
 *        module command-line helpers.
 */
#include <multiboot.h>
#include <bfb.h>
#include <initrd.h>
#include <cmdline.h>
#include <lib/string.h>


/** Extract command name from the multiboot module command line.
 *
 * @param buf      Destination buffer (will be always NULL-terminated).
 * @param size     Size of destination buffer (in bytes).
 * @param cmd_line Input string (the command line).
 *
 */
void multiboot_extract_command(char *buf, size_t size, const char *cmd_line) {
    (void)buf;
    (void)size;
    (void)cmd_line;
}

/** Extract arguments from the multiboot module command line.
 *
 * @param buf      Destination buffer (will be always NULL-terminated).
 * @param size     Size of destination buffer (in bytes).
 * @param cmd_line Input string (the command line).
 *
 */
void multiboot_extract_argument(char *buf, size_t size, const char *cmd_line) {
    (void)buf;
    (void)size;
    (void)cmd_line;
}

static void multiboot_modules(uint32_t count, multiboot_module_t *mods) {
    /* First module only — the boot RAM disk (grub.cfg / initrd.img). */
    if (count > 0 && !initrd_mod_start) {
        initrd_mod_start = mods[0].start;
        initrd_mod_end   = mods[0].end;
    }
}

static void multiboot_memmap(uint32_t length, multiboot_memmap_t *memmap)
{
	uint32_t pos = 0;

	while ((pos < length) && (e820counter < MEMMAP_E820_MAX_RECORDS)) {
		e820table[e820counter] = memmap->mm_info;

		/* Compute address of next structure. */
		uint32_t size = sizeof(memmap->size) + memmap->size;
		memmap = (multiboot_memmap_t *) ((uintptr_t) memmap + size);
		pos += size;

		e820counter++;
	}
}

/**
 * @brief Parse the Multiboot 1 information block: record the memory sizes and
 *        walk the E820 map into @ref e820table.
 * @param info Multiboot information structure passed by the loader.
 */
void multiboot_info_parse(const multiboot_info_t *info) {
	/* Command line. */
	if ((info->flags & MULTIBOOT_INFO_FLAGS_CMDLINE) != 0 && info->cmd_line) {
		strncpy(kernel_cmdline, (char *) MULTIBOOT_PTR(info->cmd_line),
		        sizeof(kernel_cmdline) - 1);
		kernel_cmdline[sizeof(kernel_cmdline) - 1] = 0;
	}

	/* Copy modules information. */
	if ((info->flags & MULTIBOOT_INFO_FLAGS_MODS) != 0)
		multiboot_modules(info->mods_count,
		    (multiboot_module_t *) MULTIBOOT_PTR(info->mods_addr));

	/* Copy memory map. */
	if ((info->flags & MULTIBOOT_INFO_FLAGS_MMAP) != 0)
		multiboot_memmap(info->mmap_length,
		    (multiboot_memmap_t *) MULTIBOOT_PTR(info->mmap_addr));

    /* Boot framebuffer -- only if the loader actually set one up and it is a
     * linear RGB mode. A missing FB flag (console boot) or an EGA-text "mode"
     * would otherwise feed garbage / 0xB8000 into vbe_init(). */
    if ((info->flags & MULTIBOOT_INFO_FLAGS_FB) != 0 &&
        info->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB &&
        info->framebuffer_addr != 0) {
        bfb_addr     = (uint32_t) info->framebuffer_addr;
        bfb_width    = info->framebuffer_width;
        bfb_height   = info->framebuffer_height;
        bfb_bpp      = info->framebuffer_bpp;
        bfb_scanline = info->framebuffer_pitch;
    }
}
