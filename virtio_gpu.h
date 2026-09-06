/**
 * @file virtio_gpu.h
 * @brief virtio-gpu (1af4:1050, as exposed by QEMU's `-vga virtio`): a 2D
 *        scanout backed by the framebuffer shadow, and a poll thread that
 *        follows the host window as the user resizes it.
 *
 * The boot path is unchanged: GRUB's VBE framebuffer (the "VGA" half of
 * virtio-vga) brings the console up in [video.c](video.c). @ref
 * virtio_gpu_probe then attaches the 32-bpp shadow surface as a virtio-gpu
 * resource and points scanout 0 at it, so every frame is presented with a
 * TRANSFER_TO_HOST_2D + RESOURCE_FLUSH instead of a copy to the linear
 * framebuffer. @ref virtio_gpu_thread polls GET_DISPLAY_INFO a few times a
 * second; when the host's preferred size changes (an SDL/GTK window resize,
 * relayed to the guest by QEMU) it switches the desktop to the new mode in
 * place via @ref video_set_geometry.
 */
#pragma once

#include <types.h>

struct pci_device;

/** @brief Bind the device: negotiate features, set up the control queue,
 *         create the scanout resource and install the present hook. */
void virtio_gpu_probe(struct pci_device *d);

/** @return Non-zero once the scanout is live (the present hook is installed). */
int virtio_gpu_active(void);

/** @brief Kernel thread: track the host window size and re-mode on change.
 *         Parks itself when no virtio-gpu was found. */
void virtio_gpu_thread(void);
