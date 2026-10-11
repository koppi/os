/**
 * @file apps/install/engine.c
 * @brief The installer's engine. See engine.h for the layout it produces.
 *
 * Three things in here are done by hand because the usual tool is not there to do
 * them: the GPT (no sgdisk), the FAT16 volume (no mkfs.fat -- and its contents are
 * known up front, so every file is laid out contiguously and the FAT is computed
 * rather than discovered), and what `grub-bios-setup` does to GRUB's boot sector and
 * core image. For the last one, GRUB 2 documents two patch points:
 *
 *   boot.img  + 0x5C   u64  LBA of core.img's first sector
 *   core.img  + 0x1F4  u64  LBA of the *second* core sector, where the run of
 *                           sectors that diskboot.img loads next begins
 *
 * `grub-mkimage` writes the second as if core.img were going to sit at LBA 1 (that
 * is, 2); here it sits at the start of the BIOS boot partition.
 */
#include "engine.h"

#define MEMCPY(d, s, n)  __builtin_memcpy((d), (s), (n))
#define MEMSET(d, c, n)  __builtin_memset((d), (c), (n))
#define MEMCMP(a, b, n)  __builtin_memcmp((a), (b), (n))

/** Where a file's bytes come from. */
enum { SRC_PAYLOAD, SRC_RAW, SRC_MEM };

#define SECTOR 512u
#define MIB_SECTORS 2048u

/** FAT16 holds at most 65524 clusters; with 32 KiB clusters that is just under 2 GiB. */
#define FAT16_MAX_CLUSTERS 65524u
#define FAT16_MAX_MIB      2047u

/** Progress weights for the work that is not file data. */
#define U_GPT    68u
#define U_CORE   400u
#define U_FORMAT 64u
#define U_TABLES 700u
#define U_MBR    1u
#define U_CHECK  400u

/** Slack kept free on the volume after the files: room to write, and for the
 *  user's own files, since the volume is /hda once it is running. */
#define FREE_MIB 8u

/* ------------------------------------------------------------------ *
 *  Little helpers                                                     *
 * ------------------------------------------------------------------ */

static inline void put16(inst_u8 *p, inst_u32 v) { p[0] = (inst_u8) v; p[1] = (inst_u8) (v >> 8); }
static inline void put32(inst_u8 *p, inst_u32 v) { put16(p, v); put16(p + 2, v >> 16); }
static inline void put64(inst_u8 *p, inst_u64 v) { put32(p, (inst_u32) v); put32(p + 4, (inst_u32) (v >> 32)); }
static inline inst_u32 get16(const inst_u8 *p) { return (inst_u32) p[0] | ((inst_u32) p[1] << 8); }
static inline inst_u32 get32(const inst_u8 *p) { return get16(p) | (get16(p + 2) << 16); }

static inline inst_u32 min_u32(inst_u32 a, inst_u32 b) { return a < b ? a : b; }

