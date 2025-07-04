# koppi's hobby OS

A hobby operating system for the i386 architecture.

## Features

*   **Bootloader:** GRUB
*   **Kernel:** 32-bit kernel written in C
*   **Memory Management:** Paging, heap, and physical memory manager
*   **Filesystems:** FAT
*   **Drivers:**
    *   ATA
    *   Floppy
    *   Keyboard
    *   Mouse
    *   PCI
    *   AC97 Audio
    *   VGA
    *   PIT
    *   RTC
*   **Userspace:** Simple shell and a few applications

## Building and Running

### Prerequisites

```bash
sudo apt -y install grub-common xorriso mtools nasm gcc-multilib qemu-system-x86 grub-pc-bin
```

### Build

To build the kernel and create the bootable ISO image, run:

```bash
make
```

This will create the `os.iso` file.

### Run

To run the OS in QEMU, use:

```bash
make qemu-iso
```

## Cleaning up

To remove all build artifacts, run:

```bash
make clean
```