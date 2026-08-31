# koppi's hobby OS

A small hobby operating system for the **i386** architecture: a 32-bit
multiboot kernel written in C with a handful of drivers, a FAT filesystem, a
preemptive priority-scheduled process/thread model, a minimal C library and a
few userspace programs.

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
* Preemptive fixed-priority round-robin scheduler with real-time policies
  (`SCHED_OTHER` / `SCHED_RR` / `SCHED_FIFO`) — [`sched.c`](sched.c). Timer-IRQ
  driven: each tick it runs the highest-priority ready thread, round-robins
  equal priorities by quantum, and lets a higher priority preempt within a tick.
* Processes (flat binaries loaded from the filesystem) — [`proc.c`](proc.c)
* Threads — [`thread.c`](thread.c)
* `int 0x72` syscall gate — [`syscall.c`](syscall.c). Implemented calls:
  `printf`, `gets`/`scanf`, `fork`, `exit`, process return, `fopen`, `fclose`,
  `malloc`, `free`.

### Filesystems
* Virtual filesystem layer — [`vfs.c`](vfs.c)
* **FAT** (FAT12/16) driver — [`fat.c`](fat.c). Reads the layout from the BPB
  (reserved sectors, FAT count/size, root-dir size), so it is not tied to
  1.44M floppy geometry — but it still assumes one sector per cluster.
* ELF loading helpers — [`elf.c`](elf.c)

### USB
A small USB 1.1 stack, driven from a kernel thread (no USB interrupts):

* **UHCI** host-controller driver — [`uhci.c`](uhci.c). Found on the PCI bus,
  reset and started; its 1024-entry frame list, queue heads, transfer
  descriptors and data buffers are static and live in the identity-mapped low
  memory so `&x == phys(x)`.
* **USB core** — [`usb.c`](usb.c). Synchronous EP0 control transfers, port
  reset, and single-device-per-port enumeration (device descriptor →
  `SET_ADDRESS` → configuration → `SET_CONFIGURATION`). The same routine
  enumerates devices on a root port and behind a hub; a device's USB address is
  its slot index, so an address is reused once the device is unplugged.
* **Hub driver** — [`usb_hub.c`](usb_hub.c). A device that enumerates as class 9
  is registered here: the driver reads the hub descriptor, powers every
  downstream port and recursively enumerates whatever is attached (up to three
  hubs deep). On each USB service pass it re-reads every downstream port's
  status on a slow cadence, so a device attached or removed while the system is
  running is enumerated or torn down on the fly — **hot-plug behind a hub
  works**. Detection is GET_STATUS polling; the hub's status-change interrupt
  endpoint is not used.
* **HID boot driver** — [`usb_hid.c`](usb_hid.c). Forces the HID *boot*
  protocol (no report-descriptor parsing): a keyboard's fixed 8-byte report is
  translated from HID usage codes to ASCII and pushed into the same ring buffer
  the console reads, and a mouse's `[buttons, dx, dy]` report updates the shared
  pointer state — so real USB keyboards and mice work alongside the PS/2 ones.
  A HID device unplugged from a hub releases its interrupt endpoint.

Devices may hang off either UHCI root port or a hub plugged into one; the QEMU
flags exercise both (`usb-kbd` on root port 1, a `usb-hub` on root port 2 with a
`usb-mouse` behind it). Hot-plug only works behind a hub — the two UHCI root
ports are still probed just once at boot. Try it from the QEMU monitor:
`device_add usb-kbd,bus=usb-bus.0,port=2.2` then `device_del <id>`.

### Storage & block devices
`main_proc` ([`sched.c`](sched.c)) probes both channels at boot:
* **Floppy** — [`floppy.c`](floppy.c) + [`dma.c`](dma.c), mounted as `fda`
  (and `fdb` if present).
* **IDE / ATA hard disks** — [`ata.c`](ata.c), each detected drive mounted as
  `hda`, `hdb`, …

Any drive that carries a FAT volume is mounted automatically. Programs and
files are loaded from whichever device the path names — `start hda/hello`,
`read fda/mouse.bmp`, `cd hda` — so the OS runs equally from the floppy or the
hard disk. The QEMU setup attaches `floppy.img` (floppy A), `hda.img`
(primary master) and `os.iso` (the boot CD).

### PCI
[`pci.c`](pci.c) walks every bus/slot/function once at boot into a device table
(honouring the multifunction bit), names each one from a small class-code and
vendor/device table ([`pci_ids.c`](pci_ids.c)), decodes its BARs and IRQ line,
then hands each record to the first matching entry of a driver table. Every PCI
function the QEMU `pc` machine exposes has a handler, so nothing logs as
"unknown":

