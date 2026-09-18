/**
 * @file main.c
 * @brief Kernel entry point and boot-time bring-up sequence.
 */
#include <ver.h>
#include <log.h>
#include <uart.h>
#include <kconsole.h>
#include <multiboot.h>
#include <multiboot2.h>
#include <io.h>
#include <memory.h>
#include <bfb.h>
#include <video.h>
#include <vga.h>
#include <gdt.h>
#include <idt.h>
#include <pic.h>
#include <pit.h>
#include <keyboard.h>
#include <mouse.h>
#include <syscall.h>
#include <tss.h>
#include <sched.h>
#include <rtc.h>
#include <vfs.h>
#include <fpu.h>
#include <sb16.h>
#include <pci.h>


#include <cmdline.h>
#include <pat.h>
#include <bootdiag.h>

#include <acpi.h>
#include <apic.h>
#include <percpu.h>
#include <initrd.h>


#include "cpu.h"

/** Total number of boot_progress() steps (one per subsystem init). */
#define BOOT_PROG_TOTAL 20

/** Increment the boot-progress counter and label the bar with @p msg. */
#define BOOT_PROG(msg) boot_progress(++boot_prog_cur, BOOT_PROG_TOTAL, (msg))

static unsigned boot_prog_cur;

/** The running OS version, printed at boot. Derived from git at build time. */
struct version_tuplet os_ver = {
#ifdef OS_VER_MAJ
    .maj = OS_VER_MAJ,
#else
    .maj = 0,
#endif
#ifdef OS_VER_MIN
    .min = OS_VER_MIN,
#else
    .min = 0,
#endif
#ifdef OS_VER_REV
    .rev = OS_VER_REV,
#else
    .rev = 0,
#endif
};

/** Total memory (KiB) reported by the multiboot2 basic-meminfo tag. */
extern uint32_t multiboot2_mem_size;

/* ------------------------------------------------------------------ *
 *  Early-boot progress beacon (bootdiag)                              *
 *                                                                    *
 *  A real ThinkPad has no serial port and the screen is dark until    *
 *  fbcon_init(), so when the kernel dies in between there is nothing   *
 *  to see. With `bootdiag` on the GRUB line this beeps the PC speaker  *
 *  N times at checkpoint N and, while paging is still off, floods the  *
 *  framebuffer with a per-checkpoint colour. The last thing you hear   *
 *  / see is the checkpoint the kernel reached; the next step is the    *
 *  one that hung.                                                      *
 * ------------------------------------------------------------------ */
static int bootdiag_on;

/* A tuning-free spin: `pause` retires in wildly different times across CPUs
 * (~5 cycles on Broadwell, ~140 on Kaby Lake), so a "unit" here is only
 * roughly a few ms. bootdiag multiplies it up into clearly human-scale beeps
 * and holds -- exact duration does not matter, only that they are countable. */
static void bd_spin(volatile uint32_t units) {
    while (units--)
        for (volatile int i = 0; i < 20000; i++)
            __asm__ volatile("pause");
}

static void bd_tone(int freq, uint32_t on_units, uint32_t off_units) {
    uint32_t c = 1193182u / (uint32_t) freq;
    outportb(0x43, 0xB6);
    outportb(0x42, c & 0xFF);
    outportb(0x42, (c >> 8) & 0xFF);
    outportb(0x61, inportb(0x61) | 3);
    bd_spin(on_units);
    outportb(0x61, inportb(0x61) & 0xFC);
    bd_spin(off_units);
}

/** @param stage 1..N checkpoint number. @param paging_off non-zero while the
 *         framebuffer is still identity-accessible (before vmm_init). */
