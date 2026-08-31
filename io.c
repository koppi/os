/**
 * @file io.c
 * @brief Interrupt-flag control, CPU halt, QEMU shutdown and tick-based sleep.
 */
#include <io.h>
#include <pit.h> // for get_tick_count()
#include <printf.h>
#include <log.h>
#include <pci_acpi.h>

/** @brief Execute one @c hlt instruction. */
void halt() {
    __asm__ volatile("hlt");
}

/**
 * @brief Power the machine off (under QEMU) or reset it; does not return.
 *
 * A non-zero @p status_code is written to QEMU's `isa-debug-exit` port (0xF4) so
 * the emulator exits with that status; @p status_code 0 is a clean shutdown and
 * goes through @ref acpi_poweroff (guest-initiated ACPI S5). Either way it then
 * issues a keyboard-controller CPU reset as a fallback for real hardware, and
 * finally spins forever.
 */
void __attribute__((noreturn)) exit_qemu(const int status_code) {
  if (status_code) {
    outportb(0xf4, status_code); // qemu isa-debug-exit port
  } else {
    // Clean shutdown: prefer a real guest-initiated ACPI S5 power-off; falls
    // through if there is no PIIX4 ACPI (e.g. non-QEMU hardware).
    acpi_poweroff();
  }
  disable_int();
  while ((inportb(0x64) & 2) != 0);
  outportb(0x64, 0xd1);
  while ((inportb(0x64) & 2) != 0);
  outportb(0x60, 0xfe); // keyboard reset

  // Busy-wait halt.
  while(1);
}

/** @brief Enable maskable interrupts (`sti`) and emit a log line. */
void enable_int() {
    klogf(LOG_INFO, "Interrupts are enabled.\n");
    __asm__ volatile("sti");
}

/** @brief Disable maskable interrupts (`cli`) and emit a log line. */
void disable_int() {
    klogf(LOG_INFO, "Interrupts are disabled.\n");
    __asm__ volatile("cli");
}

/**
 * @brief Coarse busy sleep of @p s milliseconds.
 *
 * Halts between polls so the CPU is not spun at full speed. Uses the
 * free-running PIT ms clock (@ref pit_ms), which is unaffected by the
 * scheduler's per-CPU tick counter.
 */
void sleep(int s) {
    //for (int i = 0; i < s * 1000000; i++) { } return;
    // TODO better solution
    uint32_t target = pit_ms() + (uint32_t) s;
    while ((int32_t) (pit_ms() - target) < 0) {
        halt();
    }
}
