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
  key (persisted hex-encoded at `/hda/sshkey`, so it survives reboots),
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
| SNTP client (sets the RTC at boot) | [`ntp.c`](ntp.c) |
| Minimal TCP (client + passive-open listen/accept) | [`tcp.c`](tcp.c) |
| In-kernel NFSv4.1 client (auto-mounts `/nfs`) | [`nfs.c`](nfs.c) |
| In-kernel SSHv2 server (port 22) | [`ssh.c`](ssh.c) |
| Crypto primitives (SHA-2, bignum, Curve25519, Ed25519, AES-128, CSPRNG) | [`sha2.c`](sha2.c), [`bignum256.c`](bignum256.c), [`curve25519.c`](curve25519.c), [`ed25519.c`](ed25519.c), [`aes128.c`](aes128.c), [`csprng.c`](csprng.c) |
| QEMU / Bochs standard VGA (DISPI mode control) | [`pci_vga.c`](pci_vga.c) |
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
  `time`(14, RTC), `clock`(15, PIT) — see [`syscall.c`](syscall.c).
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
`io` or `debug`. `require` searches `/hda/?.lua` and `/fda/?.lua`.

```
start hda/lua                 -- REPL (exit with os.exit() or Ctrl-D)
start hda/lua /hda/t.lua      -- run a script; t.lua is the port's self-test
```

There is no Ctrl-C, so a non-terminating script needs a `reboot`. See
[`apps/lua/PORTING.md`](apps/lua/PORTING.md) for the full build and shim notes.

### Console shell
The kernel debug console ([`commands.c`](commands.c), `kmain_console`) runs as
the scheduler's first process. It reads keystrokes from the ring-buffered
keyboard driver, echoes them, supports backspace, and executes a line on Enter.

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
`example`, `mem`, the `lua` interpreter (with `t.lua` and `mod.lua`), and the
`mouse.bmp` cursor bitmap. The in-kernel FAT driver only
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
KVM acceleration, and a SLIRP `netdev` forwarding host port **2222** to the
guest's port 22 — once the guest has DHCP'd an address (see the boot log),
`ssh -p 2222 koppi@localhost` (password `os`) from the host reaches the
in-kernel SSH server.

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
PCI (enumerate + bind drivers) → **ACPI/MADT → Local APIC → AP bring-up** →
scheduler.

`sched_init()` does not return: it builds one idle thread per core, releases
the parked application processors and `iret`s into the scheduler's first
process (`main_proc` in [`sched.c`](sched.c)), which brings up the floppy and
IDE block devices, mounts their FAT volumes, starts the framebuffer redraw thread,
the USB thread (`usb_thread` — enumerate, then poll HID endpoints and hub
ports) and, if an e1000 was found, the `net` thread (`net_thread` — run the
DHCP client, sync the RTC over NTP, then service the RX ring, retry the NFS
mount and poll the SSH listener), and then runs the interactive console
(`kmain_console`).
