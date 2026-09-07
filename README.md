# koppi's hobby OS

A small hobby operating system for the **i386** architecture: a 32-bit
multiboot kernel written in C with a handful of drivers, a FAT filesystem, a
preemptive priority-scheduled process/thread model, a minimal C library and a
few userspace programs.

Current version: **git-derived** at build time — `(major, minor, revision)` is
computed from the last commit (`ver.h`, `main.c`, `Makefile`).

## Features

### Boot
* Boots via **GRUB** as a **Multiboot 1** *and* **Multiboot 2** image
  ([`boot.S`](boot.S), [`multiboot.c`](multiboot.c),
  [`multiboot2.c`](multiboot2.c)). The bootloader menu ([`grub.cfg`](grub.cfg))
  offers GUI (framebuffer) and console entries.
* `os.iso` is **self-contained**: GRUB also loads `initrd.img` (the userland,
  as a FAT16 image) as a Multiboot module, so the OS boots to a shell off a CD
  or USB stick with no hard disk or floppy attached. See
  [`initrd.c`](initrd.c) and **Storage & block devices**. A machine with a
  little over 136 MiB of RAM is needed (the module is relocated to 128 MiB);
  a smaller box falls back to booting from a real FAT disk.
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
* Fault isolation — a ring-3 exception (#GP, #PF, #UD, #DE, #BP, #OF, #BR,
  #NM, #TS, #NP, #SS, #MF, #AC, #XM) kills only the faulting process via
  `return_exception()`; kernel-mode faults still panic — [`exception.c`](exception.c).

### SMP / multi-core
QEMU is launched with `-smp 4` and **all four cores run kernel threads and
ring-3 processes in parallel** — `ps` and the `cpus` command show which core
each is on.

* Per-CPU state (`cpu_t`): its own current thread/process, page directory, TSS,
  idle thread, LAPIC timer and preemption gate — [`percpu.h`](percpu.h),
  [`smp.c`](smp.c). `this_cpu()` resolves it from the local APIC id.
* **AP bring-up**: the BSP parses the ACPI **MADT** ([`acpi.c`](acpi.c)),
  maps + calibrates the Local APIC ([`apic.c`](apic.c)), copies a real-mode
  trampoline to `0x8000` ([`ap_boot.asm`](ap_boot.asm)) and raises
  INIT-SIPI-SIPI. Each AP loads the kernel page tables, sets up its APIC / FPU /
  TSS in `ap_main` and parks until `sched_init` releases it into the scheduler.
* **SMP scheduler** ([`sched.c`](sched.c)): one global run queue (the process
  ring) under `sched_lock`; each core tracks its own current thread and a
  `process_t::cpu` field keeps a process from running on two cores at once.
  Preemption is driven by each core's LAPIC timer (vector `0xEF`) — no global
  PIT tick. A core with nothing to run falls back to its idle thread.
* **Fine-grained spinlocks** — real test-and-set with `pause`; the holder keeps
  local interrupts off (never preempted mid-section) but *spins* with them on,
  so a lock waiter still services TLB-shootdown IPIs. One lock per subsystem:
  physical allocator, page-table window, kernel heap, VMM, scheduler, process
  lifecycle, VFS and the console — [`spinlock.h`](spinlock.h). System-call
  entry is a **trap gate** so a syscall blocked on a lock stays interruptible.
* **TLB-shootdown IPIs** (vector `0xFD`): every `vmm_unmap*` / kernel-mapping
  change flushes locally, broadcasts via `lapic_ipi_allbutself` and waits for
  the other cores to acknowledge — [`apic.c`](apic.c). Degrades to a plain
  `invlpg` on `-smp 1`.
* **Reschedule IPIs** (vector `0xFC`) kick the other cores through `schedule()`
  when a new or boosted thread appears.
* The page-table storage window moved from a fixed `0x200000` (which the
  linked-in 1.3 MiB font blob had grown the kernel image straight through) to
  just past `kernel_end` — [`paging.c`](paging.c).

### Scheduling & processes
* Preemptive weighted fixed-priority round-robin scheduler with real-time
  policies (`SCHED_OTHER` / `SCHED_RR` / `SCHED_FIFO`) — [`sched.c`](sched.c).
  Each core's LAPIC timer runs `schedule()`: each thread carries a `weight`
  and its quantum is `WEIGHT_BASE * weight` ticks; it runs the highest-priority
  ready thread, round-robins equal priorities by quantum, and lets a higher
  priority preempt within a tick. SMP-aware — see **SMP / multi-core** above.
* Processes (flat binaries loaded from the filesystem) — [`proc.c`](proc.c)
* Threads — [`thread.c`](thread.h)
* `int 0x72` syscall gate — [`syscall.c`](syscall.c). Implemented calls:
  `printf`, `gets`/`scanf`, `fork`, `exit`, process return, `fopen`, `fclose`,
  `malloc`, `free`, `realloc`, `write`, `fread`, `time`, `clock`, `spit`
  (whole-file write).

### Filesystems
* Virtual filesystem layer — [`vfs.c`](vfs.c)
* **FAT** (FAT12/16) driver — [`fat.c`](fat.c). Reads the layout from the BPB
  (reserved sectors, FAT count/size, root-dir size), so it is not tied to
  1.44M floppy geometry — but it still assumes one sector per cluster.
  **Writes** are supported on FAT16: `fat_write_all()` reallocates a file's
  cluster chain (a one-pass free-cluster scan, both FAT copies kept in sync),
  writes the data and updates the directory entry — enough for a program on the
  OS to save a multi-KB output. `vfs_spit()` (create/truncate + write) is
  exposed to userspace as syscall #16.
* ELF loading helpers — [`elf.c`](elf.c)

### USB
A small polled USB stack, driven from a kernel thread (no USB interrupts).
Whichever host controller the machine has is brought up — the three cover
every PC from the QEMU `pc` machine to a modern ThinkPad:

* **UHCI** (USB 1.1) — [`uhci.c`](uhci.c). Found on the PCI bus, reset and
  started; its 1024-entry frame list, queue heads, transfer descriptors and
  data buffers are static and live in the identity-mapped low memory so
  `&x == phys(x)`.
* **EHCI** (USB 2.0) — [`ehci.c`](ehci.c). The controller on a Sandy/Ivy
  Bridge ThinkPad (X220), which has no xHCI. BIOS→OS handoff via `USBLEGSUP`,
  an async schedule for control transfers and a periodic schedule for
  interrupt-IN, plus one or two levels of hub with the split-transaction
  fields for the full-speed devices behind a PCH Rate-Matching Hub.
  **Opt-in** (`ehci` on the boot line): the handoff can disturb a real
  ThinkPad's PS/2 keyboard, so it stays off unless asked for. See
  **Booting on real hardware (ThinkPad X220 and similar)**.
* **xHCI** (USB 3.x) — [`xhci.c`](xhci.c). The only controller on a recent
  laptop (X250 / T470s). Controller bring-up, root-port reset, Enable Slot /
  Address Device / Configure Endpoint, then polled interrupt-IN. EHCI and
  xHCI both do their own enumeration and feed reports straight to the HID
  driver rather than going through `usb.c`.
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
`main_proc` ([`sched.c`](sched.c)) brings the block devices up at boot:
* **Boot RAM disk** — [`initrd.c`](initrd.c), mounted as `rd`. GRUB loads
  `initrd.img` (a FAT16 image with the whole userland) from `os.iso` as a
  Multiboot module; the kernel relocates it to high memory and serves it as a
  read/write block device. **This is the root filesystem** — `/rd/zsh`,
  `/rd/cc`, … — and it needs no attached disk, so `os.iso` boots on its own
  from a CD or a USB stick. Writes to `/rd` are RAM-backed and lost on reboot.
* **Floppy** — [`floppy.c`](floppy.c) + [`dma.c`](dma.c), mounted as `fda`
  (and `fdb` if present).
* **IDE / ATA hard disks** — [`ata.c`](ata.c), each detected drive mounted as
  `hda`, `hdb`, … `identify()` bounds every status poll, so an empty or absent
  IDE channel (the common case on real hardware, and when booting `os.iso`
  with no disk) is detected instead of hanging the boot.
* **AHCI / SATA** — [`ahci.c`](ahci.c), polled, one command in flight; the M.2
  SATA SSD on an X250-class laptop, mounted as the next free `hd{a,b,…}`.
* **NVMe** — [`nvme.c`](nvme.c), polled, one admin + one I/O queue; the M.2
  PCIe SSD on a Kaby Lake laptop (T470s), mounted as the next free `hd{a,b,…}`.
  See **Booting on real hardware (ThinkPad T470s …)** below.

Any drive that carries a FAT volume is mounted automatically. Programs and
files are loaded from whichever device the path names — `start rd/hello`,
`read rd/mouse.bmp`, `cd hda` — so the OS runs the same whether a file lives on
the RAM disk or a real disk. `make qemu-iso` boots `os.iso` alone;
`make qemu-iso DISK=hda.img` also attaches a persistent scratch disk (it shows
up as `/hda`), and `FLOPPY=floppy.img` likewise (`/fda`).

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
| virtio-gpu (`-vga virtio`) | [`virtio_gpu.c`](virtio_gpu.c) | modern virtio-pci 2D scanout off the framebuffer shadow; follows host window resizes |
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
console's `ping` / `dns` / `http` / `ntpdate` commands submit their work to it
with `net_exec()` and block until it finishes.

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
* **NTP** ([`ntp.c`](ntp.c)): once DHCP has a lease the `net` thread does a
  one-shot SNTP query (resolving `pool.ntp.org`, falling back to a fixed
  address) and writes the answer to the CMOS clock — `ntp: RTC set to
  Mon 2026-08-31 15:46:49 UTC`. The RTC is treated as UTC from then on;
  `ntpdate` re-syncs on demand and `date` prints it.
* **TCP** ([`tcp.c`](tcp.c)): minimal client — active open, stop-and-wait send,
  in-order receive, MSS option, a 1 s retransmit timer, up to four connections.
  No listen/accept, no congestion control. `http <host> [path]` (or
  `http http://host/path`) does an HTTP/1.0 GET and prints the response.
* **NFSv4.1** ([`nfs.c`](nfs.c)): an in-kernel NFS client hung off the VFS. Once
  DHCP has an address the `net` thread auto-mounts `cube00.fritz.box:/nfs`
  at `/nfs` (retrying every 10 s until the server answers), logged as
  `nfs: mounted ... at /nfs`. It speaks ONC-RPC (AUTH_SYS, from a reserved
  local port) over the TCP client above: EXCHANGE_ID + CREATE_SESSION, then one
  SEQUENCE-wrapped COMPOUND per operation — OPEN/CLOSE with real stateids, READ,
  WRITE, CREATE (via OPEN), REMOVE, READDIR, GETATTR — reconnecting on
  `NFS4ERR_BADSESSION`. Single session slot, no locking or delegations. Then
  `ls /nfs`, `cd /nfs`, `read /nfs/<file>`, `touch /nfs/<file>`,
  `write /nfs/<file> <text>` and `rm /nfs/<file>` work like any other mount;
  `nfs` prints the mount status. The server must permit the client's address
  (the client uses a privileged source port, so a default `sec`/`secure`
  export is fine).
* **SSHv2 server** ([`ssh.c`](ssh.c)): a small in-kernel sshd, enough for a
  stock OpenSSH client to log in and reach the same command shell as the
  local console. Algorithms: curve25519-sha256 key exchange, ssh-ed25519 host
  key (hex-encoded at `/rd/sshkey` — on the RAM disk it is regenerated each
  boot, so the client will warn about a changed host key; drop a persistent
  disk at `/hda` and point `HOSTKEY_PATH` there to keep it stable),
  aes128-ctr cipher, hmac-sha2-256 MAC — all defaults a modern OpenSSH client
  already offers, so no client-side flags are needed beyond accepting the
  host key on first connect. Authentication is a single fixed
  username/password (`koppi` / `os` — see `SSH_USERNAME`/`SSH_PASSWORD` in
  [`ssh.c`](ssh.c)), since there is no user database in this OS. A `shell`
  channel request bridges to `console_exec()` — the exact same command table
  as the physical keyboard console — echoing keystrokes and mirroring output
  both locally and over the channel; an `exec` request runs one command and
  exits. Like NFS, this runs entirely on the `net` thread: `ssh_tick()` polls
  the listening socket and, once accepted, runs the whole session to
  completion before returning, so only one SSH session is serviced at a time
  and other `net`-thread work pauses for its duration. `ssh` prints the
  listener status. The underlying crypto (SHA-256/512, a small 256-bit
  bignum, Curve25519, Ed25519, AES-128) is hand-rolled in
  [`bignum256.c`](bignum256.c), [`sha2.c`](sha2.c),
  [`curve25519.c`](curve25519.c), [`ed25519.c`](ed25519.c) and
  [`aes128.c`](aes128.c) — there is no OpenSSL/libsodium here — seeded by a
  best-effort timing-jitter CSPRNG ([`csprng.c`](csprng.c)); treat it as a
  hobby-OS demo, not an audited implementation.

`ipv4_send()` / `udp_send()` and `tcp_connect/send/recv/close()` are the hooks
for anything more (there is no TLS or resolver cache).

### Drivers
| Area | Files |
| --- | --- |
| Boot RAM disk (Multiboot module, mounted `/rd`) | [`initrd.c`](initrd.c) |
| ATA / IDE disk (PIO, probed at boot) | [`ata.c`](ata.c), [`ata_asm.asm`](ata_asm.asm) |
| Floppy disk (+ DMA) | [`floppy.c`](floppy.c), [`dma.c`](dma.c) |
| PS/2 keyboard (IRQ-driven, ring-buffered) | [`keyboard.c`](keyboard.c), [`keyboard_asm.asm`](keyboard_asm.asm) |
| PS/2 mouse | [`mouse.c`](mouse.c), [`mouse_asm.asm`](mouse_asm.asm) |
| USB host controllers, polled (UHCI 1.1 / EHCI 2.0 / xHCI 3.x) | [`uhci.c`](uhci.c), [`ehci.c`](ehci.c), [`xhci.c`](xhci.c) |
| USB core (enumeration, control/interrupt transfers) | [`usb.c`](usb.c) |
| USB hub (recursive enumeration + hot-plug polling) | [`usb_hub.c`](usb_hub.c) |
| USB HID boot devices (keyboard, mouse) | [`usb_hid.c`](usb_hid.c) |
| PCI bus (enumeration, naming, driver binding) | [`pci.c`](pci.c), [`pci_ids.c`](pci_ids.c) |
| i440FX / PIIX3 chipset (host bridge, ISA bridge, IDE) | [`pci_piix.c`](pci_piix.c) |
| PIIX4 ACPI power management (poweroff / reboot) | [`pci_acpi.c`](pci_acpi.c) |
| Intel 82540EM gigabit NIC ("e1000", polled) | [`e1000.c`](e1000.c) |
| IPv4 stack (Ethernet, ARP, IPv4, UDP, ICMP) | [`net.c`](net.c), [`icmp.c`](icmp.c) |
| DHCP client / DNS resolver | [`dhcp.c`](dhcp.c), [`dns.c`](dns.c) |
| SNTP client (sets the RTC at boot) | [`ntp.c`](ntp.c) |
| Minimal TCP (client + passive-open listen/accept) | [`tcp.c`](tcp.c) |
| In-kernel NFSv4.1 client (auto-mounts `/nfs`) | [`nfs.c`](nfs.c) |
| In-kernel SSHv2 server (port 22) | [`ssh.c`](ssh.c) |
| Crypto primitives (SHA-2, bignum, Curve25519, Ed25519, AES-128, CSPRNG) | [`sha2.c`](sha2.c), [`bignum256.c`](bignum256.c), [`curve25519.c`](curve25519.c), [`ed25519.c`](ed25519.c), [`aes128.c`](aes128.c), [`csprng.c`](csprng.c) |
| QEMU / Bochs standard VGA (DISPI mode control) | [`pci_vga.c`](pci_vga.c) |
| virtio-gpu 2D scanout + live window-resize (`-vga virtio`) | [`virtio_gpu.c`](virtio_gpu.c) |
| AC97 audio | [`pci_ac97.c`](pci_ac97.c), [`sound.c`](sound.c) |
| PC speaker | [`pcspk.c`](pcspk.c) |
| VGA / VBE framebuffer | [`vga.c`](vga.c), [`video.c`](video.c), [`graphics.c`](graphics.c) |
| PIT timer (1 kHz tick) | [`pit.c`](pit.c) |
| RTC / CMOS clock (Unix-time conversion, NTP-settable) | [`rtc.c`](rtc.c) |
| Serial UART (kernel console / log) | [`uart.c`](uart.c) |

### Graphics / UI
* [`microui.c`](microui.c) immediate-mode UI + [`renderer.c`](renderer.c)
* [`ssfn.h`](ssfn.h) Scalable Screen Font renderer; the GNU Unifont
  ([`unifont.sfn`](unifont.sfn)) is linked into the kernel as `font.o`.
* BMP loader — [`bmp.c`](bmp.c) (e.g. [`mouse.bmp`](mouse.bmp) cursor)

### Audio
* [`hxcmod.c`](hxcmod.c) Amiga MOD player; sample module in
  [`mods/01.mod`](mods/01.mod).
* Audio output goes through whichever card is present: the AC97 codec
  ([`pci_ac97.c`](pci_ac97.c)), a Sound Blaster 16 via [`sound.c`](sound.c),
  or — on the laptops — Intel HD Audio ([`hda.c`](hda.c)), with MOD playback
  routed through the Azalia codec when one is present. The PS/2 keyboard's
  **volume-up / volume-down / mute** keys drive the master level: they step
  the SB16 mixer inline from the IRQ and flag the HD Audio thread to re-apply
  the codec's output amp ([`sound.c`](sound.c), [`hda.c`](hda.c),
  [`keyboard.c`](keyboard.c)).

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
* Per-process user heap ([`heap.c`](heap.c)) backing the `malloc`/`free`/
  `realloc` syscalls: a first-fit free list that starts at 4 pages and
  [grows on demand](heap.c) (`heap_grow`) up to 64 MiB. Each process also gets
  a **256 KiB** user stack and a 16 KiB ring-0 stack ([`proc.c`](proc.c)).
