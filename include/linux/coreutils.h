/**
 * @file coreutils.h
 * @brief A busybox-style set of coreutils / util-linux commands for the kernel
 *        debug console.
 *
 * These plug into @ref console_exec (commands.c) after the small set of
 * "native" built-ins, so every command here is reachable three ways at once:
 * the in-kernel console, the userspace shell (apps/zsh, via the `run` syscall)
 * and an SSH `shell`/`exec` channel.
 */
#pragma once

/**
 * @brief Try to run @p line as one of the coreutils commands.
 *
 * @param line Full command line; the first whitespace-delimited token is the
 *             command name.
 * @return 1 if the command name was recognised (and handled), 0 otherwise so
 *         the caller can fall through to its "not found" path.
 */
int coreutils_try(char *line);

/** @brief Print the one-line-per-command help for every coreutils command. */
void coreutils_help(void);
