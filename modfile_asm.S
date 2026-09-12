/**
 * @file modfile_asm.S
 * @brief Embeds the sample MOD tune in the kernel image.
 *
 * @c .incbin drops mods/01.mod straight into .data and exports
 * @c modfile_0 / @c modfile_0_size, so the player in hxcmod.c has a song to
 * start without any filesystem being mounted yet.
 */

    .globl modfile_0, modfile_0_size

    .section .data

modfile_0:      .incbin "mods/01.mod"
modfile_0_size: .long modfile_0_size - modfile_0