| Device | Driver | What it does |
| --- | --- | --- |
| 82441FX host bridge | [`pci_piix.c`](pci_piix.c) | identify |
| 82371SB PIIX3 ISA bridge | [`pci_piix.c`](pci_piix.c) | log PIRQ[A-D] routing + ELCR |
| 82371SB PIIX3 IDE | [`pci_piix.c`](pci_piix.c) | enable I/O + bus master, log IDETIM (transfers stay in [`ata.c`](ata.c)) |
| 82371SB PIIX3 USB (UHCI) | [`uhci.c`](uhci.c) | brought up later from the `usb` kernel thread |
| 82371AB PIIX4 ACPI | [`pci_acpi.c`](pci_acpi.c) | latch the PM I/O base; back `poweroff` / `reboot` |
| QEMU/Bochs standard VGA | [`pci_vga.c`](pci_vga.c) | record the framebuffer BAR, report the DISPI mode |
| 82540EM gigabit Ethernet | [`e1000.c`](e1000.c) | polled link layer; IPv4/DHCP on top — see below |
| 82801AA AC'97 audio | [`pci_ac97.c`](pci_ac97.c) | codec bring-up + BDL playback |

An unrecognised device (e.g. `-device rtl8139`) is still enumerated and named;
it just logs "has no driver". `pci` at the console reprints the table.

**ACPI power** ([`pci_acpi.c`](pci_acpi.c)): the PIIX4 PM register block has a
fixed layout, so no AML is needed — `poweroff` enters S5 with a single word
write to `PM1_CNT` (`exit_qemu(0)` uses this for a clean guest-initiated
shutdown), and `reboot` pulses the 0xCF9 reset-control port.

### Networking

The **e1000** driver ([`e1000.c`](e1000.c)) is the link layer only: it maps the
MMIO register BAR 1:1 into the kernel (it sits above the low-4 MiB identity map),
resets the card, brings the link up, reads the MAC from the EEPROM and sets up
static RX/TX descriptor rings in the identity-mapped `.bss`. It exposes
`e1000_send()` / `e1000_rx_poll()`.

On top of that, [`net.c`](net.c) is a very small IPv4 stack — Ethernet framing,
an 8-entry **ARP** cache (replies to who-has for our address, resolves next
hops), **IPv4** (20-byte header, checksum, on-link vs. gateway routing) and
**UDP** (checksum omitted, a tiny port→handler table). No fragmentation, no
options. Everything runs on the `net` kernel thread, so there is no locking; the
console's `ping` / `dns` / `http` commands submit their work to it with
`net_exec()` and block until it finishes.

* **DHCP** ([`dhcp.c`](dhcp.c)): on boot the `net` thread runs
  `DISCOVER → OFFER → REQUEST → ACK` against QEMU's SLIRP server and the machine
  comes up with an address (`10.0.2.15/24 via 10.0.2.2` by default), renewed at
  T1. Logged as `net: 10.0.2.15/24 via 10.0.2.2, dns 10.0.2.3, lease 86400s`.
* **ICMP** ([`icmp.c`](icmp.c)): replies to inbound echo requests (the guest is
  pingable) and backs the `ping <host> [count]` command, which prints per-packet
  RTTs from the free-running `pit_ms()` clock.
* **DNS** ([`dns.c`](dns.c)): a single-query A-record resolver over UDP to the
  DHCP-supplied server, honouring name compression. `dns <name>` prints the
  addresses; `ping` / `http` resolve names through it.
* **TCP** ([`tcp.c`](tcp.c)): minimal client — active open, stop-and-wait send,
  in-order receive, MSS option, a 1 s retransmit timer, up to four connections.
  No listen/accept, no congestion control. `http <host> [path]` (or
  `http http://host/path`) does an HTTP/1.0 GET and prints the response.

`ipv4_send()` / `udp_send()` and `tcp_connect/send/recv/close()` are the hooks
for anything more (there is no TLS or resolver cache).

