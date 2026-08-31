/**
 * @file io.c
 * @brief Interrupt-flag control, CPU halt, QEMU shutdown and tick-based sleep.
 */
#include <io.h>
#include <pit.h> // for get_tick_count()
#include <printf.h>
#include <log.h>

/** @brief Execute one @c hlt instruction. */
void halt() {
    __asm__ volatile("hlt");
}

/**
 * @brief Power the machine off (under QEMU) or reset it; does not return.
 *
 * Writes @p status_code to QEMU's `isa-debug-exit` port (0xF4) so the emulator
 * exits with that status, then issues a keyboard-controller CPU reset as a
 * fallback for real hardware, and finally spins forever.
 *
 * @param status_code Exit code for QEMU; 0 skips the debug-exit write.
 */
void __attribute__((noreturn)) exit_qemu(const int status_code) {
  if (status_code) {
    outportb(0xf4, status_code); // qemu isa-debug-exit port
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
 * @brief Coarse busy sleep measured in PIT ticks.
 *
 * Halts between polls so the CPU is not spun at full speed. The tick source is
 * reset elsewhere, so the granularity is approximate.
 *
 * @param s Number of ticks (~milliseconds at the 1 kHz PIT rate) to wait.
 */
void sleep(int s) {
    //for (int i = 0; i < s * 1000000; i++) { } return;
    //XXX if (get_tick_count() == 0) return;
    // TODO better solution
    int ticks = get_tick_count() + s;
    int passed = 0;
    while((passed += get_tick_count()) < ticks) {
        halt();
        //printf("sleep %d ...\n", passed);
    }
}