inst_u32 inst_crc32(inst_u32 crc, const void *data, inst_u32 n) {
    static inst_u32 table[256];
    static int ready;
    if (!ready) {
        for (inst_u32 i = 0; i < 256; i++) {
            inst_u32 c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        ready = 1;
    }
    const inst_u8 *p = (const inst_u8 *) data;
    crc = ~crc;
    while (n--)
        crc = table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

/** @brief Append the decimal form of @p v to @p s (which has room). */
static char *put_dec(char *s, inst_u32 v) {
    char tmp[12];
    int n = 0;
    do { tmp[n++] = (char) ('0' + v % 10); v /= 10; } while (v);
    while (n)
        *s++ = tmp[--n];
    return s;
}

/** @brief Record a failure: @p pre, then @p n if @p has_n, then @p post. Always returns -1. */
static int failf(inst *I, const char *pre, int has_n, inst_u32 n, const char *post) {
    char *d = I->err, *end = I->err + sizeof I->err - 12;
    while (*pre && d < end)
        *d++ = *pre++;
    if (has_n)
        d = put_dec(d, n);
    while (post && *post && d < end + 11)
        *d++ = *post++;
    *d = 0;
    I->state = INST_FAILED;
    return -1;
}
#define FAIL(I, msg)       failf((I), (msg), 0, 0, 0)
#define FAILN(I, msg, n)   failf((I), (msg), 1, (n), 0)

/** Unix time -> FAT date and time. Anything before 2000 is clamped there. */
static void fat_stamp(inst_u32 unix_time, inst_u16 *date, inst_u16 *time) {
    if (unix_time < 946684800u)
        unix_time = 946684800u;
    inst_u32 days = unix_time / 86400u, secs = unix_time % 86400u;
    /* days since 1970-01-01 -> civil date (Howard Hinnant's algorithm) */
    inst_u32 z = days + 719468u;
    inst_u32 era = z / 146097u;
    inst_u32 doe = z - era * 146097u;
    inst_u32 yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    inst_u32 y = yoe + era * 400u;
    inst_u32 doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    inst_u32 mp = (5u * doy + 2u) / 153u;
    inst_u32 d = doy - (153u * mp + 2u) / 5u + 1u;
    inst_u32 m = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2)
        y++;
    if (y < 1980) y = 1980;
    if (y > 2107) y = 2107;
    *date = (inst_u16) (((y - 1980) << 9) | (m << 5) | d);
    *time = (inst_u16) (((secs / 3600u) << 11) | (((secs / 60u) % 60u) << 5) | ((secs % 60u) / 2u));
}

static void make_random(inst *I, void *buf, inst_u32 n) {
    if (I->opts.random) {
        I->opts.random(buf, n);
        return;
    }
    /* No random source (a test run): a fixed, recognisable pattern. */
    inst_u8 *p = (inst_u8 *) buf;
    for (inst_u32 i = 0; i < n; i++)
        p[i] = (inst_u8) (0xA5 ^ (i * 37u));
}

static void make_guid(inst *I, inst_u8 *g) {
    make_random(I, g, 16);
    g[7] = (inst_u8) ((g[7] & 0x0F) | 0x40);        /* version 4 */
    g[8] = (inst_u8) ((g[8] & 0x3F) | 0x80);        /* RFC 4122 variant */
}

/* ------------------------------------------------------------------ *
 *  Reading the payload: a FAT16 image with a flat root directory      *
 * ------------------------------------------------------------------ */

/** Load the payload's BPB and FAT. */
static int pl_open(inst *I) {
    inst_u8 *s = I->buf;
    if (I->payload.read(I->payload.ctx, 0, s, 1))
        return FAIL(I, "cannot read the installation payload");
    if (get16(s + 11) != SECTOR || s[510] != 0x55 || s[511] != 0xAA)
        return FAIL(I, "the installation payload is not a FAT image");

    inst_u32 cs = s[13], reserved = get16(s + 14), nfats = s[16];
    inst_u32 rootents = get16(s + 17);
    inst_u32 total = get16(s + 19) ? get16(s + 19) : get32(s + 32);
    inst_u32 fatsz = get16(s + 22);
    if (!cs || (cs & (cs - 1)) || !nfats || !fatsz || !rootents || !total)
        return FAIL(I, "the installation payload has an odd FAT layout");

    inst_u32 root_secs = (rootents * 32u + SECTOR - 1) / SECTOR;
    I->pl_cs = cs;
    I->pl_root = reserved + nfats * fatsz;
    I->pl_data = I->pl_root + root_secs;
    if (I->pl_data >= total || (total - I->pl_data) / cs < 4085u)
        return FAIL(I, "the installation payload is not FAT16");
    if (fatsz * (SECTOR / 2) > INST_PL_FAT_MAX)
        return FAIL(I, "the installation payload is too large");

    for (inst_u32 done = 0; done < fatsz; ) {
        inst_u32 n = min_u32(INST_CHUNK, fatsz - done);
        if (I->payload.read(I->payload.ctx, reserved + done, I->buf, n))
            return FAIL(I, "cannot read the payload's FAT");
        MEMCPY((inst_u8 *) I->pl_fat + done * SECTOR, I->buf, n * SECTOR);
        done += n;
    }
    return 0;
}

/** Find @p name (11 chars, "KERNEL  ELF") in the payload's root. */
static int pl_find(inst *I, const char *name, inst_u32 *first, inst_u32 *size) {
    inst_u32 root = I->pl_root;
    inst_u32 root_secs = I->pl_data - root;
    for (inst_u32 s = 0; s < root_secs; s++) {
        inst_u8 sec[SECTOR];
        if (I->payload.read(I->payload.ctx, root + s, sec, 1))
            return -1;
        for (inst_u32 e = 0; e < SECTOR; e += 32) {
            inst_u8 *d = sec + e;
            if (d[0] == 0)
                return -1;
            if (d[0] == 0xE5 || (d[11] & 0x0F) == 0x0F || (d[11] & 0x08))
                continue;
            if (!MEMCMP(d, name, 11)) {
                *first = get16(d + 26);
                *size = get32(d + 28);
                return 0;
            }
        }
    }
    return -1;
}

/**
 * Read @p n sectors of a payload file starting at its sector @p idx.
 * Walks the cluster chain from the start: files here are a few thousand sectors,
 * so a walk per 128 KiB chunk costs nothing worth a cursor.
 */
static int pl_read(inst *I, inst_u32 first, inst_u32 idx, inst_u32 n, void *buf) {
    inst_u32 cs = I->pl_cs;
    inst_u32 c = first;
    for (inst_u32 k = idx / cs; k; k--) {
        if (c < 2 || c >= 0xFFF0u)
            return -1;
        c = I->pl_fat[c];
    }
    inst_u8 *out = (inst_u8 *) buf;
    inst_u32 off = idx % cs;
    while (n) {
        if (c < 2 || c >= 0xFFF0u)
            return -1;
        inst_u32 run = min_u32(cs - off, n);
        if (I->payload.read(I->payload.ctx, I->pl_data + (c - 2) * cs + off, out, run))
            return -1;
        out += run * SECTOR;
        n -= run;
        off = 0;
        if (n)
            c = I->pl_fat[c];
    }
    return 0;
}

/** Read a whole small payload file into @p dst (at most @p max bytes). */
static int pl_slurp(inst *I, const char *name, void *dst, inst_u32 max, inst_u32 *got) {
    inst_u32 first, size;
    if (pl_find(I, name, &first, &size))
        return -1;
    if (size > max)
        return -1;
    inst_u32 secs = (size + SECTOR - 1) / SECTOR;
    inst_u8 *d = (inst_u8 *) dst;
    /* Whole sectors straight in would overrun a buffer sized to the file. */
    for (inst_u32 done = 0; done < secs; ) {
        inst_u32 n = min_u32(INST_CHUNK, secs - done);
        if (pl_read(I, first, done, n, I->vbuf))
            return -1;
        inst_u32 bytes = min_u32(n * SECTOR, size - done * SECTOR);
        MEMCPY(d + done * SECTOR, I->vbuf, bytes);
        done += n;
    }
    *got = size;
    return 0;
}

/* ------------------------------------------------------------------ *
 *  inst_open                                                          *
 * ------------------------------------------------------------------ */

static void add_dir(inst *I, const char *name, inst_u8 ntres, int parent) {
    inst_dir *d = &I->dirs[I->ndirs++];
    MEMCPY(d->name, name, 12);
    d->ntres = ntres;
    d->parent = parent;
    d->first = 0;
}

static void add_file(inst *I, const char *name, inst_u8 ntres, int parent, int src,
                     inst_u32 size, inst_u32 src_a, const inst_u8 *mem) {
    inst_file *f = &I->files[I->nfiles++];
    MEMCPY(f->name, name, 12);
    f->ntres = ntres;
    f->parent = parent;
    f->src = src;
    f->size = size;
    f->src_a = src_a;
    f->mem = mem;
    f->first = f->nclusters = 0;
    f->crc = 0;
}

int inst_open(inst *I, const inst_dev *payload, const inst_dev *initrd) {
    MEMSET(I, 0, sizeof *I);
    I->payload = *payload;
    I->initrd = *initrd;
    I->state = INST_IDLE;
    I->phase = "";

    if (pl_open(I))
        return -1;

    inst_u32 got = 0;
    if (pl_slurp(I, "BOOT    IMG", I->boot_img, sizeof I->boot_img, &got) || got != 512 ||
        I->boot_img[510] != 0x55 || I->boot_img[511] != 0xAA)
        return FAIL(I, "the payload has no usable GRUB boot sector (BOOT.IMG)");
    if (pl_slurp(I, "CORE    IMG", I->core_img, sizeof I->core_img, &got) || got < 1024)
        return FAIL(I, "the payload has no usable GRUB core image (CORE.IMG)");
    I->core_size = got;

    /* The version line is optional. */
    {
        inst_u32 vgot = 0;
        char v[64];
        MEMSET(v, 0, sizeof v);
        if (!pl_slurp(I, "VERSION    ", v, sizeof v - 1, &vgot)) {
            inst_u32 i = 0;
            for (; i < vgot && i < sizeof I->version - 1 && v[i] >= ' '; i++)
                I->version[i] = v[i];
            I->version[i] = 0;
        }
    }

    /* The RAM disk is what is being installed: its size comes from its own BPB. */
    inst_u8 *s = I->buf;
    if (I->initrd.read(I->initrd.ctx, 0, s, 1) || get16(s + 11) != SECTOR ||
        s[510] != 0x55 || s[511] != 0xAA)
        return FAIL(I, "the boot RAM disk is not a FAT image");
    inst_u32 rd_secs = get16(s + 19) ? get16(s + 19) : get32(s + 32);
    if (!rd_secs || rd_secs > I->initrd.sectors)
        return FAIL(I, "the boot RAM disk has an odd size");

    inst_u32 sz_efi = 0, sz_kernel = 0, sz_cfg = 0, sz_font = 0, f_efi = 0, f_kernel = 0, f_cfg = 0, f_font = 0;
    if (pl_find(I, "BOOTX64 EFI", &f_efi, &sz_efi)) return FAIL(I, "the payload has no BOOTX64.EFI");
    if (pl_find(I, "KERNEL  ELF", &f_kernel, &sz_kernel)) return FAIL(I, "the payload has no KERNEL.ELF");
    if (pl_find(I, "GRUB    CFG", &f_cfg, &sz_cfg)) return FAIL(I, "the payload has no GRUB.CFG");
    if (pl_find(I, "ASCII   PF2", &f_font, &sz_font)) return FAIL(I, "the payload has no ASCII.PF2");

    /* The marker GRUB searches for to find this volume among all the disks
     * (see arch/x86/boot/grub/embed.cfg). Its contents are for people. */
    I->marker[0] = 0;
    {
        static const char head[] = "koppi-os system volume\n";
        MEMCPY(I->marker, head, sizeof head);
    }

    add_dir(I, "           ", 0, -1);                          /* 0: root */
    add_dir(I, "EFI        ", 0, 0);                           /* 1 */
    add_dir(I, "BOOT       ", 0, 1);                           /* 2: EFI/BOOT */
    add_dir(I, "BOOT       ", 0x08, 0);                        /* 3: boot */
    add_dir(I, "GRUB       ", 0x08, 3);                        /* 4: boot/grub */
    add_dir(I, "FONTS      ", 0x08, 4);                        /* 5: boot/grub/fonts */

    add_file(I, "GRUB    CFG", 0x18, 4, SRC_PAYLOAD, sz_cfg, f_cfg, 0);
    add_file(I, "KOPPIOS ID ", 0x18, 4, SRC_MEM, 0, 0, (const inst_u8 *) I->marker);
    add_file(I, "ASCII   PF2", 0x18, 5, SRC_PAYLOAD, sz_font, f_font, 0);
    add_file(I, "BOOTX64 EFI", 0, 2, SRC_PAYLOAD, sz_efi, f_efi, 0);
    add_file(I, "KERNEL  ELF", 0x18, 3, SRC_PAYLOAD, sz_kernel, f_kernel, 0);
    add_file(I, "INITRD  IMG", 0x18, 3, SRC_RAW, rd_secs * SECTOR, 0, 0);

    /* koppios.id is the one file whose size is not a payload property. */
    inst_u32 mlen = 0;
    while (I->marker[mlen])
        mlen++;
    I->files[1].size = mlen;
    I->opened = 1;
    return 0;
}

/* ------------------------------------------------------------------ *
 *  inst_plan                                                          *
 * ------------------------------------------------------------------ */

static inst_u32 clusters_for(inst_u32 bytes, inst_u32 cs) {
    inst_u32 per = cs * SECTOR;
    return (bytes + per - 1) / per;
}

/**
 * FAT16 geometry for a volume of @p vol sectors: the smallest cluster size that
 * keeps the cluster count under the FAT16 limit, and the FAT size that follows.
 */
static int fat_geometry(inst *I, inst_u32 vol) {
    const inst_u32 reserved = 1, nfats = 2, root_secs = 32;
    for (inst_u32 cs = 1; cs <= 64; cs <<= 1) {
        inst_u32 fs = 1, clusters = 0;
        for (int iter = 0; iter < 4; iter++) {
            if (vol <= reserved + root_secs + nfats * fs)
                return -1;
            clusters = (vol - reserved - root_secs - nfats * fs) / cs;
            fs = ((clusters + 2) * 2 + SECTOR - 1) / SECTOR;
        }
        clusters = (vol - reserved - root_secs - nfats * fs) / cs;
        if (clusters > FAT16_MAX_CLUSTERS || clusters < 4085u)
            continue;
        I->cs = cs;
        I->fat_sectors = fs;
        I->reserved = reserved;
        I->root_sectors = root_secs;
        I->clusters = clusters;
        return 0;
    }
    return -1;
}

static int plan_inner(inst *I, const inst_dev *target, const inst_opts *opts) {
    if (!target->write)
        return FAIL(I, "this disk cannot be written");

    I->target = *target;
    I->opts = *opts;
    I->disk_sectors = target->sectors;

    /* How much the files need, at the worst cluster size (64 sectors): each file
     * wastes at most one cluster, each directory takes one. */
    inst_u32 need_mib = 0;
    {
        unsigned long long bytes = 0;
        for (int i = 0; i < I->nfiles; i++)
            bytes += I->files[i].size + 32768u;
        bytes += (unsigned long long) I->ndirs * 32768u;
        bytes += 2u * 128u * 1024u + 16u * 1024u;       /* two FATs + the root */
        need_mib = (inst_u32) ((bytes + (1u << 20) - 1) >> 20) + FREE_MIB;
        need_mib = (need_mib + 7u) & ~7u;
    }
    I->min_mib = need_mib;

    /* The usable span: after the BIOS boot partition and before the backup GPT. */
    inst_u32 p1_start = MIB_SECTORS, p1_size = MIB_SECTORS, p2_start = p1_start + p1_size;
    if (I->disk_sectors < p2_start + 34u + MIB_SECTORS)
        return failf(I, "this disk is too small: it needs at least ", 1, I->min_mib + 4, " MiB");
    inst_u32 avail = (I->disk_sectors - 34u - p2_start) / MIB_SECTORS;   /* MiB */
    I->max_mib = avail < FAT16_MAX_MIB ? avail : FAT16_MAX_MIB;
    if (I->max_mib < I->min_mib)
        return failf(I, "this disk is too small: it needs at least ", 1, I->min_mib + 4, " MiB");

    inst_u32 mib = opts->part_mib ? opts->part_mib : I->max_mib;
    if (mib < I->min_mib) mib = I->min_mib;
    if (mib > I->max_mib) mib = I->max_mib;
    I->part_mib = mib;

    I->p1_start = p1_start;
    I->p1_size = p1_size;
    I->p2_start = p2_start;
    I->p2_size = mib * MIB_SECTORS;

    if (fat_geometry(I, I->p2_size))
        return failf(I, "cannot lay out a FAT16 volume of ", 1, mib, " MiB");
    I->data_lba = I->p2_start + I->reserved + 2u * I->fat_sectors + I->root_sectors;

    /* Hand out clusters: directories first, then the files, each contiguous. */
    inst_u32 next = 2;
    for (int i = 1; i < I->ndirs; i++)
        I->dirs[i].first = next++;
    for (int i = 0; i < I->nfiles; i++) {
        inst_file *f = &I->files[i];
        f->nclusters = clusters_for(f->size, I->cs);
        f->first = f->nclusters ? next : 0;
        next += f->nclusters;
        f->crc = 0;
    }
    if (next - 2 > I->clusters)
        return failf(I, "the files do not fit a volume of ", 1, mib, " MiB");

    make_guid(I, I->disk_guid);
    make_guid(I, I->p1_guid);
    make_guid(I, I->p2_guid);
    make_random(I, &I->serial, 4);

    /* Progress: each file is written and then read back; the metadata is a small
     * fixed allowance, weighted by roughly how many sectors it is. */
    I->units_done = 0;
    I->units_total = U_GPT + U_CORE + U_FORMAT + U_TABLES + U_MBR + U_CHECK;
    for (int i = 0; i < I->nfiles; i++)
        I->units_total += 2u * ((I->files[i].size + SECTOR - 1) / SECTOR);

    I->cur = 0;
    I->pos = 0;
    I->phase = "Ready";
    I->state = INST_PLANNED;
    return 0;
}

int inst_plan(inst *I, const inst_dev *target, const inst_opts *opts) {
    if (!I->opened)
        return -1;
    I->state = INST_IDLE;
    I->err[0] = 0;
    if (plan_inner(I, target, opts)) {
        I->state = INST_IDLE;           /* a plan that fails is not a failed install */
        return -1;
    }
    return 0;
}

void inst_limits(const inst *I, inst_u32 *min_mib, inst_u32 *max_mib) {
    *min_mib = I->min_mib;
    *max_mib = I->max_mib;
}

/* ------------------------------------------------------------------ *
 *  The writing                                                        *
 * ------------------------------------------------------------------ */

static int wr(inst *I, inst_u32 lba, const void *buf, inst_u32 n) {
    if (I->target.write(I->target.ctx, lba, buf, n))
        return FAILN(I, "cannot write to the disk at sector ", lba);
    return 0;
}

static int rd_target(inst *I, inst_u32 lba, void *buf, inst_u32 n) {
    if (I->target.read(I->target.ctx, lba, buf, n))
        return FAILN(I, "cannot read the disk at sector ", lba);
    return 0;
}

/** One 128-byte GPT entry. */
static void gpt_entry(inst_u8 *e, const inst_u8 *type, const inst_u8 *guid,
                      inst_u32 first, inst_u32 last, const char *name) {
    MEMCPY(e, type, 16);
    MEMCPY(e + 16, guid, 16);
    put64(e + 32, first);
    put64(e + 40, last);
    put64(e + 48, 0);
    for (int i = 0; name[i] && i < 36; i++)
        put16(e + 56 + 2 * i, (inst_u8) name[i]);
}

/** Build the GPT header for one copy into @p h (512 bytes). */
static void gpt_header(inst *I, inst_u8 *h, inst_u32 here, inst_u32 there,
                       inst_u32 entries_lba, inst_u32 entries_crc) {
    inst_u32 last = I->disk_sectors - 1;
    MEMSET(h, 0, SECTOR);
    MEMCPY(h, "EFI PART", 8);
    put32(h + 8, 0x00010000u);
    put32(h + 12, 92);
    put64(h + 24, here);
    put64(h + 32, there);
    put64(h + 40, 34);
    put64(h + 48, last - 33u);
    MEMCPY(h + 56, I->disk_guid, 16);
    put64(h + 72, entries_lba);
    put32(h + 80, 128);
    put32(h + 84, 128);
    put32(h + 88, entries_crc);
    put32(h + 16, inst_crc32(0, h, 92));
}

static const inst_u8 GUID_BIOS_BOOT[16] = {
    0x48, 0x61, 0x68, 0x21, 0x49, 0x64, 0x6F, 0x6E, 0x74, 0x4E, 0x65, 0x65, 0x64, 0x45, 0x46, 0x49 };
static const inst_u8 GUID_ESP[16] = {
    0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B };

/** Both copies of the GPT. Does not touch sector 0 -- that is written last. */
static int write_gpt(inst *I) {
    inst_u8 *e = I->buf;                       /* 32 sectors of entries */
    MEMSET(e, 0, 128 * 128);
    gpt_entry(e, GUID_BIOS_BOOT, I->p1_guid, I->p1_start, I->p1_start + I->p1_size - 1, "BIOS boot");
    gpt_entry(e + 128, GUID_ESP, I->p2_guid, I->p2_start, I->p2_start + I->p2_size - 1, "koppi-os");
    inst_u32 ecrc = inst_crc32(0, e, 128 * 128);
    inst_u32 last = I->disk_sectors - 1;

    inst_u8 h[SECTOR];
    if (wr(I, 2, e, 32)) return -1;
    gpt_header(I, h, 1, last, 2, ecrc);
    if (wr(I, 1, h, 1)) return -1;
    if (wr(I, last - 32u, e, 32)) return -1;
    gpt_header(I, h, last, 1, last - 32u, ecrc);
    if (wr(I, last, h, 1)) return -1;
    return 0;
}

/** Check both GPT copies read back right. */
static int verify_gpt(inst *I) {
    inst_u32 last = I->disk_sectors - 1;
    inst_u8 h[SECTOR];
    for (int copy = 0; copy < 2; copy++) {
        inst_u32 hl = copy ? last : 1u, el = copy ? last - 32u : 2u;
        if (rd_target(I, hl, h, 1) || rd_target(I, el, I->vbuf, 32))
            return -1;
        inst_u32 hcrc = get32(h + 16);
        put32(h + 16, 0);
        if (MEMCMP(h, "EFI PART", 8) || inst_crc32(0, h, 92) != hcrc ||
            inst_crc32(0, I->vbuf, 128 * 128) != get32(h + 88))
            return FAIL(I, "the partition table did not read back correctly");
    }
    return 0;
}

/** GRUB's core image, patched to sit at the start of the BIOS boot partition. */
static int write_core(inst *I) {
    put64(I->core_img + 0x1F4, (inst_u64) I->p1_start + 1u);
    inst_u32 secs = (I->core_size + SECTOR - 1) / SECTOR;
    if (secs > I->p1_size)
        return FAIL(I, "GRUB's core image does not fit the BIOS boot partition");
    MEMSET(I->core_img + I->core_size, 0, secs * SECTOR - I->core_size);
    for (inst_u32 done = 0; done < secs; ) {
        inst_u32 n = min_u32(INST_CHUNK, secs - done);
        if (wr(I, I->p1_start + done, I->core_img + done * SECTOR, n))
            return -1;
        done += n;
    }
    return 0;
}

/** The boot sector of the new FAT16 volume. */
static void bpb(inst *I, inst_u8 *b) {
    MEMSET(b, 0, SECTOR);
    b[0] = 0xEB; b[1] = 0x3C; b[2] = 0x90;
    MEMCPY(b + 3, "KOPPIOS ", 8);
    put16(b + 11, SECTOR);
    b[13] = (inst_u8) I->cs;
    put16(b + 14, I->reserved);
    b[16] = 2;
    put16(b + 17, 512);
    if (I->p2_size < 65536u) put16(b + 19, I->p2_size);
    b[21] = 0xF8;
    put16(b + 22, I->fat_sectors);
    put16(b + 24, 63);
    put16(b + 26, 255);
    put32(b + 28, I->p2_start);
    if (I->p2_size >= 65536u) put32(b + 32, I->p2_size);
    b[36] = 0x80;
    b[38] = 0x29;
    put32(b + 39, I->serial);
    char label[11];
    MEMSET(label, ' ', 11);
    for (int i = 0; i < 11 && I->opts.label[i]; i++) {
        char c = I->opts.label[i];
        label[i] = (c >= 'a' && c <= 'z') ? (char) (c - 32) : c;
    }
    MEMCPY(b + 43, label, 11);
    MEMCPY(b + 54, "FAT16   ", 8);
    b[510] = 0x55; b[511] = 0xAA;
}

/** One 32-byte directory entry. */
static void dirent(inst_u8 *e, const char *name, inst_u8 attr, inst_u8 ntres,
                   inst_u32 first, inst_u32 size, inst_u16 date, inst_u16 time) {
    MEMSET(e, 0, 32);
    MEMCPY(e, name, 11);
    e[11] = attr;
    e[12] = ntres;
    put16(e + 14, time);
    put16(e + 16, date);
    put16(e + 18, date);
    put16(e + 22, time);
    put16(e + 24, date);
    put16(e + 26, first);
    put32(e + 28, size);
}

/** FATs, root directory and subdirectories: everything the copy did not write. */
static int write_tables(inst *I) {
    inst_u16 date, time;
    fat_stamp(I->opts.unix_time, &date, &time);

    /* The FAT: every object was given a contiguous run. */
    MEMSET(I->fat, 0, sizeof I->fat);
    I->fat[0] = 0xFFF8;
    I->fat[1] = 0xFFFF;
    for (int i = 1; i < I->ndirs; i++)
        I->fat[I->dirs[i].first] = 0xFFFF;
    for (int i = 0; i < I->nfiles; i++) {
        const inst_file *f = &I->files[i];
        for (inst_u32 k = 0; k < f->nclusters; k++)
            I->fat[f->first + k] = (k + 1 < f->nclusters) ? (inst_u16) (f->first + k + 1) : 0xFFFF;
    }
    inst_u32 fat_lba = I->p2_start + I->reserved;
    for (int copy = 0; copy < 2; copy++)
        for (inst_u32 done = 0; done < I->fat_sectors; ) {
            inst_u32 n = min_u32(INST_CHUNK, I->fat_sectors - done);
            if (wr(I, fat_lba + copy * I->fat_sectors + done, (inst_u8 *) I->fat + done * SECTOR, n))
                return -1;
            done += n;
        }

    /* The root: the volume label, then its children. */
    {
        inst_u8 *r = I->buf;
        MEMSET(r, 0, I->root_sectors * SECTOR);
        int n = 0;
        char label[11];
        MEMSET(label, ' ', 11);
        for (int i = 0; i < 11 && I->opts.label[i]; i++) {
            char c = I->opts.label[i];
            label[i] = (c >= 'a' && c <= 'z') ? (char) (c - 32) : c;
        }
        dirent(r + 32 * n++, label, 0x08, 0, 0, 0, date, time);
        for (int i = 1; i < I->ndirs; i++)
            if (I->dirs[i].parent == 0)
                dirent(r + 32 * n++, I->dirs[i].name, 0x10, I->dirs[i].ntres, I->dirs[i].first, 0, date, time);
        for (int i = 0; i < I->nfiles; i++)
            if (I->files[i].parent == 0)
                dirent(r + 32 * n++, I->files[i].name, 0x20, I->files[i].ntres, I->files[i].first, I->files[i].size, date, time);
        if (wr(I, I->p2_start + I->reserved + 2u * I->fat_sectors, r, I->root_sectors))
            return -1;
    }

    /* Each subdirectory is one cluster: "." and "..", then its children. */
    for (int d = 1; d < I->ndirs; d++) {
        inst_u8 *r = I->buf;
        MEMSET(r, 0, I->cs * SECTOR);
        int n = 0;
        int par = I->dirs[d].parent;
        dirent(r + 32 * n++, ".          ", 0x10, 0, I->dirs[d].first, 0, date, time);
        dirent(r + 32 * n++, "..         ", 0x10, 0, par == 0 ? 0 : I->dirs[par].first, 0, date, time);
        for (int i = 1; i < I->ndirs; i++)
            if (I->dirs[i].parent == d)
                dirent(r + 32 * n++, I->dirs[i].name, 0x10, I->dirs[i].ntres, I->dirs[i].first, 0, date, time);
        for (int i = 0; i < I->nfiles; i++)
            if (I->files[i].parent == d)
                dirent(r + 32 * n++, I->files[i].name, 0x20, I->files[i].ntres, I->files[i].first, I->files[i].size, date, time);
        if (n * 32 > (int) (I->cs * SECTOR))
            return FAIL(I, "a directory overflowed its cluster");
        if (wr(I, I->data_lba + (I->dirs[d].first - 2u) * I->cs, r, I->cs))
            return -1;
    }

    /* The boot sector last of the volume's own, so a half-written volume is not
     * mistaken for a whole one. */
    inst_u8 b[SECTOR];
    bpb(I, b);
    return wr(I, I->p2_start, b, 1);
}

/** Sector 0: GRUB's boot.img with the protective MBR partition entry set. */
static void build_mbr(inst *I, inst_u8 *m) {
    MEMCPY(m, I->boot_img, SECTOR);
    put64(m + 0x5C, I->p1_start);                   /* where core.img begins */
    MEMSET(m + 0x1BE, 0, 64);
    inst_u32 span = I->disk_sectors - 1u;
    m[0x1BE] = 0x80;                                /* some BIOSes insist on an active entry */
    m[0x1BF] = 0x00; m[0x1C0] = 0x02; m[0x1C1] = 0x00;
    m[0x1C2] = 0xEE;                                /* GPT protective */
    m[0x1C3] = 0xFF; m[0x1C4] = 0xFF; m[0x1C5] = 0xFF;
    put32(m + 0x1C6, 1);
    put32(m + 0x1CA, span);
    m[510] = 0x55; m[511] = 0xAA;
}

/** Where file data goes. */
static inline inst_u32 data_sector(const inst *I, const inst_file *f, inst_u32 idx) {
    return I->data_lba + (f->first - 2u) * I->cs + idx;
}

/**
 * Fetch @p n sectors of @p f from the source into @p buf (zero-padded past the end
 * of the file's last sector). @return 0, or -1 with an error recorded.
 */
static int src_read(inst *I, const inst_file *f, inst_u32 idx, inst_u32 n, inst_u8 *buf) {
    switch (f->src) {
    case SRC_PAYLOAD:
        if (pl_read(I, f->src_a, idx, n, buf))
            return FAIL(I, "cannot read a file from the installation payload");
        break;
    case SRC_RAW:
        if (I->initrd.read(I->initrd.ctx, f->src_a + idx, buf, n))
            return FAILN(I, "cannot read the RAM disk at sector ", idx);
        break;
    default:
        MEMSET(buf, 0, n * SECTOR);
        if (idx * SECTOR < f->size)
            MEMCPY(buf, f->mem + idx * SECTOR, min_u32(n * SECTOR, f->size - idx * SECTOR));
        break;
    }
    /* Whatever the source returns past the file's end is not part of the file. */
    inst_u32 end = idx * SECTOR + n * SECTOR;
    if (end > f->size) {
        inst_u32 from = f->size > idx * SECTOR ? f->size - idx * SECTOR : 0;
        MEMSET(buf + from, 0, n * SECTOR - from);
    }
    return 0;
}

/** CRC over the part of a chunk that belongs to the file. */
static inst_u32 crc_chunk(const inst_file *f, inst_u32 idx, inst_u32 n, const inst_u8 *buf, inst_u32 crc) {
    inst_u32 start = idx * SECTOR;
    if (start >= f->size)
        return crc;
    return inst_crc32(crc, buf, min_u32(n * SECTOR, f->size - start));
}

static inline inst_u32 file_sectors(const inst_file *f) { return (f->size + SECTOR - 1) / SECTOR; }

/** The next piece of the copy. @return 0 normally, -1 on error; sets I->state when done. */
static int copy_step(inst *I) {
    while (I->cur < I->nfiles && file_sectors(&I->files[I->cur]) == 0)
        I->cur++;
    if (I->cur >= I->nfiles) {
        if (I->target.flush)
            I->target.flush(I->target.ctx);
        I->state = INST_TABLES;
        I->cur = 0;
        I->pos = 0;
        return 0;
    }
    inst_file *f = &I->files[I->cur];
    inst_u32 total = file_sectors(f);
    inst_u32 n = min_u32(INST_CHUNK, total - I->pos);

    if (src_read(I, f, I->pos, n, I->buf))
        return -1;
    f->crc = crc_chunk(f, I->pos, n, I->buf, f->crc);
    if (wr(I, data_sector(I, f, I->pos), I->buf, n))
        return -1;

    I->pos += n;
    I->units_done += n;
    if (I->pos >= total) {
        I->cur++;
        I->pos = 0;
    }
    return 0;
}

/** The next piece of the read-back. */
static int verify_step(inst *I) {
    while (I->cur < I->nfiles && file_sectors(&I->files[I->cur]) == 0)
        I->cur++;
    if (I->cur >= I->nfiles) {
        I->state = INST_DONE;
        I->phase = "Finished";
        return 0;
    }
    inst_file *f = &I->files[I->cur];
    inst_u32 total = file_sectors(f);
    inst_u32 n = min_u32(INST_CHUNK, total - I->pos);

    if (rd_target(I, data_sector(I, f, I->pos), I->vbuf, n))
        return -1;
    if (I->pos == 0)
        I->vcrc = 0;
    I->vcrc = crc_chunk(f, I->pos, n, I->vbuf, I->vcrc);

    I->pos += n;
    I->units_done += n;
    if (I->pos >= total) {
        if (I->vcrc != f->crc)
            return failf(I, "a file did not read back the way it was written: ", 0, 0, f->name);
        I->cur++;
        I->pos = 0;
    }
    return 0;
}

int inst_step(inst *I) {
    int r = 0;
    switch (I->state) {
    case INST_PLANNED:
    case INST_GPT:
        I->state = INST_GPT;
        I->phase = "Writing the partition table";
        r = write_gpt(I);
        if (!r) {
            I->units_done += U_GPT;
            I->state = INST_CORE;
        }
        break;
    case INST_CORE:
        I->phase = "Installing the boot loader";
        r = write_core(I);
        if (!r) {
            I->units_done += U_CORE;
            I->state = INST_FORMAT;
        }
        break;
    case INST_FORMAT: {
        I->phase = "Preparing the system volume";
        /* Kill whatever boot sector was here before; the real one goes in last. */
        MEMSET(I->buf, 0, SECTOR);
        r = wr(I, I->p2_start, I->buf, 1);
        if (!r) {
            I->units_done += U_FORMAT;
            I->state = INST_COPY;
            I->cur = 0;
            I->pos = 0;
        }
        break;
    }
    case INST_COPY:
        I->phase = "Copying the system";
        r = copy_step(I);
        break;
    case INST_TABLES:
        I->phase = "Writing the file system";
        r = write_tables(I);
        if (!r) {
            I->units_done += U_TABLES;
            I->state = INST_MBR;
        }
        break;
    case INST_MBR: {
        I->phase = "Making the disk bootable";
        inst_u8 m[SECTOR];
        build_mbr(I, m);
        r = wr(I, 0, m, 1);
        if (!r && I->target.flush)
            r = I->target.flush(I->target.ctx) ? FAIL(I, "the disk did not accept the flush") : 0;
        if (!r) {
            I->units_done += U_MBR;
            I->state = INST_VERIFY;
            I->cur = 0;
            I->pos = 0;
            I->vcrc = 0;
            /* The small things first: sector 0, both GPTs, GRUB's core image. */
            I->phase = "Checking what was written";
            inst_u8 back[SECTOR];
            r = rd_target(I, 0, back, 1);
            if (!r && MEMCMP(back, m, SECTOR))
                r = FAIL(I, "the boot sector did not read back correctly");
            if (!r)
                r = verify_gpt(I);
            if (!r) {
                inst_u32 secs = (I->core_size + SECTOR - 1) / SECTOR;
                for (inst_u32 done = 0; !r && done < secs; ) {
                    inst_u32 n = min_u32(INST_CHUNK, secs - done);
                    r = rd_target(I, I->p1_start + done, I->vbuf, n);
                    if (!r && MEMCMP(I->vbuf, I->core_img + done * SECTOR, n * SECTOR))
                        r = FAIL(I, "GRUB's core image did not read back correctly");
                    done += n;
                }
            }
            if (!r)
                I->units_done += U_CHECK;
        }
        break;
    }
    case INST_VERIFY:
        I->phase = "Checking what was written";
        r = verify_step(I);
        break;
    case INST_DONE:
        return 0;
    default:
        return -1;
    }
    if (r) {
        I->state = INST_FAILED;
        return -1;
    }
    return I->state == INST_DONE ? 0 : 1;
}

int inst_percent(const inst *I) {
    if (I->state == INST_DONE)
        return 100;
    if (!I->units_total)
        return 0;
    inst_u32 p = (inst_u32) (((unsigned long long) I->units_done * 100u) / I->units_total);
    return p > 99 ? 99 : (int) p;
}

/* ------------------------------------------------------------------ *
 *  inst_probe                                                         *
 * ------------------------------------------------------------------ */

static char *put_str(char *d, const char *s) {
    while (*s)
        *d++ = *s++;
    return d;
}

void inst_probe(const inst_dev *dv, char *out, int n) {
    inst_u8 s[SECTOR], t[SECTOR];
    char tmp[64], *d = tmp;
    tmp[0] = 0;

    if (dv->read(dv->ctx, 0, s, 1)) {
        d = put_str(d, "unreadable");
    } else {
        int zero = 1;
        for (inst_u32 i = 0; i < SECTOR; i++)
            if (s[i]) { zero = 0; break; }
        int sig = s[510] == 0x55 && s[511] == 0xAA;

        if (zero) {
            d = put_str(d, "blank");
        } else if (sig && s[0x1C2] == 0xEE && dv->sectors > 1 && !dv->read(dv->ctx, 1, t, 1) &&
                   !MEMCMP(t, "EFI PART", 8)) {
            /* count the used GPT entries (the first 16 are enough to say "some") */
            inst_u8 e[SECTOR * 4];
            int used = 0;
            if (!dv->read(dv->ctx, 2, e, 4))
                for (int i = 0; i < 16; i++)
                    for (int k = 0; k < 16; k++)
                        if (e[i * 128 + k]) { used++; break; }
            d = put_str(d, "GPT, ");
            d = put_dec(d, (inst_u32) used);
            d = put_str(d, used == 1 ? " partition" : " partitions");
        } else if (sig && get16(s + 11) == SECTOR && s[13] && s[16] && (s[0] == 0xEB || s[0] == 0xE9)) {
            d = put_str(d, "FAT volume");
        } else if (sig) {
            int used = 0;
            for (int i = 0; i < 4; i++)
                if (s[0x1BE + 16 * i + 4])
                    used++;
            d = put_str(d, "MBR, ");
            d = put_dec(d, (inst_u32) used);
            d = put_str(d, used == 1 ? " partition" : " partitions");
        } else {
            d = put_str(d, "unknown data");
        }
    }
    *d = 0;
    int i = 0;
    for (; tmp[i] && i < n - 1; i++)
        out[i] = tmp[i];
    out[i] = 0;
}
