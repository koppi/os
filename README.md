# koppi's hobby OS

A small hobby operating system for the **i386** architecture: a 32-bit
multiboot kernel written in C with a handful of drivers, a FAT filesystem, a
cooperative process/thread model, a minimal C library and a few userspace
programs.

Current version: **0.0.0** (see [`ver.h`](ver.h)).

## Features

### Boot
* Boots via **GRUB** as a **Multiboot 1** *and* **Multiboot 2** image
  ([`boot.S`](boot.S), [`multiboot.c`](multiboot.c),
  [`multiboot2.c`](multiboot2.c)). The bootloader menu ([`grub.cfg`](grub.cfg))
  offers GUI (framebuffer) and console entries.
* Physical memory is discovered from the BIOS **e820** map / multiboot memory
  info.

### Memory management
* Physical memory manager (bitmap allocator) — [`mm.c`](mm.c)
* Paging / virtual memory manager — [`paging.c`](paging.c), [`vmm.c`](vmm.c)
* Kernel heap — [`kheap.c`](kheap.c), [`heap.c`](heap.c)

### CPU / interrupts
* GDT — [`gdt.c`](gdt.c) / [`gdt_asm.asm`](gdt_asm.asm)
* IDT + exception handlers — [`idt.c`](idt.c), [`exception.c`](exception.c)
* 8259 PIC remap — [`pic.c`](pic.c)
* TSS for ring-3 → ring-0 transitions — [`tss.c`](tss.c)
* x87 FPU init — [`fpu.c`](fpu.c)

### Scheduling & processes
* Cooperative scheduler — [`sched.c`](sched.c)
* Processes (flat binaries loaded from the filesystem) — [`proc.c`](proc.c)
* Threads — [`thread.c`](thread.c)
* `int 0x72` syscall gate — [`syscall.c`](syscall.c). Implemented calls:
  `printf`, `gets`/`scanf`, `fork`, `exit`, process return, `fopen`, `fclose`,
  `malloc`, `free`.

### Filesystems
* Virtual filesystem layer — [`vfs.c`](vfs.c)
* **FAT** (FAT12/16) driver — [`fat.c`](fat.c)
* ELF loading helpers — [`elf.c`](elf.c)

### Drivers
| Area | Files |
| --- | --- |
| ATA / IDE disk | [`ata.c`](ata.c), [`ata_asm.asm`](ata_asm.asm) |
| Floppy disk (+ DMA) | [`floppy.c`](floppy.c), [`dma.c`](dma.c) |
| PS/2 keyboard (IRQ-driven, ring-buffered) | [`keyboard.c`](keyboard.c), [`keyboard_asm.asm`](keyboard_asm.asm) |
| PS/2 mouse | [`mouse.c`](mouse.c), [`mouse_asm.asm`](mouse_asm.asm) |
| PCI bus | [`pci.c`](pci.c) |
| AC97 audio | [`pci_ac97.c`](pci_ac97.c), [`sound.c`](sound.c) |
| PC speaker | [`pcspk.c`](pcspk.c) |
| VGA / VBE framebuffer | [`vga.c`](vga.c), [`video.c`](video.c), [`graphics.c`](graphics.c) |
| PIT timer (1 kHz tick) | [`pit.c`](pit.c) |
| RTC / CMOS clock | [`rtc.c`](rtc.c) |
| Serial UART (kernel console / log) | [`uart.c`](uart.c) |

### Graphics / UI
* [`microui.c`](microui.c) immediate-mode UI + [`renderer.c`](renderer.c)
* [`ssfn.h`](ssfn.h) Scalable Screen Font renderer; the GNU Unifont
  ([`unifont.sfn`](unifont.sfn)) is linked into the kernel as `font.o`.
* BMP loader — [`bmp.c`](bmp.c) (e.g. [`mouse.bmp`](mouse.bmp) cursor)

### Audio
* [`hxcmod.c`](hxcmod.c) Amiga MOD player; sample module in
  [`mods/01.mod`](mods/01.mod).

### Support libraries (freestanding, in-tree)
* [`printf.c`](printf.c) (eyalroz/printf), [`ctype.c`](ctype.c),
  [`qsort.c`](qsort.c) / [`qsort_r.c`](qsort_r.c), [`rand.c`](rand.c),
  [`arith64.c`](arith64.c) (64-bit soft arithmetic),
  [`hash_string.c`](hash_string.c)
* Data structures: [`list.h`](list.h), [`rbtree.h`](rbtree.h),
  [`hashtable.h`](hashtable.h), [`queue.h`](queue.h), [`stack.h`](stack.h)

### Userspace
* Minimal C library in [`lib/`](lib) (`stdio`, `stdlib`, `string`, `unistd`,
  `system_calls`); headers in [`include/lib/`](include/lib). Programs are
  built position-dependent (`-fno-pic -fno-pie`): the `int 0x72` ABI passes the
  first argument in `%ebx`, which PIC code reserves for the GOT.
