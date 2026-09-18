/**
 * @file pci_acpi.c
 * @brief PIIX4 ACPI power management (8086:7113): poweroff and reboot.
 *
 * The PIIX4 PM block has a fixed register layout, so no ACPI tables are needed.
 * The PM I/O base lives in PCI config register 0x40; PMREGMISC bit 0 enables the
 * decode. S5 (soft-off) on QEMU's PIIX4 uses SLP_TYP 0, which makes poweroff the
 * well-known `outw(pm_base + PM1_CNT, SLP_EN)`.
 */
#include <pci.h>
#include <pci_acpi.h>

#include <io.h>
#include <log.h>

#define PIIX4_PMBA      0x40    /* config: PM base address (dword, bits 15:6) */
#define PIIX4_PMREGMISC 0x80    /* config: bit 0 = PM I/O space enable        */

#define PM1_STS   0x00
#define PM1_EN    0x02
#define PM1_CNT   0x04
#define PM_TMR    0x08

#define SLP_EN    (1u << 13)
#define SLP_TYP(n) (((n) & 7u) << 10)

static uint16_t pm_base;
static int      present;

uint16_t acpi_pm_base(void) { return pm_base; }
int      acpi_present(void) { return present; }

void acpi_probe(pci_device_t *d) {
    pm_base = pci_cfg_read32(d, PIIX4_PMBA) & 0xFFC0;
    if (!pm_base) {
        pm_base = 0x600;                    /* QEMU PIIX4 default */
        pci_cfg_write32(d, PIIX4_PMBA, pm_base | 1u);
    }

    /* Enable the PM I/O decode: PMREGMISC (config 0x80) bit 0. It is the low
     * byte of that aligned dword, so a dword RMW is enough. */
    if (!(pci_cfg_read8(d, PIIX4_PMREGMISC) & 1u))
        pci_cfg_write32(d, PIIX4_PMREGMISC, pci_cfg_read32(d, PIIX4_PMREGMISC) | 1u);

    present = 1;

    uint32_t t0 = inportl(pm_base + PM_TMR) & 0xFFFFFF;
    klogf(LOG_INFO, "acpi: PIIX4 PM at I/O 0x%x (PM1_CNT 0x%x), timer=0x%x\n",
          pm_base, pm_base + PM1_CNT, t0);
}

void acpi_poweroff(void) {
    if (present) {
        outportw(pm_base + PM1_CNT, SLP_TYP(0) | SLP_EN);
        /* QEMU also accepts these fixed ports for pc / q35 / older bochs. */
        outportw(0x604,  0x2000);
        outportw(0xB004, 0x2000);
    }
    /* Fell through: not an ACPI machine, or S5 needs SCI ownership we don't
     * take. Caller (exit_qemu) continues to its own fallback. */
}

void acpi_reboot(void) {
    /* 0xCF9 reset control: SYS_RST=1 then pulse RST_CPU 0->1 for a hard reset. */
    disable_int();
    outportb(0xCF9, 0x02);
    outportb(0xCF9, 0x06);

    /* Fallback: keyboard-controller reset line. */
    while ((inportb(0x64) & 2) != 0);
    outportb(0x64, 0xFE);
    while (1)
        halt();
}
