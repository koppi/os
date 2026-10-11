/**
 * @file apps/install/sys.h
 * @brief The installer's window on the kernel: block devices, randomness, restart.
 *
 * Everything the engine and the interface need from outside the process, in one
 * place -- the block syscalls of include/linux/blk.h, `getrandom`, `time` and
 * `run("reboot")`. The engine itself sees none of this, only @ref inst_dev.
 */
#pragma once

#include <linux/blk.h>
#include "engine.h"

/** @brief Describe block device @p idx. @return 1 if there is one, else 0. */
int sys_blk_info(unsigned idx, struct blk_info *bi);

/** @brief Fill @p d to read (and, if @p writable, write) block device @p idx. */
void sys_blk_dev(inst_dev *d, unsigned idx, unsigned sectors, int writable);

/** @brief Rescan the partitions of device @p idx and re-mount it. @return 1 if a FAT volume mounted. */
int sys_blk_rescan(unsigned idx);

/** @brief Fill @p buf with @p n random bytes (the kernel's CSPRNG). */
void sys_random(void *buf, unsigned n);

/** @brief Seconds since the Unix epoch, from the real-time clock. */
unsigned sys_unix(void);

/** @brief Ask the kernel to restart the machine. Does not return when it works. */
void sys_reboot(void);
