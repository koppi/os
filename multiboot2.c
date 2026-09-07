/**
 * @file multiboot2.c
 * @brief Multiboot 2 tag walker: extracts basic memory info, the E820 memory
 *        map and the framebuffer parameters handed over by GRUB.
 */
#include <types.h>
#include <stddef.h>

#include <multiboot2.h>
#include <align.h>

#include <log.h>


#include <bfb.h>
#include <initrd.h>
#include <cmdline.h>
#include <lib/string.h>

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

static void multiboot2_cmdline(const multiboot2_cmdline_t *cmd) {
    strncpy(kernel_cmdline, (char *) cmd->string, sizeof(kernel_cmdline) - 1);
    kernel_cmdline[sizeof(kernel_cmdline) - 1] = 0;
}

static void multiboot2_module(const multiboot2_module_t *module) {
    /* First module only — it is the boot RAM disk (grub.cfg / initrd.img). */
    if (!initrd_mod_start) {
        initrd_mod_start = module->start;
        initrd_mod_end   = module->end;
    }
}

/** Highest usable RAM address seen in the memory map (bytes), capped at 4 GiB.
 *  Used as the PMM size when no basic-meminfo tag is present, which is the
 *  normal case when GRUB boots us from UEFI firmware. */
uint32_t multiboot2_ram_top = 0;

static void multiboot2_memmap(uint32_t length, const multiboot2_memmap_t *memmap)
{
    multiboot2_memmap_entry_t *entry = (multiboot2_memmap_entry_t *)
        ((uintptr_t) memmap + sizeof(*memmap));
    uint32_t pos = offsetof(multiboot2_tag_t, memmap) + sizeof(*memmap);

    while ((pos < length) && (e820counter < MEMMAP_E820_MAX_RECORDS)) {
        e820table[e820counter].base_address = entry->base_address;
        e820table[e820counter].size = entry->size;
        e820table[e820counter].type = entry->type;

        if (entry->type == 1) {
            uint64_t top = entry->base_address + entry->size;
            if (top > 0xFFFFF000ULL)
                top = 0xFFFFF000ULL;
            if ((uint32_t) top > multiboot2_ram_top)
                multiboot2_ram_top = (uint32_t) top;
        }

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
        /* A UEFI GOP framebuffer can sit above 4 GiB, where a 32-bit kernel
         * cannot map it -- truncating the address would scribble into RAM and
         * leave a black panel. Refuse it instead (falls back to VGA text). */
        if (fbinfo->addr >> 32) {
            klogf(LOG_WARNING,
                  "mb2: framebuffer at 0x%x%x is above 4 GiB, ignoring\n",
                  (uint32_t)(fbinfo->addr >> 32), (uint32_t)fbinfo->addr);
            return;
        }
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

/** Physical address of the ACPI RSDP GRUB copied in for us, or 0. Under UEFI
 *  the RSDP is not in the legacy 0xE0000-0xFFFFF window, so this tag is the
 *  only way to find it. */
uint32_t multiboot2_acpi_rsdp = 0;

static void multiboot2_acpi(const multiboot2_tag_t *tag) {
    /* The RSDP copy follows the 8-byte tag header. Prefer the first (v1) or
     * v2 copy we see; acpi.c re-validates the checksum. */
    if (!multiboot2_acpi_rsdp)
        multiboot2_acpi_rsdp = (uint32_t) (uintptr_t) ((const uint8_t *) tag + 8);
}

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
        case MULTIBOOT2_TAG_ACPI_OLD:
        case MULTIBOOT2_TAG_ACPI_NEW:
            multiboot2_acpi(tag);
            break;
        default:
            //printf("\n");
            break;
		}

		tag = (const multiboot2_tag_t *)
		    ALIGN_UP((uintptr_t) tag + tag->size, MULTIBOOT2_TAG_ALIGN);
	}

    /* Trust the memory map over the basic-meminfo tag: GRUB books us via UEFI
     * with a token basic-meminfo (~7 MiB, since there is no BIOS INT 15h),
     * which would leave the PMM with almost no frames. The E820 map is
     * authoritative -- use the top of RAM it reports whenever that is larger. */
    uint32_t from_map = multiboot2_ram_top / 1024;
    if (from_map > multiboot2_mem_size)
        multiboot2_mem_size = from_map;
}
