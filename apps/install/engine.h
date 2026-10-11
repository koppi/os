/**
 * @file apps/install/engine.h
 * @brief The installer's engine: lays a bootable koppi-os out on a disk.
 *
 * Nothing in here knows about the kernel, the screen or the C library. The disk it
 * writes, the boot payload it reads from and the RAM disk it clones are all
 * @ref inst_dev -- a table of sector-granular read/write functions -- so the same
 * code runs in ring 3 against the real block syscalls (main.c, sys.c) and on the
 * build machine against plain image files (host.c). That second use is what makes it
 * testable: tools/testing/install-host-test.sh installs into a file, then lets
 * sfdisk, fsck.fat and QEMU's SeaBIOS and OVMF judge the result.
 *
 * The result is the layout `grub-install` makes on a GPT disk, so it boots on both
 * kinds of firmware:
 *
 * @code
 *   LBA 0        protective MBR, whose boot code is GRUB's boot.img
 *   LBA 1..33    GPT header + 128 entries            (backup copy at the end)
 *   LBA 2048     p1  BIOS boot, 1 MiB     GRUB's core.img, embedded
 *   LBA 4096     p2  EFI System, FAT16    /EFI/BOOT/BOOTX64.EFI   (UEFI)
 *                                         /boot/kernel.elf
 *                                         /boot/initrd.img         (the RAM disk)
 *                                         /boot/grub/grub.cfg ...
 * @endcode
 *
 * FAT16, not FAT32, because it is the one the kernel's own FAT driver mounts: after
 * the first boot the same partition is /hda, the OS's persistent storage.
 *
 * The work is a state machine. @ref inst_step does a bounded amount of it (about
 * 128 KiB of disk I/O) and returns, so a GUI can call it from its frame loop and
 * keep drawing; a command-line host just calls it until it stops returning 1.
 */
#pragma once

typedef unsigned char      inst_u8;
typedef unsigned short     inst_u16;
typedef unsigned int       inst_u32;
typedef unsigned long long inst_u64;

/** A sector device: the target disk, the boot payload, or the RAM disk. */
typedef struct {
    void *ctx;
    /** Read @p n 512-byte sectors starting at @p lba. @return 0 on success. */
    int (*read)(void *ctx, inst_u32 lba, void *buf, inst_u32 n);
    /** Write @p n sectors. May be NULL for a read-only device. @return 0 on success. */
    int (*write)(void *ctx, inst_u32 lba, const void *buf, inst_u32 n);
    /** Push written sectors to the medium. May be NULL. @return 0 on success. */
    int (*flush)(void *ctx);
    inst_u32 sectors;               /**< Size in 512-byte sectors. */
} inst_dev;

/** What the user chose. */
typedef struct {
    inst_u32 part_mib;              /**< System volume size in MiB; 0 means as large as FAT16 allows. */
    char     label[12];             /**< FAT volume label (upper-cased, space-padded). */
    inst_u32 unix_time;             /**< Stamp for the files (seconds since 1970; 0 = 2000-01-01). */
    /** Fill @p buf with @p n random bytes (disk GUIDs, volume serial). */
    void (*random)(void *buf, inst_u32 n);
} inst_opts;

/** Where the installer is. */
typedef enum {
    INST_IDLE,          /**< nothing planned yet */
    INST_PLANNED,       /**< laid out, nothing written */
    INST_GPT,           /**< writing the partition tables */
    INST_CORE,          /**< embedding GRUB's core.img */
    INST_FORMAT,        /**< clearing the new volume's metadata */
    INST_COPY,          /**< copying the files */
    INST_TABLES,        /**< writing the FATs and directories */
    INST_MBR,           /**< writing the boot sector last */
    INST_VERIFY,        /**< reading everything back */
    INST_DONE,
    INST_FAILED
} inst_state;

#define INST_CHUNK        256       /**< sectors moved per step (128 KiB) */
#define INST_CORE_MAX     (512 * 1024)
#define INST_MAX_FILES    12
#define INST_MAX_DIRS     8
#define INST_PL_FAT_MAX   65536     /**< payload FAT entries held in memory */

/** One file to put on the volume. */
typedef struct {
    char       name[12];            /**< 8.3 as stored: "KERNEL  ELF" */
    inst_u8    ntres;               /**< NT case bits: 0x08 base lower, 0x10 extension lower */
    int        parent;              /**< index into the directory table; 0 is the root */
    int        src;                 /**< where the bytes come from, see engine.c */
    inst_u32   size;                /**< in bytes */
    inst_u32   src_a;               /**< payload: first cluster; raw: first sector */
    const inst_u8 *mem;             /**< for sources held in memory */
    inst_u32   first, nclusters;    /**< its place on the volume */
    inst_u32   crc;                 /**< CRC-32 of the bytes as read from the source */
} inst_file;