### Drivers
| Area | Files |
| --- | --- |
| ATA / IDE disk (PIO, probed at boot) | [`ata.c`](ata.c), [`ata_asm.asm`](ata_asm.asm) |
| Floppy disk (+ DMA) | [`floppy.c`](floppy.c), [`dma.c`](dma.c) |
| PS/2 keyboard (IRQ-driven, ring-buffered) | [`keyboard.c`](keyboard.c), [`keyboard_asm.asm`](keyboard_asm.asm) |
| PS/2 mouse | [`mouse.c`](mouse.c), [`mouse_asm.asm`](mouse_asm.asm) |
| USB 1.1 host controller (UHCI, polled) | [`uhci.c`](uhci.c) |
| USB core (enumeration, control/interrupt transfers) | [`usb.c`](usb.c) |
| USB hub (recursive enumeration + hot-plug polling) | [`usb_hub.c`](usb_hub.c) |
| USB HID boot devices (keyboard, mouse) | [`usb_hid.c`](usb_hid.c) |
| PCI bus (enumeration, naming, driver binding) | [`pci.c`](pci.c), [`pci_ids.c`](pci_ids.c) |
| i440FX / PIIX3 chipset (host bridge, ISA bridge, IDE) | [`pci_piix.c`](pci_piix.c) |
| PIIX4 ACPI power management (poweroff / reboot) | [`pci_acpi.c`](pci_acpi.c) |
| Intel 82540EM gigabit NIC ("e1000", polled) | [`e1000.c`](e1000.c) |
| IPv4 stack (Ethernet, ARP, IPv4, UDP, ICMP) | [`net.c`](net.c), [`icmp.c`](icmp.c) |
| DHCP client / DNS resolver | [`dhcp.c`](dhcp.c), [`dns.c`](dns.c) |
| Minimal client TCP | [`tcp.c`](tcp.c) |
| QEMU / Bochs standard VGA (DISPI mode control) | [`pci_vga.c`](pci_vga.c) |
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
  its own linker script and no crt0 (entry point is `main`). All three are
  copied onto both disk images:
  * [`apps/hello`](apps/hello) — prints a line via the `printf` syscall and
    returns
  * [`apps/01`](apps/01) — returns immediately (staged as `tst`)
  * [`apps/example`](apps/example) — reads a number, a char and a string with
    `scanf` and echoes them back

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
| `pci` | list the enumerated PCI devices |
| `net` | interface MAC, link, counters and the DHCP-assigned address |
| `ping <host> [count]` | ICMP echo (resolves names via DNS) |
| `dns <name>` | DNS A-record lookup |
| `http <host> [path]` | HTTP/1.0 GET, prints the response |
| `poweroff` | power the machine off (ACPI S5) |
| `reboot` | reset the machine (0xCF9) |

Paths are resolved against the working directory. A name that contains `/` is
taken as device-qualified (`start hda/hello`); a bare name resolves against the
working directory, or against `/fda` when none is set — so `start hello` runs
`/fda/hello` and `read mouse.bmp` opens `/fda/mouse.bmp`. See
**Storage & block devices** for the device names.

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
./floppy.sh    # 1.44M FAT12 floppy image
./hda.sh       # 5M FAT hard-disk image
```

Both scripts use mtools (no root / loop device) and stage `hello`, `tst`,
`example` and the `mouse.bmp` cursor bitmap. The in-kernel FAT driver only
handles one sector per cluster, so the images are made with
`mkfs.fat -C … 1440` / `mkfs.fat -s 1`.

### Run in QEMU

```bash
make qemu-iso     # boot os.iso (GUI, SDL), serial on stdio
make qemu-nox     # same, no display
make qemu-kernel  # boot kernel.elf directly with -kernel
```

QEMU is launched with 256 MB RAM, `-vga std`, the floppy + IDE hard disk +
CD-ROM images, AC97 / SB16 / PC-speaker audio, a UHCI controller with a
`usb-kbd` on root port 1 and a `usb-hub` on root port 2 carrying a `usb-mouse`,
an `isa-debug-exit` device (the kernel uses it to exit QEMU with a status code),
and KVM acceleration.

Once the `>` prompt appears, try:

```
ls
start hello
cd hda
start tst
```

`start hello` loads `/fda/hello` (prints `Hello from userspace!`, exits 0);
after `cd hda`, `start tst` loads and runs `/hda/tst` straight off the hard
disk. Programs can be run back to back in one session.

### Other targets

```bash
make kernel.lst   # full objdump disassembly
make docs         # Doxygen API docs -> docs/html/index.html (needs doxygen)
make cloc         # source line count (needs cloc)
make clean        # remove build artifacts
```

## Kernel startup

`kernel_main` ([`main.c`](main.c)) brings the system up in this order:

UART/log → parse multiboot → physical MM (e820) → VMM → kernel heap →
VGA or VBE → GDT → IDT → FPU → PIC → PIT (1 kHz) → VFS → floppy detect →
keyboard → mouse → UART RX IRQ → sound → syscalls → TSS → RTC →
PCI (enumerate + bind drivers) → scheduler.

`sched_init()` does not return: it `iret`s into the scheduler's first process
(`main_proc` in [`sched.c`](sched.c)), which brings up the floppy and IDE
block devices, mounts their FAT volumes, starts the framebuffer redraw thread,
the USB thread (`usb_thread` — enumerate, then poll HID endpoints and hub
ports) and, if an e1000 was found, the `net` thread (`net_thread` — run the
DHCP client, then service the RX ring), and then runs the interactive console
(`kmain_console`).
