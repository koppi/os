/**
 * @file vfs.h
 * @brief Virtual filesystem: an open-file handle, a per-filesystem operation
 *        vector and the device-addressed VFS calls the shell and loader use.
 */
#pragma once

#include <types.h>

/** Max devices addressable by id. */
#define MAX_DEVICES 26

/** An open file (or directory) handle. */
typedef struct {
    char name[32];              /**< Leaf name. */
    uint32_t flags;             /**< Open flags (unused). */
    uint32_t len;               /**< File length in bytes. */
    uint32_t eof;               /**< Set once the last cluster has been read. */
    uint32_t dev;               /**< Owning device id. */
    uint32_t current_cluster;   /**< Cluster the next read starts from. */
    uint32_t type;              /**< @ref FS_FILE / @ref FS_DIR / @ref FS_NULL. */
} file;

/** Per-filesystem operation vector (FAT fills this in). */
typedef struct {
    void (*mount) ();                    /**< Mount the volume. */
    void (*read) (file *f, char *str);   /**< Read one unit into @c str. */
    void (*write) (file *f, char *str);  /**< Write @c str. */
    void (*close) (file *f);             /**< Close @c f. */
    file (*open) (char *name);           /**< Open a path. */
    void (*ls) (char *dir);              /**< List a directory. */
    file (*cd) (char *dir);              /**< Resolve a directory path. */
    int (*touch) (char *name);           /**< Create an empty file. */
    int (*delete) (char *name);          /**< Delete a file. */
    void (*write_all) (file *f, char *buf, uint32_t len); /**< Replace contents. */
} filesystem;

#define FS_FILE     0  /**< file.type: regular file. */
#define FS_DIR      1  /**< file.type: directory. */
#define FS_NULL     2  /**< file.type: does not exist / open failed. */

/** @brief Clear the device table. */
void vfs_init();
/** @brief List mounted devices (at the root). */
void vfs_ls();
/** @brief List the directory named by @p dir ("dev/..."). */
void vfs_ls_dir(char *dir);
/** @brief Validate a directory path. @return non-zero if it is a directory. */
int vfs_cd(char *name);
/** @brief Create an empty file at path @p name. */
int vfs_touch(char *name);
/** @brief Delete the file at path @p name. */
int vfs_delete(char *name);
/** @brief Open a kernel-side file handle for @p name. */
file *vfs_file_open(char *name, char *mode);
/** @brief Open a file handle allocated on the calling process's user heap. */
file *vfs_file_open_user(char *name, char *mode);
/** @brief Read one unit of @p f into @p str. */
void vfs_file_read(file *f, char *str);
/** @brief Write @p str to @p f. */
void vfs_file_write(file *f, char *str);
/** @brief Create/truncate the file at path @p name and write @p len bytes of
 *         @p buf. @return bytes written, or -1 on failure. */
int vfs_spit(char *name, char *buf, uint32_t len);
/** @brief Close and free a kernel-side handle. */
void vfs_file_close(file *f);
/** @brief Close and free a user-heap handle. */
void vfs_file_close_user(file *f);
/** @brief (legacy) map a "hd\\"/"fd\\" prefix to a device number. */
int vfs_get_dev(char *name);
/** @brief Mount the filesystem on device @p name. */
void vfs_mount(char *name);
/** @brief Unmount device @p name. */
void vfs_unmount(char *name);
