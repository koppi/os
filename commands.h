/**
 * @file commands.h
 * @brief The kernel debug shell — command dispatch and the input loop.
 */
#pragma once

#include <types.h>

/** @brief Parse and execute a single console command line. */
void console_exec(char *buf);

/** @brief Locate the @p n-th space-separated argument of a command line (arg 0
 *         is the command word). The result points into @p command and is not
 *         clipped at the token end. @return NULL if there are fewer args. */
char *get_argument(char *command, int n);

/**
 * @brief Resolve a user-supplied name to an absolute device path.
 *
 * A name containing '/' is taken as device-qualified ("hda/x" -> "/hda/x"); a
 * bare name resolves against the console working directory, falling back to the
 * default device. Only the first whitespace-delimited token of @p name is used.
 *
 * @return non-zero on success, 0 if the path did not fit in @p out.
 */
int console_resolve_path(char *out, size_t outsz, const char *name);

/**
 * @brief The console working directory as an absolute path ("/" at the root).
 *
 * Exposed for a userspace shell (apps/zsh) to render in its prompt: it is the
 * same buffer @ref console_exec's "cd" handler maintains, so both agree on what
 * a relative path resolves against.
 */
const char *console_cwd(void);

/**
 * @brief Load and run a program on behalf of a ring-3 shell, blocking until it
 *        exits. Marshalled onto the init thread (see @ref console_spawn_service)
 *        because the ELF loader must run on the kernel page directory.
 * @return the child's exit status, or -1 if it could not be started.
 */
int console_spawn_request(const char *path, const char *args);

/** @brief Run one pending @ref console_spawn_request; call from the init loop. */
void console_spawn_service(void);

/** @brief Interactive read-eval-print loop for the debug console; never returns. */
void kmain_console(void);

