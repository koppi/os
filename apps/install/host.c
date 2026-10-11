/**
 * @file apps/install/host.c
 * @brief Run the installer's engine on the build machine, against image files.
 *
 *   inst-host <disk.img> <payload.img> <initrd.img> [volume-MiB]
 *
 * The same engine.c the ring-3 installer runs, with files standing in for the three
 * devices. tools/testing/install-host-test.sh uses it to install into a scratch image
 * and then check the result with sfdisk, fsck.fat and by booting it. Not part of the
 * OS image: built with the host compiler, not -m32.
 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "engine.h"

typedef struct { int fd; int writable; } fdev;

static int f_read(void *ctx, inst_u32 lba, void *buf, inst_u32 n) {
    fdev *d = ctx;
    size_t want = (size_t) n * 512;
    return pread(d->fd, buf, want, (off_t) lba * 512) == (ssize_t) want ? 0 : -1;
}

static int f_write(void *ctx, inst_u32 lba, const void *buf, inst_u32 n) {
    fdev *d = ctx;
    size_t want = (size_t) n * 512;
    return pwrite(d->fd, buf, want, (off_t) lba * 512) == (ssize_t) want ? 0 : -1;
}

static int f_flush(void *ctx) { return fsync(((fdev *) ctx)->fd); }

static void f_random(void *buf, inst_u32 n) {
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f || fread(buf, 1, n, f) != n)
        memset(buf, 0x5A, n);
    if (f) fclose(f);
}

static int open_dev(const char *path, int rw, fdev *d, inst_dev *dev) {
    d->fd = open(path, rw ? O_RDWR : O_RDONLY);
    if (d->fd < 0) {
        perror(path);
        return -1;
    }
    struct stat st;
    fstat(d->fd, &st);
    dev->ctx = d;
    dev->read = f_read;
    dev->write = rw ? f_write : 0;
    dev->flush = rw ? f_flush : 0;
    dev->sectors = (inst_u32) (st.st_size / 512);
    return 0;
}

static inst I;      /* about 1.2 MiB: not for the stack */

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s disk.img payload.img initrd.img [volume-MiB]\n", argv[0]);
        return 2;
    }
    fdev fd_disk, fd_pl, fd_rd;
    inst_dev disk, pl, rd;
    if (open_dev(argv[1], 1, &fd_disk, &disk) || open_dev(argv[2], 0, &fd_pl, &pl) ||
        open_dev(argv[3], 0, &fd_rd, &rd))
        return 1;

    if (inst_open(&I, &pl, &rd)) {
        fprintf(stderr, "open: %s\n", I.err);
        return 1;
    }
    printf("payload: %s\n", I.version[0] ? I.version : "(no version)");

    inst_opts o;
    memset(&o, 0, sizeof o);
    o.part_mib = argc > 4 ? (inst_u32) atoi(argv[4]) : 0;
    strcpy(o.label, "KOPPI-OS");
    o.unix_time = (inst_u32) time(0);
    o.random = f_random;

    char what[64];
    inst_probe(&disk, what, sizeof what);
    printf("disk: %u sectors (%u MiB), currently: %s\n", disk.sectors, disk.sectors / 2048, what);

    if (inst_plan(&I, &disk, &o)) {
        fprintf(stderr, "plan: %s\n", I.err);
        return 1;
    }
    printf("plan: volume %u MiB (allowed %u..%u), FAT16 with %u-sector clusters, %u clusters\n",
           I.part_mib, I.min_mib, I.max_mib, I.cs, I.clusters);

    const char *last = "";
    int r;
    while ((r = inst_step(&I)) == 1) {
        if (strcmp(last, I.phase)) {
            printf("  %3d%%  %s\n", inst_percent(&I), I.phase);
            last = I.phase;
        }
    }
    if (r < 0) {
        fprintf(stderr, "install failed: %s\n", I.err);
        return 1;
    }
    printf("  100%%  done\n");
    return 0;
}