* Per-process user heap ([`heap.c`](heap.c)) backing the `malloc`/`free`
  syscalls; a first-fit free list over 4 pages.
* Example programs in [`apps/`](apps), each linked as a flat ring-3 binary with
  its own linker script and no crt0 (entry point is `main`):
  * [`apps/hello`](apps/hello) — prints a line via the `printf` syscall and
    returns
  * [`apps/01`](apps/01) — returns immediately (`tst` on the floppy)
  * [`apps/example`](apps/example) — interactive `scanf`/`malloc` demo

### Console shell
The kernel debug console ([`commands.c`](commands.c), `kmain_console`) runs as
the scheduler's first process. It reads keystrokes from the ring-buffered
keyboard driver, echoes them, supports backspace, and executes a line on Enter.

| Command | Effect |
| --- | --- |
| `help` | list commands |
| `mem` | physical memory, kernel heap and `cr0/cr2/cr3` |
| `ps` | process table |
| `ls` | list the working directory (device list at the root) |
| `cd [dir]` | change working directory; no argument resets to the root |
| `start <prog> [args]` | load an ELF, run it in ring 3, block until it exits, then reap it |
| `read <file>` | print a file |
| `beep` | play a tone through the AC97 codec |

Paths are resolved against the working directory. When none is set, a bare name
resolves against `/fda` (the first floppy), so `start hello` runs
`/fda/hello` and `read mouse.bmp` opens `/fda/mouse.bmp`. Devices are named
`fd{a,b}` for the floppies and `hd{a,b,…}` for IDE disks; `main_proc` probes
both at boot (`floppy_init()` / `ata_init()`) and any drive holding a FAT
volume is mounted — e.g. `cd hda` / `ls` / `read hda/file`.

## Layout

```
*.c *.S *.asm      kernel sources (flat, top-level)
*_asm.asm          NASM assembly stubs for the matching driver
include/           kernel headers (also include/lib for userspace)
lib/               minimal userspace C library
apps/              userspace programs
mods/              sample MOD music
iso/               staging dir for grub-mkrescue (generated)
kernel.lds         kernel linker script
grub.cfg           GRUB menu
floppy.sh hda.sh   scripts to (re)build the disk images
```

## Building and Running

### Prerequisites (Debian/Ubuntu)

```bash
sudo apt -y install grub-common xorriso mtools nasm gcc-multilib qemu-system-x86 grub-pc-bin
```

### Build

```bash
make
```

This builds the userspace library and apps, compiles the kernel to
`kernel.elf`, and (via the `iso` target) produces the bootable `os.iso`.

Toolchain: system `gcc -m32` / GNU `ld` (`-melf_i386`), NASM for `.asm` stubs.
Kernel flags: `-Og -std=gnu11 -ffreestanding -fno-builtin -nodefaultlibs
-fno-stack-protector -m32`, warnings as errors (`-Werror -Wall -Wextra`),
`-DDEBUG`.

### Disk images

The repo ships a prebuilt `floppy.img`; `hda.img` is built locally. Both hold a
FAT volume with the compiled apps copied in.

```bash
./hda.sh       # FAT hard-disk image via mtools (no root needed)
./floppy.sh    # FAT floppy image (uses sudo + a loop device)
```

The in-kernel FAT driver only handles one sector per cluster, so `hda.sh`
passes `mkfs.fat -s 1`.

### Run in QEMU

```bash
make qemu-iso     # boot os.iso (GUI, SDL), serial on stdio
make qemu-nox     # same, no display
make qemu-kernel  # boot kernel.elf directly with -kernel
```

QEMU is launched with 256 MB RAM, `-vga std`, the floppy + IDE hard disk +
CD-ROM images, AC97 / SB16 / PC-speaker audio, an `isa-debug-exit` device (the
kernel uses it to exit QEMU with a status code), and KVM acceleration.

Once the `>` prompt appears, try:

```
start hello
```

It loads `/fda/hello`, which prints `Hello from userspace!` and exits 0.

### Other targets

```bash
make kernel.lst   # full objdump disassembly
make cloc         # source line count (needs cloc)
make clean        # remove build artifacts
```

## Kernel startup

`kernel_main` ([`main.c`](main.c)) brings the system up in this order:

UART/log → parse multiboot → physical MM (e820) → VMM → kernel heap →
VGA or VBE → GDT → IDT → FPU → PIC → PIT (1 kHz) → VFS → floppy detect →
keyboard → mouse → UART RX IRQ → sound → syscalls → TSS → RTC → PCI probe →
scheduler.

`sched_init()` does not return: it `iret`s into the scheduler's first process
(`main_proc` in [`sched.c`](sched.c)), which brings up the floppy and IDE
block devices, mounts their FAT volumes, starts the framebuffer redraw thread,
and then runs the interactive console (`kmain_console`).
