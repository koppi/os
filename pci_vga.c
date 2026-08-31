/**
 * @file pci_vga.c
 * @brief QEMU / Bochs standard VGA driver: identification and Bochs DISPI
 *        mode control over the 0x01CE/0x01CF index/data ports.
 */
#include <pci.h>
#include <pci_vga.h>

#include <io.h>
#include <log.h>

#define VBE_DISPI_INDEX  0x01CE
#define VBE_DISPI_DATA   0x01CF

#define DISPI_ID         0x0
#define DISPI_XRES       0x1
#define DISPI_YRES       0x2
#define DISPI_BPP        0x3
#define DISPI_ENABLE     0x4
#define DISPI_BANK       0x5
#define DISPI_VIRT_WIDTH 0x6
#define DISPI_VIRT_HEIGHT 0x7
#define DISPI_X_OFFSET   0x8
#define DISPI_Y_OFFSET   0x9

#define DISPI_ENABLED    0x01
#define DISPI_LFB_ENABLED 0x40
#define DISPI_NOCLEARMEM 0x80

static uint32_t lfb_base;
static uint32_t mmio_base;
static int      present;

static uint16_t dispi_read(uint16_t idx) {
    outportw(VBE_DISPI_INDEX, idx);
    return inportw(VBE_DISPI_DATA);
}
static void dispi_write(uint16_t idx, uint16_t val) {
    outportw(VBE_DISPI_INDEX, idx);
    outportw(VBE_DISPI_DATA, val);
}

int      bochs_vga_present(void) { return present; }
uint32_t bochs_vga_lfb(void)     { return lfb_base; }

void bochs_vga_get_mode(uint32_t *w, uint32_t *h, uint32_t *bpp) {
    if (w)   *w   = dispi_read(DISPI_XRES);
    if (h)   *h   = dispi_read(DISPI_YRES);
    if (bpp) *bpp = dispi_read(DISPI_BPP);
}

int bochs_vga_set_mode(uint32_t w, uint32_t h, uint32_t bpp) {
    if (!present)
        return 0;
    dispi_write(DISPI_ENABLE, 0);
    dispi_write(DISPI_XRES, w);
    dispi_write(DISPI_YRES, h);
    dispi_write(DISPI_BPP,  bpp);
    dispi_write(DISPI_ENABLE, DISPI_ENABLED | DISPI_LFB_ENABLED);
    return 1;
}

void bochs_vga_probe(pci_device_t *d) {
    lfb_base  = d->bar[0].addr;
    mmio_base = d->bar[2].addr;
    present   = 1;

    uint16_t id = dispi_read(DISPI_ID);
    uint16_t en = dispi_read(DISPI_ENABLE);
    uint32_t xr, yr, bpp;
    bochs_vga_get_mode(&xr, &yr, &bpp);

    klogf(LOG_INFO, "vga: QEMU/Bochs stdvga, DISPI id 0x%x\n", id);
    klogf(LOG_INFO, "vga: LFB 0x%x (%uK), MMIO 0x%x\n",
          lfb_base, d->bar[0].size / 1024, mmio_base);
    if (en & DISPI_ENABLED)
        klogf(LOG_INFO, "vga: mode %ux%u x%u bpp (LFB %s)\n",
              xr, yr, bpp, (en & DISPI_LFB_ENABLED) ? "on" : "off");
    else
        klogf(LOG_INFO, "vga: DISPI disabled (VGA text/planar mode)\n");
}