* `int 0x72` calls: `printf`(0), `gets`(1), `fork`(3), `exit`(4), return(5),
  `fopen`(6), `fclose`(7), `malloc`(9), `free`(10), `realloc`(11),
  `write`(12, length-delimited), `fread`(13, 512-byte block),
  `time`(14, RTC), `clock`(15, PIT), `spit`(16, whole-file write:
  `spit(path, buf, len)` creates/truncates the file and writes `len` bytes),
  `getkey`(17, one unechoed keystroke), `run`(18, run a console command line),
  `getcwd`(19), `listdir`(20, newline-separated directory listing),
  `spawn`(21, load+run a program, blocking) — see [`syscall.c`](syscall.c).
  `write_file()` in [`lib/`](lib) wraps #16; 17-21 back [`apps/zsh`](apps/zsh).
* The ELF loader ([`elf.c`](elf.c)) maps every page of a `PT_LOAD` segment to
  its own frame and covers the `.bss` tail, so multi-page ring-3 binaries load.
* Example programs in [`apps/`](apps), each linked as a flat ring-3 binary with
  its own linker script and no crt0 (entry point is `main`), copied onto both
  disk images:
  * [`apps/hello`](apps/hello) — prints a line via the `printf` syscall and
    returns
  * [`apps/01`](apps/01) — returns immediately (staged as `tst`)
  * [`apps/example`](apps/example) — reads a number, a char and a string with
    `scanf` and echoes them back
  * [`apps/mem`](apps/mem) — exercises heap growth, `realloc` and deep
    recursion (staged as `mem`)
  * [`apps/lua`](apps/lua) — **Lua 5.4.8**, ported to run as a ring-3 program
    (staged as `lua`); see below
  * [`apps/cc`](apps/cc) — **a self-hosting C compiler** (staged as `cc`);
    see below
  * [`apps/zsh`](apps/zsh) — the interactive **zsh-flavoured shell** (staged as
    `zsh`, launched at boot); see **Shell** above

