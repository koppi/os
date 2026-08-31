/**
 * @file commands.h
 * @brief The kernel debug shell — command dispatch and the input loop.
 */
#pragma once

/** @brief Parse and execute a single console command line. */
void console_exec(char *buf);

/** @brief Interactive read-eval-print loop for the debug console; never returns. */
void kmain_console(void);

