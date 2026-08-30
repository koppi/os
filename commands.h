#pragma once

/** Parse and execute a single console command line. */
void console_exec(char *buf);

/** Interactive read-eval-print loop for the kernel debug console. Never returns. */
void kmain_console(void);