static void bootdiag(int stage, int paging_off) {
    if (!bootdiag_on)
        return;

    /* N rising beeps == checkpoint N reached. */
    for (int i = 0; i < stage; i++)
        bd_tone(560 + stage * 120, 12, 6);

    /* While paging is off the framebuffer GRUB handed over is at its physical
     * address -- flood it so the checkpoint is visible too, and hold it long
     * enough to read. Once paging is on the fb is not mapped here; the beeps
     * carry the signal. */
    if (paging_off && bfb_addr && bfb_bpp >= 24) {
        static const uint32_t hue[] = {
            0x00440000, 0x00444400, 0x00004400, 0x00004444, 0x00000066,
            0x00440044, 0x00444444
        };
        uint32_t col   = hue[(stage - 1) % 7];
        uint32_t pitch = bfb_scanline ? bfb_scanline : bfb_width * 4;
        volatile uint8_t *fb = (volatile uint8_t *) (uintptr_t) bfb_addr;
        for (uint32_t y = 0; y < bfb_height; y++)
            for (uint32_t x = 0; x < bfb_width; x++)
                *(volatile uint32_t *) (fb + y * pitch + x * 4) = col;
        bd_spin(120);   /* ~1-2 s so the colour is readable */
    }
}

/** @brief Sub-step beacon: @p n low (400 Hz) beeps, a distinct dim colour
 *         flood, and a readable white label (see @ref bootdiag_text) -- all
 *         distinct from the rising (>=680 Hz) checkpoint markers / the green
 *         progress bar, to bisect a coarse step. The framebuffer is still
 *         identity-accessible under vmm_init (paging is off until
 *         @ref enable_paging), so a hang leaves the whole screen painted with
 *         the last sub-step: dark red (map_kernel) -> dark yellow (kernel
 *         heap) -> dark cyan (initrd) -> dark magenta (CR3 loaded). */
void bootdiag_sub(int n) {
    if (!bootdiag_on)
        return;
    for (int i = 0; i < n; i++)
        bd_tone(400, 6, 4);

    static const uint32_t hue[] = {
        0x00220000, 0x00222200, 0x00002222, 0x00220022
    };
    static const char *lab[] = {
        "vmm: kernel 4M identity + RETURN_ADDR mapped",
        "vmm: kernel heap window mapped",
        "vmm: initrd mapped",
        "vmm: CR3 loaded, before enable_paging",
    };
    if (n >= 1 && n <= 4 && bfb_addr && bfb_bpp >= 24) {
        const uint8_t bytespp = bfb_bpp >= 32 ? 4 : 3;
        uint32_t col   = hue[(n - 1) % 4];
        uint32_t pitch = bfb_scanline ? bfb_scanline : bfb_width * bytespp;
        volatile uint8_t *fb = (volatile uint8_t *) (uintptr_t) bfb_addr;
        for (uint32_t y = 0; y < bfb_height; y++)
            for (uint32_t x = 0; x < bfb_width; x++) {
                volatile uint8_t *p = fb + y * pitch + x * bytespp;
                if (bytespp == 4)
                    *(volatile uint32_t *) p = col;
                else {
                    p[0] = col & 0xFF;
                    p[1] = (col >> 8) & 0xFF;
                    p[2] = (col >> 16) & 0xFF;
                }
            }
        bootdiag_text(lab[n - 1]);
    }
}

/**
 * @brief Read the CMOS floppy-drive type byte and log both drives.
 */
static void floppy_detect() {
    outportb(0x70, 0x10);
    unsigned char c = inportb(0x71);
    static const char *drive_type[6] = {
        "no floppy drive", "360KB 5.25\"", "1.2MB 5.25\"",
        "720KB 3.5\"", "1.44MB 3.5\"", "2.88MB 3.5\""
    };
    klogf(LOG_INFO, "Floppy drive A: %s\n", drive_type[c >> 4]);
    klogf(LOG_INFO, "Floppy drive B: %s\n", drive_type[c & 0xF]);
}

/**
 * @brief C entry point, called from boot.S once a stack is set up.
 *
 * Parses the multiboot info, initialises physical memory and paging, brings up
 * every subsystem (console, GDT/IDT, PIC/PIT, drivers, VFS, syscalls, TSS,
 * RTC, PCI) and finally calls @ref sched_init, which does not return.
 *
 * @param magic Multiboot loader magic in EAX (identifies MB1 vs MB2).
 * @param addr  Physical address of the multiboot information structure.
 */