/** One directory on the volume. Slot 0 is the root. */
typedef struct {
    char       name[12];
    inst_u8    ntres;
    int        parent;
    inst_u32   first;               /**< its cluster (0 for the root) */
} inst_dir;

/** The whole installation. Large (about 1.2 MiB): keep one, in .bss. */
typedef struct {
    /* devices */
    inst_dev   target, payload, initrd;
    inst_opts  opts;

    /* progress */
    int        opened;                      /**< @ref inst_open succeeded */
    inst_state state;
    char       err[96];
    const char *phase;
    inst_u32   units_done, units_total;

    /* layout */
    inst_u32   min_mib, max_mib, part_mib;  /**< volume size bounds and the size planned */
    inst_u32   disk_sectors;
    inst_u32   p1_start, p1_size;           /**< BIOS boot partition */
    inst_u32   p2_start, p2_size;           /**< the system volume */
    inst_u32   cs, fat_sectors, reserved, root_sectors, clusters, data_lba;
    inst_u32   serial;                      /**< FAT volume serial */
    inst_u8    disk_guid[16], p1_guid[16], p2_guid[16];

    /* the files */
    inst_dir   dirs[INST_MAX_DIRS];
    int        ndirs;
    inst_file  files[INST_MAX_FILES];
    int        nfiles;
    inst_u32   boot_sector_crc;
    char       marker[64];                  /**< contents of boot/grub/koppios.id */
    char       version[64];                 /**< from the payload's VERSION file, if any */

    /* cursor */
    int        cur;                         /**< current file (copy / verify) */
    inst_u32   pos;                         /**< sector within it */
    inst_u32   vcrc;                        /**< running CRC while verifying */

    /* the payload's own FAT, for reading it */
    inst_u32   pl_cs, pl_root, pl_data;
    inst_u16   pl_fat[INST_PL_FAT_MAX];

    /* images held in memory */
    inst_u8    boot_img[512];
    inst_u32   core_size;
    inst_u8    core_img[INST_CORE_MAX];

    /* tables built for the end of the copy */
    inst_u16   fat[INST_PL_FAT_MAX];

    inst_u8    buf[INST_CHUNK * 512];
    inst_u8    vbuf[INST_CHUNK * 512];
} inst;

/**
 * @brief Read the boot payload and the RAM disk's size; the first call of all.
 *
 * Once per run. Writes nothing and needs no target yet, so the user interface can
 * call it at start-up and report a missing payload before asking anything.
 *
 * @return 0 on success, -1 on failure (reason in @c I->err, state @ref INST_FAILED).
 */
int inst_open(inst *I, const inst_dev *payload, const inst_dev *initrd);

/**
 * @brief Lay the installation out on @p target.
 *
 * Writes nothing; may be called again whenever the disk or the options change. The
 * result is only valid for the next @ref inst_step run if @p target and @p opts are
 * what the user finally confirmed.
 *
 * @return 0 on success, -1 on failure (disk too small, ...; reason in @c I->err).
 *         A failed plan leaves the state at @ref INST_IDLE, so a different disk can be tried.
 */
int inst_plan(inst *I, const inst_dev *target, const inst_opts *opts);

/**
 * @brief Do the next bounded piece of work. The first call after @ref inst_plan starts
 *        writing: nothing before it touches the disk.
 * @return 1 if there is more, 0 once the installation is finished and verified,
 *         -1 on an error (see @c I->err).
 */
int inst_step(inst *I);

/** @return Percent complete, 0..100. */
int inst_percent(const inst *I);

/** @name Sizing, for the user interface (valid after a successful @ref inst_plan) */
///@{
/** @brief Smallest and largest system volume, in MiB, for this disk and payload. */
void inst_limits(const inst *I, inst_u32 *min_mib, inst_u32 *max_mib);
///@}

/**
 * @brief Inspect @p d before it is erased, to tell the user what is on it.
 *
 * Writes a short phrase such as "blank", "GPT, 3 partitions" or "FAT volume".
 */
void inst_probe(const inst_dev *d, char *out, int n);

/** @brief The CRC-32 (IEEE, as in zip and GPT) of @p n bytes, continuing from @p crc. */
inst_u32 inst_crc32(inst_u32 crc, const void *data, inst_u32 n);