### C compiler

[`apps/cc/cc.c`](apps/cc/cc.c) is a tiny **two-pass C compiler** that runs as a
ring-3 program and compiles a single C file straight to a runnable ELF — there
is no assembler or linker on the OS, so `cc` is front end **+** i386 code
generator **+** ELF writer in one. It is written in the C subset it accepts, so
it **compiles itself**:

```
make qemu-iso                          # boot os.iso (no disk needed)
cd rd
cc cc.c -o cc2                         # cc compiles its own source
cc2 cc.c -o cc3                        # the result compiles it again
sum cc2 ; sum cc3                      # identical checksums => fixed point
cc2 test.c -o t ; t                    # -> "test: 37 checks OK"
```

* **Pass 1** (front end): lex → parse → semantic analysis, producing a typed
  AST and an insertion-ordered symbol table. **Pass 2** (back end): walk the
  AST emitting i386 machine code into a byte buffer with a relocation list,
  then lay out one `PT_LOAD` ELF at `0x800000` and patch the relocations.
* **One translation unit.** `cc` prepends [`prelude.c`](apps/cc/prelude.c) (a
  small libc: `malloc`/`printf`/`string.h`/file I/O over the syscalls) unless
  `-nostdlib` is given, so a compiled program links nothing.
* **No preprocessor** by design: `#include`/`#define` lines are skipped by the
  lexer; the libc surface comes from the prelude. Two code-generator builtins
  (`__syscall0..3`, and the plain i386 `&arg` varargs trick) stand in for
  inline assembly.
