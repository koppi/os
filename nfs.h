/**
 * @file nfs.h
 * @brief In-kernel NFSv4.1 client, exported through the VFS as the "/nfs" mount.
 *
 * Speaks NFSv4.1 (RFC 8881) over the minimal TCP stack in [tcp.c](tcp.c):
 * EXCHANGE_ID + CREATE_SESSION, a SEQUENCE-wrapped COMPOUND per operation,
 * OPEN/CLOSE with real stateids, READ/WRITE, READDIR, GETATTR, REMOVE.
 *
 * Everything talks to the socket from the `net` thread. The boot-time
 * auto-mount runs directly in @ref net_thread via @ref nfs_boot_tick; console
 * driven calls are marshalled onto the `net` thread with @ref net_exec (see
 * vfs.c). One operation is in flight at a time (single session slot).
 */
#pragma once

#include <types.h>
#include <vfs.h>

/** Server and export mounted automatically once DHCP yields an address. */
#define NFS_HOST        "cube00.fritz.box"
#define NFS_EXPORT      "/nfs"
/** First path component under '/' that the VFS routes to this client. */
#define NFS_MOUNTPOINT  "nfs"
#define NFS_PORT        2049

/** Sentinel @ref file::dev value marking an NFS-backed handle (outside the
 *  block-device id range used by device.c). */
#define NFS_DEV_ID      25

/** @brief Called every iteration of the `net` thread: (re)tries the auto-mount
 *  of @ref NFS_HOST : @ref NFS_EXPORT until it succeeds. */
void nfs_boot_tick(void);

/** @return Non-zero once the export is mounted and usable. */
int nfs_is_mounted(void);

/** @return Non-zero if @p name (with or without a leading '/') has
 *  @ref NFS_MOUNTPOINT as its first component. */
int nfs_owns_path(const char *name);

/** @brief One-line human readable mount state, for the `nfs` console command. */
const char *nfs_status_str(void);

/* ---- VFS operation vector (called from vfs.c, never with fs_lock held) ---- */

void nfs_vfs_ls(char *dir);
file nfs_vfs_cd(char *dir);
file nfs_vfs_open(char *name, const char *mode);
void nfs_vfs_read(file *f, char *buf);
void nfs_vfs_write(file *f, char *str);
void nfs_vfs_close(file *f);
int  nfs_vfs_touch(char *name);
int  nfs_vfs_delete(char *name);
