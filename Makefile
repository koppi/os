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

SRCS = $(wildcard *.[cS] *.asm) $(wildcard lib/*.c)
# ap_boot.asm is a flat binary (AP trampoline), not an ELF asm object, so it is
# excluded from the generic *.[cS]/*.asm rule and built via ap_boot_bin.o.
SRCS := $(filter-out ap_boot.asm,$(SRCS))
AP_BOOT_BIN = ap_boot_bin.o
OBJS = $(addsuffix .o,$(basename $(SRCS))) font.o $(AP_BOOT_BIN)
KERNEL = kernel.elf

ASFLAGS += -m32 -I.

CFLAGS += -Og
CFLAGS += -DDEBUG
CFLAGS += -Werror
CFLAGS += -Wall -Wextra -Wunused -Wno-pointer-to-int-cast #-pedantic -pedantic-errors
CFLAGS += -m32 -std=gnu11 -pipe -fno-stack-protector
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

QEMU ?= qemu-system-$(TARGET)
QEMUFLAGS += -vga std -m 256M -no-reboot
QEMUFLAGS += -device isa-debug-exit,iobase=0xf4,iosize=0x04
QEMUFLAGS += -enable-kvm
QEMUFLAGS += -audiodev id=pa,driver=pa -machine pcspk-audiodev=pa
QEMUFLAGS += -device sb16,audiodev=pa
QEMUFLAGS += -device ac97,audiodev=pa
#QEMUFLAGS += -d in_asm,cpu,guest_errors,exec
QEMUFLAGS += -rtc base=localtime,clock=vm
QEMUFLAGS += -drive file=floppy.img,format=raw,index=0,if=floppy
QEMUFLAGS += -drive file=hda.img,format=raw,if=ide,index=0,media=disk
QEMUFLAGS += -drive file=os.iso,if=ide,index=1,media=cdrom
QEMUFLAGS += -display sdl
QEMUFLAGS += -usb
QEMUFLAGS += -device usb-kbd,port=1
QEMUFLAGS += -device usb-hub,port=2 -device usb-mouse,port=2.1

all: lib apps $(KERNEL)

lib:
	$(MAKE) -C lib

apps:
	$(MAKE) -C apps

iso: $(KERNEL)
	@mkdir -p iso/boot/grub
	@cp $(KERNEL) iso/boot/$(KERNEL)
	@cp grub.cfg iso/boot/grub/grub.cfg
	@grub-mkrescue -o os.iso iso 1>&2 2>/dev/null

qemu-kernel: $(KERNEL)
	@echo "QEMU .."
	@$(QEMU) -kernel $(KERNEL) $(QEMUFLAGS) -display none -serial 'mon:stdio'

qemu-iso: iso
	@echo "QEMU .."
	@$(QEMU) $(QEMUFLAGS) -boot d,menu=off -serial 'mon:stdio'

qemu-nox: iso
	@echo "QEMU .."
	@$(QEMU) $(QEMUFLAGS) -boot d,menu=off -display none -serial 'mon:stdio'

$(KERNEL): $(OBJS)
	@echo "  LD $@"
	@$(LD) $(LDFLAGS) -o $@ $^

.c.o:
	@echo "  CC $<"
	@$(CC) -MD $(CFLAGS) -o $@ -c $<

.S.o:
	@echo "  CC $<"
	@$(CC) -MD $(ASFLAGS) -o $@ -c $<

%.o: %.asm
	@echo "NASM $<"
	@nasm -f elf -o $@ $^

# AP trampoline: assemble to a flat binary and wrap it as an ELF object so the
# linker embeds it; C accesses it via _binary_ap_boot_bin_start/_end/_size.
ap_boot_bin.o: ap_boot.asm
	@echo "NASM -f bin ap_boot.asm"
	@nasm -f bin -o ap_boot.bin ap_boot.asm
	@$(OBJCOPY) -I binary -O elf32-i386 -B i386 ap_boot.bin $@

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
	@rm -rf $(KERNEL) kernel.lst kernel.map $(OBJS) ap_boot.bin *.d lib/*.d *~ os.iso iso docs

.PHONY: all lib apps iso qemu-kernel qemu-iso qemu-nox cloc docs clean

-include $(OBJS:.o=.d)