* Accepts `struct`/`union`/`enum`/`typedef`, pointers and arrays, function
  pointers, `switch`, `goto`, every operator, varargs **call sites**, and
  constant global initializers. Not supported (diagnosed, not mis-compiled):
  `float`/`double`, 64-bit `long long` (folded to 32-bit), by-value struct
  args/returns, the preprocessor. See [`apps/cc/NOTES.md`](apps/cc/NOTES.md).
* Cross-develop on Linux with `make -C apps/cc native` (`./cc-native`); it
  emits byte-identical output to the on-OS `cc` for any input.

### Lua
[`apps/lua`](apps/lua) is a full port of the reference **Lua 5.4.8**
interpreter. The vendored `src/` tree is unmodified bar a two-line
[`luaconf.h`](apps/lua/src/luaconf.h) note; everything platform-specific lives
in [`apps/lua/shim/`](apps/lua/shim):

* **libm** — `sin`/`cos`/`tan`/`atan2`/`exp`/`log`/`pow`/`sqrt`/`floor`/`ceil`/
  `fmod`/`frexp`/`ldexp` implemented directly on the **x87 FPU**
  ([`libm.S`](apps/lua/shim/libm.S)). The scheduler now saves per-thread FPU
  state ([`fpu.c`](fpu.c), [`sched.c`](sched.c)) so this is safe under preemption.
* **setjmp/longjmp** — 6-word i386 buffer ([`port_asm.S`](apps/lua/shim/port_asm.S)),
  used for Lua's error handling.
* **stdio** — `stdout`/`stderr` go to the console via the `write` syscall,
  `stdin` reads a line at a time via `gets`, and files on the FAT volume are
  read through `fopen`/`fread`/`fclose` ([`stdio.c`](apps/lua/shim/stdio.c)).
