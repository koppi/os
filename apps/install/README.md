# install: the graphical installer

A wizard, in ring 3, drawn with [microui](../microui), that puts koppi's os on a disk of
the machine it is running on: it partitions the disk, installs GRUB so the machine boots
from it on **BIOS and UEFI alike**, and copies the system across.

```
make iso                  # builds os.iso, with the installer and its payload on it
make qemu-install         # boots it with a blank scratch disk (install-target.img);
                          #   at the boot menu choose "Install koppi's os to a disk"
make qemu-installed       # then starts from the disk the installer wrote, no CD
make test-install-host    # the engine over image files (tools/testing/install-host-test.sh)
make qemu-install-test    # the whole thing, unattended (tools/testing/install-boot.sh; BUS=ahci|nvme|ide|all)
```

On real hardware: write `os.iso` to a USB stick (`dd`), boot it, choose the "Install" entry. The
target must be a disk the kernel has a driver for: IDE, SATA (AHCI) or NVMe. A USB drive cannot be
installed onto -- there is no USB mass-storage driver -- but the stick the installer booted from
never needs one, since GRUB has already loaded everything into memory. `install` at the shell opens
the same wizard as a window on the desktop, but only the Install boot entry loads the payload, so
there it says so and stops at the welcome page.

The pages: **Welcome** → **Disk** (every installable disk, with what is on it now) →
**Settings** (size of the system volume, its name) → **Review** (what will be erased, and a box to
tick) → **Install** (progress; Esc and the close box are ignored from here on) →
**Finish** (restart). Enter is Next, Space ticks the box, Esc quits.

## What it writes

The layout `grub-install` makes on a GPT disk:

```
LBA 0        protective MBR, whose boot code is GRUB's boot.img
LBA 1..33    GPT header + 128 entries            (backup copy at the end of the disk)
LBA 2048     p1  BIOS boot, 1 MiB     GRUB's core.img, embedded           -> BIOS
LBA 4096     p2  EFI System, FAT16    /EFI/BOOT/BOOTX64.EFI                -> UEFI
                                      /boot/kernel.elf
                                      /boot/initrd.img                     the RAM disk, as it was booted
                                      /boot/grub/grub.cfg, koppios.id, fonts/ascii.pf2
```

FAT16, because that is what the kernel's own FAT driver mounts: once the installed system
is running, p2 is `/hda`, its persistent storage (the root file system is still the RAM
disk, as on the ISO). A FAT16 volume holds at most 2 GiB; a larger disk's remainder is left
unpartitioned.

The boot sector is written **last**, so an interrupted install leaves a disk that does not
try to boot, and everything is **read back and checksummed** before it says "done". The
kernel is then asked to rescan the disk and mount the new volume, which checks the engine's
work against an independent implementation of the same formats (`fat.c`).

## Where the files come from

There is no CD or USB driver, so everything is carried in memory by GRUB, as Multiboot
modules (the "Install" entry of [`grub.cfg`](../../arch/x86/boot/grub/grub.cfg)):

* `initrd.img`, the RAM disk, is **copied sector for sector** from the `rd` block device: the
  installed system gets exactly the userland that was running.
* `payload.img` (built by [`scripts/payload.sh`](../../scripts/payload.sh)) is a second, small FAT16
  image: the kernel, GRUB's `boot.img`, `core.img` and `BOOTX64.EFI` with
  [`embed.cfg`](../../arch/x86/boot/grub/embed.cfg) baked in, the installed `grub.cfg` and a font. It
  is loaded only by the Install entry, so ordinary boots pay nothing for it. The kernel keeps it
  behind the RAM disk and shows it to ring 3 as a read-only block device.

The installed GRUB finds its volume by a marker file (`/boot/grub/koppios.id`), not by a
device name, because the firmware may number the disks differently next boot.

## The pieces

| file | |
| --- | --- |
| `engine.c`, `engine.h` | all the work: GPT, FAT16, the GRUB patch points, the copy, the read-back. Knows nothing about the kernel or the screen; sector devices are tables of functions |
| `sys.c`, `sys.h` | the ring-3 side of those: the block syscalls ([`blk.h`](../../include/linux/blk.h)), randomness, reboot |
| `main.c` | the wizard |
| `host.c` | the same engine on the build machine, over image files: `tools/testing/install-host-test.sh` |

The engine is a state machine: `inst_step()` does about 128 KiB of I/O and returns, so the
interface keeps drawing (and answering the mouse) while a disk is written.

## Kernel support

* **Block syscalls 46–49** ([`block/blk.c`](../../block/blk.c)): list the disks, read and write
  sectors, flush, rescan. Only whole disks the kernel can drive may be written — never the RAM
  disk, the payload or a floppy — and only inside the disk. The console command `lsblk` lists
  them.
* **Partitions**: the FAT driver now looks inside an MBR or a GPT for a FAT volume, so a
  partitioned disk mounts as `/hda` ([`fs/fat/fat.c`](../../fs/fat/fat.c)).
* **Bulk writes** skip the per-sector cache flush the drivers used to do (a flush per sector is
  invisible in an emulator and ruinous on an SSD) and flush once at the end.
* The `install` word on the kernel command line starts this program instead of the shell.

## Testing

`tools/testing/install-host-test.sh` runs the engine on image files and checks them with `sfdisk`,
`fsck.fat` and QEMU's SeaBIOS and OVMF. `tools/testing/install-boot.sh [ahci|nvme|ide|all]` drives the
real wizard in QEMU, then checks the disk from the host and boots it alone. Speed: AHCI and NVMe
use DMA and take about 15 seconds for the 32 MiB RAM disk image (written, then read back); an IDE disk is
PIO, and under KVM every word of port I/O traps to the emulator, so the same install takes minutes
there (real IDE hardware does not pay that). `make qemu-install` therefore attaches its disk
through AHCI. What neither test can
tell: whether a particular real machine's firmware accepts the layout; that has to be tried.
