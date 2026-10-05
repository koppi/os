# Avoid funny character set dependencies
unexport LC_ALL
LC_COLLATE=C
LC_NUMERIC=C
export LC_COLLATE LC_NUMERIC

MAKEFLAGS += --no-print-directory

TARGET ?= i386

CC=gcc
LD=ld
OBJCOPY=objcopy

SRCS = $(wildcard *.c *.S) $(wildcard lib/*.c)
# ap_boot.S is a flat binary (AP trampoline), not an ELF asm object, so it is
# excluded from the generic *.c/*.S rule and built via ap_boot_bin.o.
SRCS := $(filter-out ap_boot.S,$(SRCS))
# lib/cxx_start.c is a userspace-only ELF entry point for C++ apps (it calls
# `main`, which the kernel has no symbol named); it belongs to lib/Makefile's
# own build, not the kernel's.
SRCS := $(filter-out lib/cxx_start.c,$(SRCS))
# lib/pthread_glibc.c is a userspace-only pthread implementation built
# against the real system <pthread.h>/<sched.h> (for Qt/libstdc++ ABI
# compatibility -- see its own file comment); <sched.h> collides head-on
# with this kernel's own top-level sched.h (the in-kernel scheduler) once
# both are reachable via the same -I., so it is compiled by its own app's
# Makefile with a narrower include path instead of the kernel's.
SRCS := $(filter-out lib/pthread_glibc.c,$(SRCS))
# lib/libc_ext.c is a userspace-only libc extension (real glibc-shaped
# semaphore/setjmp/time/etc. surface for the real Qt6 graphical closure)
# built against real system <semaphore.h>/<setjmp.h>/<time.h> and
# lib/pthread_glibc.c's own clock_gettime()/__errno_location() -- same
# reasoning and same exclusion as lib/pthread_glibc.c above.
SRCS := $(filter-out lib/libc_ext.c,$(SRCS))
# lib/emutls.c, lib/tls.c, lib/libm.c and lib/pthread.c are userspace-only too (emulated TLS on
# top of the pthread shim, the real ELF TLS runtime behind the set_thread_area syscall, the x87
# libm, and the pthread-shaped shim over the thread syscalls); lib/Makefile builds them with the
# user flags. Left in SRCS they only "worked" while a stale lib/*.o from that build sat there for
# the kernel link to pick up, and broke every clean kernel build (host <string.h>,
# -pedantic-errors).
SRCS := $(filter-out lib/emutls.c lib/tls.c lib/libm.c lib/pthread.c,$(SRCS))
AP_BOOT_BIN = ap_boot_bin.o
OBJS = $(addsuffix .o,$(basename $(SRCS))) font.o $(AP_BOOT_BIN)
KERNEL = kernel.elf

ASFLAGS += -m32 -I.

CFLAGS += -O3
CFLAGS += -DDEBUG
CFLAGS += -Werror
CFLAGS += -Wall -Wextra -Wunused -Wno-pointer-to-int-cast -pedantic -pedantic-errors
CFLAGS += -m32 -std=gnu23 -pipe -fno-stack-protector
CFLAGS += -finline-functions -Wno-missing-field-initializers
CFLAGS += -fno-omit-frame-pointer -ffreestanding -fno-builtin
CFLAGS += -nodefaultlibs
#CFLAGS += -nostdlib -nostdinc -fno-builtin
CFLAGS += -I. -Iinclude

GIT_COUNT := $(shell git rev-list --count HEAD 2>/dev/null || echo 0)
GIT_HASH_TAIL := $(shell git rev-parse HEAD 2>/dev/null | sed 's/^.\{32\}//')
GIT_REV := $(shell printf '%u' 0x$(GIT_HASH_TAIL) 2>/dev/null || echo 0)
CFLAGS += -DOS_VER_MAJ=0 -DOS_VER_MIN=$(GIT_COUNT) -DOS_VER_REV=$(GIT_REV)
#CFLAGS += -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function
#CFLAGS += -Wno-type-limits -Wno-array-bounds -Wno-discarded-qualifiers
#CFLAGS += -Wno-int-conversion -Wno-sign-compare -Wno-maybe-uninitialized
#CFLAGS += -Wno-strict-aliasing

LDFLAGS += -melf_i386 -T kernel.lds -Map kernel.map -z muldefs -z noexecstack

# Networking backend for the e1000 NIC:
#   NET=user   (default) SLIRP user-mode networking. No host setup needed, but
#              it NATs the guest (10.0.2.15) and can rewrite the source port on
#              connections that leave the SLIRP-internal address space, which
#              breaks NFS mounts against a "secure" (privileged-port-only)
#              export. Fine for DHCP/DNS/HTTP/ICMP.
#   NET=bridge Bridged networking: the guest gets a real address on BRIDGE
#              (default br0) via the setuid qemu-bridge-helper, so DHCP/NFS
#              behave like a real machine on the LAN. Requires host setup:
#                1. a bridge device, e.g.
#                     sudo ip link add br0 type bridge
#                     sudo ip link set br0 up
#                     sudo ip link set <phys-if> master br0   # or use your
#                       distro's netplan/NetworkManager bridge config instead
#                2. /etc/qemu/bridge.conf containing "allow br0"
#                3. qemu-bridge-helper must be setuid root (usually already
#                   true from the qemu-utils/qemu-system-common package)
#              Then: make qemu-iso NET=bridge [BRIDGE=br0]
NET ?= user
BRIDGE ?= br0
ifeq ($(NET),bridge)
NETDEV = -netdev bridge,id=n0,br=$(BRIDGE) -device e1000,netdev=n0
else
    NETDEV = -netdev user,id=n0,hostfwd=tcp::2222-:22 -device e1000,netdev=n0
endif

QEMU ?= qemu-system-$(TARGET)
# Number of CPUs. -smp 4 is the tested default: the whole userland, including
# repeated self-hosting builds of the cc compiler (compute- and syscall-heavy),
# runs reliably under it. Set SMP=1 only to bisect a suspected SMP regression.
SMP ?= 4
# -vga virtio: the modern virtio-gpu 2D device (virtio-vga: keeps VBE for the
# GRUB hand-off). virtio_gpu.c drives scanout 0 from the framebuffer shadow and
# follows the SDL/GTK window as it is resized, re-moding the desktop to match.
QEMUFLAGS += -vga virtio -m 512M -no-reboot
QEMUFLAGS += -smp $(SMP)
QEMUFLAGS += -device isa-debug-exit,iobase=0xf4,iosize=0x04
QEMUFLAGS += -enable-kvm
QEMUFLAGS += -audiodev id=pa,driver=pa -machine pcspk-audiodev=pa
QEMUFLAGS += -device sb16,audiodev=pa
QEMUFLAGS += -device ac97,audiodev=pa
# Intel HD Audio (Azalia): the audio path on a real laptop. When present the
# MOD player routes through it instead of the Sound Blaster (see sb16.c).
QEMUFLAGS += -device intel-hda -device hda-output,audiodev=pa
#QEMUFLAGS += -d in_asm,cpu,guest_errors,exec
QEMUFLAGS += -rtc base=localtime,clock=vm
# The root filesystem now rides inside os.iso as a Multiboot module (initrd.img),
# so no hd/floppy image is needed to boot. Set DISK=<file> for a persistent
# scratch disk (it shows up inside the OS as /hda); FLOPPY=<file> likewise (/fda).
QEMUFLAGS += -drive file=os.iso,if=ide,index=1,media=cdrom
ifneq ($(DISK),)
QEMUFLAGS += -drive file=$(DISK),format=raw,if=ide,index=0,media=disk
endif
ifneq ($(FLOPPY),)
QEMUFLAGS += -drive file=$(FLOPPY),format=raw,index=0,if=floppy
endif
QEMUFLAGS += -display sdl
QEMUFLAGS += -usb
QEMUFLAGS += -device usb-kbd,port=1
QEMUFLAGS += -device usb-hub,port=2 -device usb-mouse,port=2.1
QEMUFLAGS += $(NETDEV)

all: lib apps $(KERNEL)

lib:
	$(MAKE) -C lib

apps:
	$(MAKE) -C apps

# The boot RAM disk: the whole userland, packed into a FAT16 image that GRUB
# hands to the kernel as a Multiboot module. hda.sh picks the size -- 8 MiB is
# plenty for the payload plus a few generations of cc output (it is RAM-backed
# and ephemeral), 16 MiB when a Doom IWAD is being staged with it.
initrd.img: hda.sh apps $(wildcard doom1.wad)
	@echo "  IMG initrd.img"
	@IMG=initrd.img ./hda.sh >/dev/null

iso: $(KERNEL) initrd.img
	@mkdir -p iso/boot/grub
	@cp $(KERNEL) iso/boot/$(KERNEL)
	@cp initrd.img iso/boot/initrd.img
	@cp grub.cfg iso/boot/grub/grub.cfg
	@grub-mkrescue -o os.iso iso 1>&2 2>/dev/null

qemu-kernel: $(KERNEL) initrd.img
	@echo "QEMU .."
	@$(QEMU) -kernel $(KERNEL) -initrd initrd.img $(QEMUFLAGS) -display none -serial 'mon:stdio'

qemu-iso: iso
	@echo "QEMU .."
	@$(QEMU) $(QEMUFLAGS) -boot d,menu=off -serial 'mon:stdio'

qemu-nox: iso
	@echo "QEMU .."
	@$(QEMU) $(QEMUFLAGS) -boot d,menu=off -display none -serial 'mon:stdio'

# Boot os.iso in X250-shaped QEMU configs (q35 + AHCI + xHCI + e1000e) under
# both SeaBIOS and OVMF/UEFI; serial log + screenshot per config in /tmp/x250-boot.
qemu-x250: iso
	@bash test/x250-boot.sh all

# Boot os.iso in T470s-shaped QEMU configs (q35 + NVMe + xHCI + e1000e + HD
# Audio) under both SeaBIOS and OVMF/UEFI; logs + screenshots in /tmp/t470s-boot.
qemu-t470s: iso
	@bash test/t470s-boot.sh all

# Boot the kernel (via -kernel, with `ehci` on the command line) in X220-shaped
# QEMU configs (BIOS/CSM + AHCI + EHCI USB — no xHCI — + an Intel e1000 NIC);
# logs + screenshots in /tmp/x220-boot.
qemu-x220: $(KERNEL) initrd.img
	@bash test/x220-boot.sh all

# Boot os.iso in MacBook Air 2013-shaped QEMU configs (q35 + xHCI-only + QXL
# GOP-like video, UEFI/OVMF only); logs + screenshots in /tmp/mba-boot.
qemu-mba: iso
	@bash test/mba-boot.sh all

# Log into the in-kernel SSH server with a stock OpenSSH client and check its
# shell's line editor (needs ssh + sshpass on the host); logs in /tmp/ssh-boot.
qemu-ssh: iso
	@bash test/ssh-boot.sh

# Check the raw key stream (what a full-screen program reads) on all three
# keyboard paths -- i8042, USB HID over UHCI, USB HID over xHCI with no i8042 at
# all, which is the MacBook Air's topology. Needs no WAD; logs in /tmp/keys-boot.
qemu-keys: iso
	@bash test/keys-boot.sh all

# Drive apps/doom headless in QEMU: start the game over the serial console,
# send keystrokes through the monitor, screenshot each step into /tmp/doom-boot.
# Needs a WAD staged on the RAM disk (apps/doom/PORTING.md).
qemu-doom: iso
	@bash test/doom-boot.sh all

# Drive apps/chipnomad the same way: start the tracker over the serial console,
# walk its screens, load a song, play it into a WAV, screenshot each step.
qemu-chipnomad: iso
	@bash test/chipnomad-boot.sh all

# Drive the ring-3 microui apps (apps/calc, apps/clock): start each over the
# serial console, type at the calculator, click its keypad and the clock's
# options with the monitor's mouse, check Esc gives the desktop back.
# Screenshots and the apps' own log lines per step in /tmp/microui-boot.
qemu-microui: iso
	@bash test/microui-boot.sh all

# Drive apps/hello-qt-gui (the real Qt 6.8 window) and judge it from pixels:
# right shape and gradient, the ball animates, Esc returns the desktop. Needs
# the app built first: make -C apps/hello-qt-gui -j4
qemu-qt-gui: iso
	@bash test/qt-gui-boot.sh all

# Drive apps/hello-qt-widgets (real QtWidgets) with the keyboard and the mouse and
# judge it from the app's own serial log: focus chain, buttons, sliders, typing,
# the combo-box popup, a run-time style switch, Quit. Needs the app built first:
# make -C apps/hello-qt-widgets -j4
qemu-qt-widgets: iso
	@bash test/qt-widgets-boot.sh all

# Build a GPT+FAT32 UEFI USB image (os-usb.img) that the UEFI-only laptops
# (no CSM, e.g. a MacBook) boot from: grub2 x86_64-efi + kernel + initrd.
usb: $(KERNEL) initrd.img
	@bash ./make-usb.sh

$(KERNEL): $(OBJS)
	@echo "  LD $@"
	@$(LD) $(LDFLAGS) -o $@ $^

.c.o:
	@echo "  CC $<"
	@$(CC) -MD $(CFLAGS) -o $@ -c $<

.S.o:
	@echo "  CC $<"
	@$(CC) -MD $(ASFLAGS) -o $@ -c $<

# AP trampoline: assemble to a flat binary and wrap it as an ELF object so the
# linker embeds it; C accesses it via _binary_ap_boot_bin_start/_end/_size.
ap_boot_bin.o: ap_boot.S ap_boot.ld
	@echo "  AS ap_boot.S"
	@$(CC) $(ASFLAGS) -o ap_boot.tmp.o -c ap_boot.S
	@$(LD) -m elf_i386 -T ap_boot.ld -o ap_boot.tmp.elf ap_boot.tmp.o
	@$(OBJCOPY) -O binary ap_boot.tmp.elf ap_boot.bin
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 ap_boot.bin $@
	@rm -f ap_boot.tmp.o ap_boot.tmp.elf

font.o:
	@$(OBJCOPY) -O elf32-i386 -B i386 -I binary unifont.sfn font.o

kernel.lst: $(KERNEL)
	objdump -D $(KERNEL) > kernel.lst

cloc::
	cloc . --exclude-ext=md,txt,toml,json

docs::
	doxygen Doxyfile

clean::
	@$(MAKE) -C lib clean
	@$(MAKE) -C apps clean
	@rm -rf $(KERNEL) kernel.lst kernel.map $(OBJS) ap_boot.bin ap_boot.tmp.* *.d lib/*.d *~ os.iso iso initrd.img docs

.PHONY: all lib apps iso qemu-kernel qemu-iso qemu-nox qemu-x250 qemu-t470s qemu-x220 qemu-mba qemu-keys qemu-ssh qemu-doom qemu-chipnomad qemu-microui qemu-qt-gui qemu-qt-widgets cloc docs clean

-include $(OBJS:.o=.d)