* `strtod`, a standard-signature `string.c`, ascii `ctype.h`, a civil-time
  `time.c` (for `os.time`/`os.date`), and the in-tree
  [`printf.c`](printf.c)/[`arith64.c`](arith64.c) round it out.

`lua_Number` is `double` and `lua_Integer` is 64-bit. Libraries: `base`,
`package`, `coroutine`, `table`, `string`, `math`, `utf8`, and a trimmed `os`
(`time`/`date`/`clock`/`getenv`/`exit`; `execute`/`remove`/`rename` fail). No
`io` or `debug`. `require` searches `/rd/?.lua` and `/fda/?.lua`.

```
cd rd
lua                          -- REPL (exit with os.exit() or Ctrl-D)
lua t.lua                    -- run a script; t.lua is the port's self-test
```

There is no Ctrl-C, so a non-terminating script needs a `reboot`. See
[`apps/lua/PORTING.md`](apps/lua/PORTING.md) for the full build and shim notes.

### Shell

The interactive shell is [`apps/zsh`](apps/zsh/main.c) — a small **zsh-flavoured
shell that runs in ring 3**. The scheduler's first process (`main_proc`) launches
`/rd/zsh` and blocks until it exits, falling back to the in-kernel console
(`kmain_console`, below) if the image is missing or the shell leaves (Ctrl-D /
`exit`).

`zsh` owns the interactive surface — a line editor with history, tab-completion,
aliases, `!` history expansion and `*`/`?` globbing — but it does not
reimplement the commands. It classifies each line:

* **shell builtins** (`cd`, `pwd`, `echo`, `history`, `alias`/`unalias`,
  `which`, `help`, `source`, `clear`, `exit`) run in-process;
* a name that matches a **program** on the current volume (or `/rd`) is loaded
  and run via the `spawn` syscall — so `hello` works like `./hello`, and a file
  argument (`lua t.lua`) is resolved against the working directory;
* **everything else** is handed to the kernel console dispatcher through the
  `run` syscall, so `ls`, `pci`, `date`, `poweroff` … behave exactly as they do
  on the in-kernel console.

Editor keys: `Tab` completes, `^P`/`^N` walk history, `^U`/`^W` kill,
`^L` clears, `^C` abandons the line, `^D` on an empty line leaves the shell.
Over a PS/2 keyboard only the printable set plus `Tab`/Backspace reach the
shell, so history there is via `!!` / `!n` / `!prefix`; a serial console
(`-serial mon:stdio`) delivers the control keys too. `/rd/zshrc` is sourced at
start-up — a place for aliases.

New syscalls behind this: `getkey` (17, one unechoed keystroke), `run` (18,
`console_exec` on behalf of ring 3), `getcwd` (19), `listdir` (20, for
completion/globbing) and `spawn` (21). `spawn` is marshalled onto `main_proc`
because the ELF loader stages the image at a fixed kernel address that a ring-3
process's page directory does not map ([`commands.c`](commands.c)
`console_spawn_request` / `console_spawn_service`).

### Kernel console (`kmain_console`)

The kernel debug console ([`commands.c`](commands.c), `kmain_console`) is the
fallback shell and the command table that `zsh`'s `run` syscall and the SSH
`shell` channel dispatch through. It reads keystrokes from the ring-buffered
keyboard driver, echoes them, supports backspace, and executes a line on Enter.
(The framebuffer desktop's console window is now output-only — it shows the log
but no longer competes with the shell for keystrokes.)

| Command | Effect |
| --- | --- |
| `help` | list commands |
| `mem` | physical memory, kernel heap and `cr0/cr2/cr3` |
| `ps` | process table (with the CPU each process is on) |
| `cpus` | online CPUs and what each core is currently running |
| `ls` | list the working directory (device list at the root) |
| `cd [dir]` | change working directory; no argument resets to the root |
| `start <prog> [args]` | load an ELF, run it in ring 3, block until it exits, then reap it |
| `read <file>` | print a file |
| `touch <file>` | create an empty file |
| `write <file> <text>` | write one record of text to a file |
| `rm <file>` | delete a file |
| `sum <file>` | FNV-1a checksum + byte length of a file |
| `beep` | play a tone through the AC97 codec |
| `pci` | list the enumerated PCI devices |
| `net` | interface MAC, link, counters and the DHCP-assigned address |
| `nfs` | NFSv4.1 client mount status for `/nfs` |
| `ssh` | in-kernel SSH server listener status |
| `ping <host> [count]` | ICMP echo (resolves names via DNS) |
| `dns <name>` | DNS A-record lookup |
| `http <host> [path]` | HTTP/1.0 GET, prints the response |
| `date` | print the wall clock as a Unix timestamp and a UTC string |
| `ntpdate` | re-sync the RTC from an NTP server, then print it |
| `poweroff` | power the machine off (ACPI S5) |
| `reboot` | reset the machine (0xCF9) |