void kernel_main(unsigned long magic, unsigned long addr)
{
    unsigned size = *(unsigned*)addr;
    uint64_t tsc = rdtsc();

    uart_init();
    kconsole = &uartdev;

    klogf(LOG_INFO, "-- os %u.%u.%u --\n", os_ver.maj, os_ver.min, os_ver.rev);
    klogf(LOG_INFO, "  kernel ELF size = %u\n", size);

    uint32_t mem_kib = 0;
    if (magic == MULTIBOOT_LOADER_MAGIC)
    {
        klogf(LOG_INFO, "MultiBoot 1 addr: 0x%x magic: 0x%x size: 0x%x\n", (uintptr_t)addr, (unsigned)magic, size);
        multiboot_info_parse((const multiboot_info_t*)addr);
        multiboot_info_t* info = (multiboot_info_t*)addr;
        mem_kib = info->mem_upper + info->mem_lower;
    }
    else if (magic == MULTIBOOT2_LOADER_MAGIC)
    {
        klogf(LOG_INFO, "MultiBoot 2 addr: 0x%x magic: 0x%x size: 0x%x\n", (uintptr_t)addr, (unsigned)magic, size);
        multiboot2_info_parse((const multiboot2_info_t*)addr);
        mem_kib = multiboot2_mem_size;
    }
    else
    {
        klogf(LOG_EMERG, "Error: no multiboot, magic: 0x%lx. Exiting.", magic);
        exit_qemu(1);
    }

    if (kernel_cmdline[0])
        klogf(LOG_INFO, "  cmdline: %s\n", kernel_cmdline);

    bootdiag_on = cmdline_has("bootdiag");
    bootdiag(1, 1);   /* checkpoint 1: multiboot parsed, cmdline read */
    BOOT_PROG("multiboot");

    /* The loader's memory-size fields are unreliable (a UEFI GRUB reports a
     * token ~7 MiB); the E820 map is authoritative, so use the top of RAM it
     * reports whenever that is larger. */
    {
        uint64_t top = 0;
        for (int i = 0; i < e820counter; i++)
            if (e820table[i].type == 1) {
                uint64_t e = e820table[i].base_address + e820table[i].size;
                if (e > 0xFFFFF000ULL) e = 0xFFFFF000ULL;
                if (e > top) top = e;
            }
        if ((uint32_t)(top / 1024) > mem_kib)
            mem_kib = (uint32_t)(top / 1024);
    }
    pmm_init(mem_kib);

    /* Move the boot RAM disk out of the bootloader's scratch area (which GRUB
     * packs right behind the kernel) before pmm/vmm claim that memory. Must
     * run with paging still off. */
    initrd_relocate();
    BOOT_PROG("boot RAM disk");

    for (int i = 0; i < e820counter; i++)
    {
        struct e820memmap map = e820table[i];
        if (map.type != 1)
            continue;

        klogf(LOG_INFO, " BIOS-e820 [addr: 0x%x%x, size: 0x%x%x] %s\n",
              (unsigned)(map.base_address >> 32), (unsigned)(map.base_address & 0xffffffff),
              (unsigned)(map.size >> 32), (unsigned)(map.size & 0xffffffff),
              e820_type_to_string((unsigned)map.type));

        /* The 32-bit PMM only tracks the low 4 GiB: drop ranges that start
         * above it and clip one that crosses it, so no wrap-around frees the
         * wrong frames. */
        uint64_t base = map.base_address;
        uint64_t end  = base + map.size;
        if (base >= 0xFFFFF000ULL)
            continue;
        if (end > 0xFFFFF000ULL)
            end = 0xFFFFF000ULL;
        pmm_init_reg((uint32_t) base, (uint32_t) (end - base));
    }

    /* Keep the relocated RAM disk out of the frame allocator's reach. */
    if (initrd_phys_start)
        pmm_deinit_reg(initrd_phys_start, initrd_phys_end - initrd_phys_start);

    pmm_init2();
    /* Reserve the kernel-heap window so pmm_malloc() never hands out a frame
     * that vmm_init() identity-maps for the heap. */
    pmm_deinit_reg(KHEAP_BASE, KHEAP_SIZE);
    /* The kernel-thread stack window needs no reservation of its own: it sits
     * below KERNEL_SPACE_END, which pmm_init2() has already taken out of the
     * allocator's reach. */
    klogf(LOG_INFO, "pmm: mem_size=%u KiB  max_frames=%u  used=%u  free=%u KiB\n",
          (unsigned) multiboot2_mem_size, (unsigned) get_max_blocks(),
          (unsigned) get_used_blocks(),
          (unsigned) ((get_max_blocks() - get_used_blocks()) * 4));
    bootdiag(2, 1);   /* checkpoint 2: physical MM up, paging still off */
    BOOT_PROG("physical memory");
    vmm_init();
    boot_progress_paging_on();
    bootdiag(3, 0);   /* checkpoint 3: paging enabled (map_kernel survived) */
    BOOT_PROG("paging");
    kheap_init();
    BOOT_PROG("kernel heap");
    if (cmdline_has("nofb"))
        bfb_addr = 0;               /* force VGA text mode */
    pat_init();                     /* WC memory type -> a fast framebuffer */
    bootdiag(4, 0);   /* checkpoint 4: PAT done, about to touch the framebuffer */
    if (!bfb_addr) vga_init();
    else vbe_init();
    bootdiag(5, 0);   /* checkpoint 5: framebuffer init returned (fbcon should now paint) */
    BOOT_PROG("GDT");
    gdt_init();
    klogf(LOG_INFO, "DBG: after gdt_init\n");
    BOOT_PROG("interrupts");
    idt_init(0x8);
    klogf(LOG_INFO, "DBG: after idt_init\n");
    BOOT_PROG("FPU");
    fpu_init();
    klogf(LOG_INFO, "DBG: after fpu_init\n");
    BOOT_PROG("PIC");
    pic_init(0x20, 0x28);
    klogf(LOG_INFO, "DBG: after pic_init\n");
    BOOT_PROG("PIT timer");
    pit_init();
    klogf(LOG_INFO, "DBG: after pit_init\n");
    pit_start_counter(1000, PIT_COUNTER_0, PIT_MODE_SQUAREWAVEGEN);
    klogf(LOG_INFO, "DBG: after pit_start_counter\n");
    BOOT_PROG("VFS");
    vfs_init();
    klogf(LOG_INFO, "DBG: after vfs_init\n");
    floppy_detect();
    klogf(LOG_INFO, "DBG: after floppy_detect\n");
    BOOT_PROG("keyboard");
    keyboard_init();
    klogf(LOG_INFO, "DBG: after keyboard_init\n");
    BOOT_PROG("mouse");
    mouse_init();
    klogf(LOG_INFO, "DBG: after mouse_init\n");
    uart_rx_ir();
    klogf(LOG_INFO, "DBG: after uart_rx_ir\n");
    BOOT_PROG("sound");
    sound_init();
    klogf(LOG_INFO, "DBG: after sound_init\n");
    BOOT_PROG("syscalls");
    syscall_init();
    klogf(LOG_INFO, "DBG: after syscall_init\n");
    BOOT_PROG("TSS");
    install_tss();
    klogf(LOG_INFO, "DBG: after install_tss\n");
    BOOT_PROG("RTC");
    rtc_init();
    klogf(LOG_INFO, "DBG: after rtc_init\n");
    BOOT_PROG("PCI");
    pci_init();
    klogf(LOG_INFO, "DBG: after pci_init\n");

    /* SMP bring-up for the boot CPU: parse ACPI/MADT, register the BSP,
     * map + enable the local APIC, and calibrate its timer against the PIT.
     * Application processors are started by smp_init() in Stage 2. */
    /* SMP bring-up for the boot CPU: parse ACPI/MADT, register the BSP,
     * map + enable the local APIC, and calibrate its timer against the PIT.
     * Application processors are started by smp_init() in Stage 2. */
    acpi_init();
    smp_register_bsp(acpi_bsp_apicid());
    apic_init();
    BOOT_PROG("SMP");
    smp_init();

    klogf(LOG_INFO, "Initialization took: %llu\n", rdtsc() - tsc);

    BOOT_PROG("scheduler");
    // sched_init() does not return: it iret's into the scheduler's first
    // process (main_proc), which brings up the interactive console.
    sched_init();

    // ReSharper disable once CppDFAEndlessLoop
    while (1) halt();
}
