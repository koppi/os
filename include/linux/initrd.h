/**
 * @file initrd.h
 * @brief The boot RAM disk: a FAT image handed to the kernel by the bootloader
 *        as a Multiboot module, relocated to high physical memory and exposed
 *        to the VFS as the block device @c rd.
 *
 * This is what makes @c os.iso self-contained — the root filesystem (the shell,
 * the apps, the @c cc toolchain) rides inside the ISO as @c /boot/initrd.img
 * instead of living on a separate @c hda.img / @c floppy.img. Writes to @c /rd
 * are RAM-backed and therefore lost on reboot.
 */
#pragma once

#include <linux/types.h>

/** Raw module extent as reported by the bootloader (0 if no module). */
extern uint32_t initrd_mod_start, initrd_mod_end;
/** Module extent after @ref initrd_relocate copies it to high RAM (0 if none). */
extern uint32_t initrd_phys_start, initrd_phys_end;

/**
 * The installer's boot payload: an optional second Multiboot module, the one whose
 * command line has the word @c payload (grub.cfg's "Install" entry). Raw extent as
 * the loader reported it, and after @ref initrd_relocate has put it directly behind
 * the RAM disk. Zero when there is none, which is every normal boot.
 */
extern uint32_t payload_mod_start, payload_mod_end;
extern uint32_t payload_phys_start, payload_phys_end;

/**
 * @brief Tell the module parsers which module is which.
 * @param start,end The module's extent.
 * @param cmdline   Its command line as the loader gave it (may be NULL): "path args".
 *
 * A module carrying the word @c payload is the installer's payload; the first other
 * module is the boot RAM disk. Called once per module, by the Multiboot 1 and 2 parsers.
 */
void initrd_note_module(uint32_t start, uint32_t end, const char *cmdline);

/**
 * @brief Copy the Multiboot module out of the bootloader's scratch area (which
 *        GRUB packs right behind the kernel, on top of where the heap and the
 *        ELF-load window will land) to a 4 MiB-aligned block in high memory.
 *
 * Must run with paging still off — before @ref vmm_init — so the whole of
 * physical RAM is addressable. A no-op when no module was supplied.
 */
void initrd_relocate(void);

/** @brief Identity-map the relocated module into the kernel directory. Called
 *         from @ref vmm_init after @ref map_kernel, before paging is enabled. */
void initrd_map(void);

/** @name Page-directory slot range covering the relocated module.
 *  @c create_address_space clones these into every process so a syscall can
 *  reach @c /rd on the caller's own address space. Both return -1 when there
 *  is no module. */
///@{
int initrd_pde_lo(void);
int initrd_pde_hi(void);
///@}

/** @brief Register the payload, if there is one, as a read-only raw block device (no
 *         filesystem is mounted on it; ring 3 reads it through block/blk.c). */
void payload_init(void);

/** @brief Register the relocated module as the @c rd block device and mount its
 *         FAT filesystem. Called from @c main_proc before @c ata_init. */
void ramdisk_init(void);