Paths are resolved against the working directory. A name that contains `/` is
taken as device-qualified (`start rd/hello`); a bare name resolves against the
working directory, or against `/rd` when none is set — so `start hello` runs
`/rd/hello` and `read cc.c` opens `/rd/cc.c`. See **Storage & block devices**
for the device names. (`/fda` is still reachable by an explicit path, but the
floppy driver's read path can wedge on a cold motor, so it is not the default.)

#### `coreutils` — a busybox-style toolbox ([`coreutils.c`](coreutils.c))

`console_exec()` tries the built-ins above first, then hands anything else to a
small toolbox of coreutils / util-linux commands. They are reachable the same
three ways as everything else: the kernel console, `zsh` (via the `run`
syscall), and an SSH channel. Filters take a **file argument** — there is no
stdin, no pipe and no redirection — and, because the kernel heap is only
~100 KiB, the whole-file commands (`sort`, `tac`, `tail`, `uniq`) cap at 64
lines and `cp` / `base64` / `wget` stream through a right-sized heap block.

| Group | Commands |
| --- | --- |
| view / slice | `cat` `head` `tail` `nl` `tac` `rev` `cut` `strings` `hexdump` (=`xxd` `od` `hd`) |
| inspect | `wc` `grep` (`egrep` `fgrep`) `cmp` `cksum` `md5sum` `sha256sum` `du` |
| transform | `tr` `sort` `uniq` `base64` |
| files | `cp` `mv` `basename` `dirname` (no `mkdir` / `ln` — the FAT driver is root-directory-only) |
| shell-ish | `echo` `printf` `pwd` `true` `false` `seq` `sleep` `usleep` `yes` `expr` `factor` `time` |
| system | `uname` `arch` `hostname` `whoami` `id` `groups` `logname` `nproc` `uptime` `free` `date` `cal` `env` `printenv` `clear` (`reset`) `sync` `mount` `umount` `halt` |
| net | `wget <url> [-O file]` |

`grep` matches a fixed substring (not a regex); `-i` `-v` `-n` `-c` work.
`expr` handles `+ - * / %`, the comparisons, and `length` / `substr` / `index`.
`date +FORMAT` understands `%s %Y %m %d %H %M %S %%`. `yes` prints until a key
is pressed. Commands that classically report status (`true`, `false`, `cmp`)
just run — the console has no `$?`.

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
hda.sh floppy.sh   scripts to (re)build the FAT images (initrd / hd / floppy)
```

## Building and Running

### Prerequisites (Debian/Ubuntu)

```bash
sudo apt -y install grub-common xorriso mtools gcc-multilib qemu-system-x86 grub-pc-bin
```

### Build

```bash
make
```

This builds the userspace library and apps, compiles the kernel to
`kernel.elf`, and (via the `iso` target) packs the userland into `initrd.img`
and produces the bootable, self-contained `os.iso`.

Toolchain: system `gcc -m32` / GNU `ld` (`-melf_i386`), NASM for `.asm` stubs.
Kernel flags: `-Og -std=gnu11 -ffreestanding -fno-builtin -nodefaultlibs
-fno-stack-protector -m32`, warnings as errors (`-Werror -Wall -Wextra`),
`-DDEBUG`.

### FAT images

`make iso` builds `initrd.img` automatically — that is the root filesystem the
running OS sees as `/rd`. `hda.img` / `floppy.img` are optional extra disks,
built locally, only needed if you want persistent storage across reboots.

```bash
IMG=initrd.img SIZE=8M ./hda.sh   # what `make iso` runs
./hda.sh                          # 16M FAT16 hd image (hda.img)
./floppy.sh                       # 1.44M FAT12 floppy image
```

All use mtools (no root / loop device) and stage the `zsh` shell (with its
`zshrc`), `hello`, `tst`, `example`, `mem`, `fault`, the `lua` interpreter (with
`t.lua` / `mod.lua`), `mouse.bmp`, and the `cc` compiler with its source
(`cc.c`), runtime (`prelude.c`) and tests. The in-kernel FAT driver only
handles one sector per cluster, so the images are made with `mkfs.fat -F 16
-s 1` (or `-C … 1440` for the FAT12 floppy).

### Run in QEMU

```bash
make qemu-iso                  # boot os.iso (GUI, SDL), serial on stdio
make qemu-nox                  # same, no display
make qemu-kernel               # boot kernel.elf directly (-kernel -initrd)
make qemu-iso DISK=hda.img     # also attach a persistent scratch disk (/hda)
```

QEMU is launched with 256 MB RAM, `-vga std`, the `os.iso` CD-ROM (which
carries the kernel *and* the initrd — no other drive is attached by default),
AC97 / SB16 / PC-speaker audio, a UHCI controller with a `usb-kbd` on root
port 1 and a `usb-hub` on root port 2 carrying a `usb-mouse`, an
`isa-debug-exit` device (the kernel uses it to exit QEMU with a status code),
KVM acceleration, and a SLIRP `netdev` forwarding host port **2222** to the
guest's port 22 — once the guest has DHCP'd an address (see the boot log),
`ssh -p 2222 koppi@localhost` (password `os`) from the host reaches the
in-kernel SSH server.

Once the `>` prompt appears, try:

```
ls
cd rd
hello
lua t.lua
```

`hello` loads `/rd/hello` (prints `Hello from userspace!`, exits 0). Programs
can be run back to back in one session. On real hardware, write `os.iso` to a
USB stick (`sudo dd if=os.iso of=/dev/sdX bs=4M` — it is isohybrid) and boot
it; the OS comes up the same way, with no disk required.

### Booting on real hardware (ThinkPad X250 and similar)

`os.iso` is a hybrid image: `grub-mkrescue` builds it with both a BIOS El
Torito catalog and an EFI System Partition (`BOOTX64.EFI`), so the same USB
stick boots under **UEFI** or **Legacy/CSM**. On a ThinkPad X250 either works
— for CSM, set *Startup → UEFI/Legacy Boot* to `Both` or `Legacy Only` in the
firmware setup.

The X250 has no serial port, so **all boot output goes to the screen** via a
framebuffer text console (`fbcon`, in [`video.c`](video.c)): the full kernel
log scrolls during bring-up, and a panic paints the fault + register dump
full-screen instead of triple-faulting into a reboot loop. Once the desktop
compositor starts it takes over the screen and the log continues in the
on-screen console window.

What comes up on the X250:

| Subsystem   | Driver                                    | Notes |
|-------------|-------------------------------------------|-------|
| Display     | multiboot2 GOP/VBE framebuffer ([`video.c`](video.c)), write-combining via PAT ([`pat.c`](pat.c)) | any width/pitch/bpp; 32-bpp GOP is ideal, 24-bpp VBE works. No native modeset — the firmware sets the mode, the kernel just draws to it |
| Keyboard / TrackPoint / touchpad | i8042 PS/2 ([`keyboard.c`](keyboard.c), [`mouse.c`](mouse.c)) | full controller bring-up, not firmware-dependent |
| Storage     | AHCI / SATA ([`ahci.c`](ahci.c))          | the M.2 SSD, mounted `/hda` |
| USB         | xHCI ([`xhci.c`](xhci.c))                 | external HID keyboards / mice (boot protocol) |
| Ethernet    | Intel I218-LM ([`e1000.c`](e1000.c))      | shares the 8254x register model; PCH-LAN reset quirks handled |
| Timers / IRQ | LAPIC timer + PIT ch.2 + i8259 virtual-wire | no dependency on IRQ 0 being delivered |
| SMP         | ACPI MADT (RSDP from the multiboot2 tag under UEFI) | all cores |
| Audio | Intel HD Audio ([`hda.c`](hda.c)) | analog codec `8086:9ca0`; one-shot PCM / `beep` |
| RTC, ACPI power-off, PC speaker | as on QEMU | |

The framebuffer is mapped **write-combining** (a WC entry programmed into
IA32_PAT, [`pat.c`](pat.c)) rather than strong-uncacheable. On real hardware
the panel lives across the display link, so an uncached mapping turned every
pixel into its own bus transaction and the desktop crawled; WC lets the CPU
burst a cache line at a time. There is no native Intel-graphics driver — no
GEM, no GTT, no modesetting — the firmware brings up the panel and the kernel
composites into the linear framebuffer it was handed. The compositor is
capped at ~60 fps so it stops re-blitting the whole screen as fast as the bus
allows.

Not supported: the Intel Wireless-AC 7265 WiFi, the Realtek RTS5227 SD-card
reader, the fingerprint reader, and USB mass storage. Every device is named
in the `pci` output regardless.

`test/x250-boot.sh` boots `os.iso` in QEMU `q35` configurations that
approximate the X250 (AHCI, xHCI, `e1000e`), under both SeaBIOS and OVMF
(UEFI), capturing a serial log and a screenshot for each.

### Booting on real hardware (ThinkPad T470s and similar)

The same hybrid `os.iso` boots a **Kaby Lake** ThinkPad (T470s / T460s /
T470 / X270 — Sunrise Point-LP PCH) under UEFI or CSM. The T470s also has no
serial port, so boot output goes to the screen the same way (`fbcon`).

The one thing a T470s does differently from an X250 is **storage**: its M.2
slot usually carries an **NVMe** SSD, not a SATA one, so the kernel now has a
small NVMe driver.

| Subsystem | Driver | Notes |
|-----------|--------|-------|
| Display | multiboot2 GOP/VBE framebuffer ([`video.c`](video.c)), WC via PAT ([`pat.c`](pat.c)) | HD Graphics 620 (`8086:5916`); firmware sets the mode (1920×1080 or 2560×1440), no native modeset |
| Keyboard / TrackPoint / touchpad | i8042 PS/2 ([`keyboard.c`](keyboard.c), [`mouse.c`](mouse.c)) | Synaptics absolute + TrackPoint pass-through; a non-Synaptics pad falls back to the plain 3-byte protocol (`nosyn` forces it) |
| Storage (NVMe) | [`nvme.c`](nvme.c) | the M.2 PCIe SSD — one admin + one I/O queue, polled, namespace 1 mounted as the next free `hd{a,b,…}` (512-byte-block namespaces only) |
| Storage (SATA) | AHCI ([`ahci.c`](ahci.c)) | for a T470s built with an M.2 **SATA** SSD instead |
| USB | xHCI ([`xhci.c`](xhci.c)) | Sunrise Point-LP `8086:9d2f`; external HID keyboards / mice |
| Ethernet | Intel I219-LM ([`e1000.c`](e1000.c)) | `is_pch_lan()` covers the whole I219 family (Kaby Lake `15d7`/`15d8`/`15b8`…) |
| Audio | Intel HD Audio ([`hda.c`](hda.c)) | Sunrise Point-LP `8086:9d71` |
| Timers / IRQ / SMP / RTC / power-off | as on the X250 | LAPIC timer, ACPI MADT, no IRQ-0 dependency |

The NVMe driver ([`nvme.c`](nvme.c)) is deliberately minimal: it disables the
controller, sets up a page-aligned admin queue pair in identity-mapped `.bss`,
`IDENTIFY`s the controller and the active namespace list, creates one I/O
queue pair (interrupts off — every completion is polled), and serves
512-byte sector reads/writes through the same [`device_t`](device.h) block
interface AHCI and IDE use. A 64-bit BAR a UEFI firmware parked above 4 GiB is
re-homed into the low PCI hole, same as [`xhci.c`](xhci.c). A namespace with a
non-512-byte LBA format is detected and left unmounted rather than
mis-mounted.

If a real laptop comes up **black** — a `T470s` has no serial port and the
screen stays dark until `fbcon_init()` — add **`bootdiag`** to the GRUB line
(there is a "bootdiag" menu entry). It turns the PC speaker into an
early-boot progress beacon: at checkpoint N of the pre-console bring-up
(1 parsed multiboot, 2 physical MM up, 3 paging on, 4 PAT done, 5
framebuffer up) the kernel beeps N times and, while paging is still off,
floods the framebuffer with a per-checkpoint colour. The last beep count /
colour you get is the last checkpoint the kernel reached — the next step is
the one that hung. See [`main.c`](main.c).

Escape hatches on the GRUB line (press `e`): `nonvme` `noahci` `noxhci`
`nousb` `nonet` `nosmp` `nofb` `nosyn`. The "safe" menu entry sets
`noxhci noahci nonvme nosmp`.

Not supported: the Intel Wireless-AC 8265 WiFi, the Realtek RTS522A SD-card
reader, the fingerprint reader, and USB mass storage — every device is still
named in the `pci` output.

`test/t470s-boot.sh` (`make qemu-t470s`) boots `os.iso` in QEMU `q35` configs
that approximate the T470s (NVMe, xHCI, `e1000e`, Intel HD Audio) under both
SeaBIOS and OVMF/UEFI, capturing a serial log and a screenshot for each.

### Booting on real hardware (ThinkPad X220 and similar)

The same hybrid `os.iso` boots a **Sandy Bridge** ThinkPad (X220 / X220i /
T420 / T520 — 6-series "Cougar Point" PCH). The X220 is a BIOS/CSM machine —
set *Startup → Boot Mode* to `Legacy` — and, like the newer ThinkPads, has no
serial port, so boot output goes to the screen (`fbcon`).

Electrically the X220 is an older, simpler X250. The one thing it does
differently is **USB**: the 6-series chipset has *no xHCI* — USB is **EHCI**
(USB 2.0) only — so the kernel has an EHCI driver. Its 82579LM Ethernet was
already covered by `e1000.c`'s PCH-LAN path, its SATA is AHCI, and its audio is
Intel HD Audio, so everything else carries over unchanged.

| Subsystem | Driver | Notes |
|-----------|--------|-------|
| Display | multiboot2 GOP/VBE framebuffer ([`video.c`](video.c)), WC via PAT ([`pat.c`](pat.c)) | HD Graphics 3000 (`8086:0126`); the CSM VBIOS sets a 1366×768 linear mode (24- or 32-bpp), no native modeset |
| Keyboard / TrackPoint / touchpad | i8042 PS/2 ([`keyboard.c`](keyboard.c), [`mouse.c`](mouse.c)) | Synaptics absolute + TrackPoint pass-through; `nosyn` forces the plain 3-byte protocol |
| Storage | AHCI ([`ahci.c`](ahci.c)) | the 2.5" SATA SSD/HDD (`8086:1c03`), mounted `/hda`. Set the BIOS SATA mode to `AHCI` |
| USB (opt-in) | **EHCI** ([`ehci.c`](ehci.c)) | `8086:1c26` / `8086:1c2d`; external HID keyboards / mice. **Off by default** — add `ehci` to the boot line |
| Ethernet | Intel 82579LM ([`e1000.c`](e1000.c)) | `8086:1502`; `is_pch_lan()` skips the disruptive MAC reset and reads the MAC from `RAL0`/`RAH0` |
| Audio | Intel HD Audio ([`hda.c`](hda.c)) | Cougar Point `8086:1c20` |
| Timers / IRQ / SMP / RTC / power-off | as on the X250 | LAPIC timer, ACPI MADT (RSDP from the BIOS scan), no IRQ-0 dependency |

**USB is opt-in on the X220.** Taking the EHCI controller from the firmware
(the `USBLEGSUP` BIOS→OS handoff) knocks the BIOS "USB legacy support" SMM out
from under the 8042 on a real X220 and kills the *internal* PS/2 keyboard —
which the machine needs, since its keyboard and TrackPoint are PS/2. So
`ehci_init()` does nothing unless `ehci` is on the boot line (there is an "os
(USB 2.0 / EHCI enabled)" GRUB entry for it), and when it does run it calls
`keyboard_reinit()` after the handoff to put the 8042's translation + IRQ 1
back. The default boot leaves USB alone and the built-in keyboard works.

When enabled, the EHCI driver ([`ehci.c`](ehci.c)) is the same shape as
[`xhci.c`](xhci.c): the `USBLEGSUP` handoff, a controller reset, an **async
schedule** (one queue head, one control transfer in flight) for enumeration,
and a **periodic schedule** (a 1024-entry frame list feeding a chain of
interrupt queue heads, one persistent transfer descriptor per HID endpoint,
re-armed after each poll) for the boot reports. The 6-series PCH sits a
**Rate-Matching Hub** between the EHCI root ports and the connectors, so a
full-speed keyboard is reached through a transaction translator — `ehci.c`
walks one or two levels of hub and fills in the split-transaction fields, but
that path has no QEMU equivalent and is unverified on real hardware.

Escape hatches on the GRUB line (press `e`): `nousb` `noahci` `nonet` `nosmp`
`nofb` `nosyn` to disable, `ehci` to enable USB 2.0.

Not supported: the Intel Centrino Advanced-N 6205 WiFi, the SD-card reader,
the fingerprint reader, and USB mass storage — every device is still named in
the `pci` output.

`test/x220-boot.sh` (`make qemu-x220`) boots the kernel (via `-kernel`, with
`ehci` on the command line) in a BIOS (`qemu-system-i386 -machine pc`) and a
`q35` config, each with an EHCI controller, a USB keyboard and a USB tablet,
capturing a serial log and a
screenshot for each.

### Other targets

```bash
make kernel.lst   # full objdump disassembly
make docs         # Doxygen API docs -> docs/html/index.html (needs doxygen)
make cloc         # source line count (needs cloc)
make clean        # remove build artifacts
```

## Kernel startup

`kernel_main` ([`main.c`](main.c)) brings the system up in this order:

UART/log → parse multiboot → **relocate the initrd module** → physical MM
(e820, initrd frames reserved) → VMM (initrd identity-mapped) → kernel heap →
PAT (WC memory type) → VGA or VBE → GDT → IDT → FPU → PIC → PIT (1 kHz) → VFS → floppy detect →
keyboard → mouse → UART RX IRQ → sound → syscalls → TSS → RTC →
PCI (enumerate + bind drivers) → **ACPI/MADT → Local APIC → AP bring-up** →
scheduler.

`sched_init()` does not return: it builds one idle thread per core, releases
the parked application processors and `iret`s into the scheduler's first
process (`main_proc` in [`sched.c`](sched.c)), which mounts the boot RAM disk
(`/rd`), probes the floppy and IDE channels for any extra FAT volumes, starts
the framebuffer redraw thread,
the USB thread (`usb_thread` — enumerate, then poll HID endpoints and hub
ports) and, if an e1000 was found, the `net` thread (`net_thread` — run the
DHCP client, sync the RTC over NTP, then service the RX ring, retry the NFS
mount and poll the SSH listener), and finally launches the ring-3 shell
([`apps/zsh`](apps/zsh/main.c)) — servicing its `spawn` requests while it runs
and dropping to the in-kernel console (`kmain_console`) if it is missing or
exits.
