/**
 * @file pci_acpi.h
 * @brief Minimal power management via the PIIX4 ACPI function (8086:7113).
 *
 * No AML interpreter: the PIIX4 PM registers have a fixed layout and QEMU's S5
 * sleep type is 0, so poweroff is a single word write to PM1_CNT. Reboot goes
 * through the 0xCF9 reset-control port.
 */
#pragma once

#include <types.h>

struct pci_device;

/** @brief Latch the PM I/O base from config space and enable PM decode. */
void acpi_probe(struct pci_device *d);

/** @return Non-zero once @ref acpi_probe has run successfully. */
int acpi_present(void);

/** @return The PM register I/O base (0 if not initialised). */
uint16_t acpi_pm_base(void);

/**
 * @brief Enter ACPI S5 (soft off). Returns only if it had no effect (e.g. not
 *        running under an ACPI-capable machine), so callers can fall back.
 */
void acpi_poweroff(void);

/** @brief Hard-reset the machine through the 0xCF9 reset-control register. */
void acpi_reboot(void);
