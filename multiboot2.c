/**
 * @file multiboot2.c
 * @brief Multiboot 2 tag walker: extracts basic memory info, the E820 memory
 *        map and the framebuffer parameters handed over by GRUB.
 */
#include <types.h>
#include <stddef.h>
#include <multiboot.h>
#include <multiboot2.h>
#include <align.h>

#include <log.h>
#include <printf.h>

#include <bfb.h>
#include <initrd.h>

/** Multiboot 2 tags are padded to an 8-byte boundary. */
#define MULTIBOOT2_TAG_ALIGN  8

const char * tag_names[] = {
    "end",
    "cmdline",
    "boot_loader_name",
    "module",
    "basic_meminfo",
    "bootdev",
    "mmap",
    "vbe",
    "framebuffer",
    "elf_sections",
    "apm",
    "efi32",
    "efi64",
    "smbios",
    "acpi_old",
    "acpi_new",
    "network",
    "EFI_MMAP",
    "EFI_BS",
    "EFI32_IH",
    "EFI64_IH",
    "LOAD_BASE_ADDR"
};

static void multiboot2_cmdline(const multiboot2_cmdline_t *module) {
    (void)module;
	//multiboot_cmdline(module->string);
}

static void multiboot2_module(const multiboot2_module_t *module) {
    /* First module only — it is the boot RAM disk (grub.cfg / initrd.img). */
    if (!initrd_mod_start) {
        initrd_mod_start = module->start;
        initrd_mod_end   = module->end;
    }
}

static void multiboot2_memmap(uint32_t length, const multiboot2_memmap_t *memmap)
{
    multiboot2_memmap_entry_t *entry = (multiboot2_memmap_entry_t *)
        ((uintptr_t) memmap + sizeof(*memmap));
    uint32_t pos = offsetof(multiboot2_tag_t, memmap) + sizeof(*memmap);

    while ((pos < length) && (e820counter < MEMMAP_E820_MAX_RECORDS)) {
        e820table[e820counter].base_address = entry->base_address;
        e820table[e820counter].size = entry->size;
        e820table[e820counter].type = entry->type;

        /* Compute address of next entry. */
        entry = (multiboot2_memmap_entry_t *)
            ((uintptr_t) entry + memmap->entry_size);
        pos += memmap->entry_size;

        e820counter++;
    }
}

static void multiboot2_fbinfo(const multiboot2_fbinfo_t *fbinfo)
{
    if (fbinfo->visual == MULTIBOOT2_VISUAL_RGB) {
        bfb_addr = fbinfo->addr;
        bfb_width = fbinfo->width;
        bfb_height = fbinfo->height;
        bfb_bpp = fbinfo->bpp;
        bfb_scanline = fbinfo->scanline;
        
        bfb_red_pos = fbinfo->rgb.red_pos;
        bfb_red_size = fbinfo->rgb.red_size;
        
        bfb_green_pos = fbinfo->rgb.green_pos;
        bfb_green_size = fbinfo->rgb.green_size;
        
        bfb_blue_pos = fbinfo->rgb.blue_pos;
        bfb_blue_size = fbinfo->rgb.blue_size;
    }
}

uint32_t multiboot2_mem_size = 0;

void multiboot2_info_parse(const multiboot2_info_t *info) {
    (void)tag_names;
    
	const multiboot2_tag_t *tag = (const multiboot2_tag_t *)
	    ALIGN_UP((uintptr_t) info + sizeof(*info), MULTIBOOT2_TAG_ALIGN);

	while (tag->type != MULTIBOOT2_TAG_TERMINATOR) {
        /*klogf(LOG_INFO, "   tag: 0x%x name: %s \n",
          tag->type, tag_names[tag->type]);*/
		switch (tag->type) {
		case MULTIBOOT2_TAG_CMDLINE:
			multiboot2_cmdline(&tag->cmdline);
            //printf("%s\n", tag->cmdline.string);
			break;
		case MULTIBOOT2_TAG_MODULE:
			multiboot2_module(&tag->module);
            //printf("%x %x\n", tag->module.start, tag->module.end);
			break;
		case MULTIBOOT2_TAG_MEMMAP:
			multiboot2_memmap(tag->size, &tag->memmap);
            /*printf( "%d %d\n",
                    tag->memmap.entry_size,
                    tag->memmap.entry_version);*/
            break;
        case MULTIBOOT2_TAG_BASIC_MEMINFO:
            /*printf ("mem_lower = %uKB, mem_upper = %uKB\n",
                    ((struct multiboot_tag_basic_meminfo *) tag)->mem_lower,
                    ((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);*/
            multiboot2_mem_size =
                (uint32_t)(((struct multiboot_tag_basic_meminfo *) tag)->mem_lower) +
                (uint32_t)(((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);
            break;
        case MULTIBOOT2_TAG_FBINFO:
            multiboot2_fbinfo(&tag->fbinfo);
            break;
        default:
            //printf("\n");
            break;
		}

		tag = (const multiboot2_tag_t *)
		    ALIGN_UP((uintptr_t) tag + tag->size, MULTIBOOT2_TAG_ALIGN);
	}
}
