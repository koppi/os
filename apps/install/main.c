/**
 * @file apps/install/main.c
 * @brief The graphical installer: a wizard in ring 3, drawn with microui.
 *
 * Six pages -- Welcome, Disk, Settings, Review, Install, Finish -- over the engine in
 * engine.c, which does the actual work and knows nothing about any of this. The
 * installer reaches the machine through sys.c: the block syscalls (blk.h) for the disks,
 * the boot RAM disk and the installation payload, and `reboot` at the end.
 *
 * It is a window on the desktop when there is one (`install`), and the whole screen
 * when asked (`install -f`) or when there is no desktop; the "Install" entry of the
 * boot menu starts it the second way, before any desktop exists. Esc quits, except
 * while a disk is being written.
 *
 * The layout is fixed, 640 x 456: that is what fits a 640 x 480 surface once the
 * full-screen path has added its own title bar (see ../microui/mui.h), and it is a
 * comfortable size on every screen this runs on.
 */
#include <linux/printf.h>

#include "mui.h"
#include "engine.h"
#include "sys.h"

#define WIN_W    640
#define WIN_H    456

#define BANNER_H 60
#define BAR_H    52
#define SIDE_W   148

/* ------------------------------------------------------------------ *
 *  Colours                                                            *
 * ------------------------------------------------------------------ */

#define RGB(r, g, b) ((mu_Color) { (r), (g), (b), 255 })

static const mu_Color C_BG      = RGB(22, 27, 38);
static const mu_Color C_PANEL   = RGB(30, 37, 52);
static const mu_Color C_PANEL2  = RGB(37, 45, 63);
static const mu_Color C_BORDER  = RGB(58, 70, 96);
static const mu_Color C_TEXT    = RGB(226, 232, 244);
static const mu_Color C_DIM     = RGB(140, 152, 176);
static const mu_Color C_ACCENT  = RGB(74, 144, 255);
static const mu_Color C_ACCENT2 = RGB(40, 98, 200);
static const mu_Color C_GOOD    = RGB(96, 200, 130);
static const mu_Color C_BAD     = RGB(244, 108, 108);
static const mu_Color C_WARN    = RGB(240, 192, 96);

/** The microui theme: set once, on the first frame, when the context exists. */
static void apply_theme(mu_Context *ctx) {
    mu_Style *st = ctx->style;
    st->colors[MU_COLOR_TEXT]        = C_TEXT;
    st->colors[MU_COLOR_BORDER]      = C_BORDER;
    st->colors[MU_COLOR_WINDOWBG]    = C_BG;
    st->colors[MU_COLOR_TITLEBG]     = RGB(14, 18, 28);
    st->colors[MU_COLOR_TITLETEXT]   = C_TEXT;
    st->colors[MU_COLOR_PANELBG]     = C_PANEL;
    st->colors[MU_COLOR_BUTTON]      = RGB(52, 64, 90);
    st->colors[MU_COLOR_BUTTONHOVER] = RGB(66, 80, 110);
    st->colors[MU_COLOR_BUTTONFOCUS] = RGB(80, 96, 130);
    st->colors[MU_COLOR_BASE]        = RGB(18, 23, 34);
    st->colors[MU_COLOR_BASEHOVER]   = RGB(24, 30, 44);
    st->colors[MU_COLOR_BASEFOCUS]   = RGB(30, 38, 56);
    st->colors[MU_COLOR_SCROLLBASE]  = RGB(24, 30, 44);
    st->colors[MU_COLOR_SCROLLTHUMB] = RGB(70, 84, 114);
}

/* ------------------------------------------------------------------ *
 *  The installer's state                                              *
 * ------------------------------------------------------------------ */

enum { PG_WELCOME, PG_DISK, PG_SETTINGS, PG_REVIEW, PG_INSTALL, PG_FINISH, PG_COUNT };

static const char *const STEP_NAMES[PG_COUNT] = {
    "Welcome", "Disk", "Settings", "Review", "Install", "Finish"
};

#define MAX_DISKS 8

typedef struct {
    int                idx;             /**< the kernel's block device number */
    struct blk_info    bi;
    char               contents[48];    /**< what is on it now (inst_probe) */
    inst_u32           min_mib;         /**< what the system needs, if it fits at all */
    int                fits;
    char               why[96];         /**< when it does not */
} disk_t;

static struct {
    int      page;
    int      ready;                     /**< payload and RAM disk opened */
    char     problem[96];               /**< why not */
    int      pl_idx, rd_idx;

    disk_t   disks[MAX_DISKS];
    int      ndisks;
    int      sel;                       /**< index into disks[], -1 for none */

    float    size_mib;                  /**< the slider */
    int      whole;                     /**< "use all the space" */
    char     label[12];
    int      agree;

    int      running;                   /**< a disk is being written: Esc must not quit */
    int      result;                    /**< 0 not yet, 1 installed, -1 failed */
    int      mounted_check;             /**< 1 if the kernel mounted the new volume afterwards */
    unsigned t_start, t_end;
    char     log[6][56];
    int      nlog;
    const char *last_phase;
} A = { .sel = -1, .label = "KOPPI-OS", .whole = 1 };

static inst g_inst;                     /**< about 1.2 MiB: .bss */

/** @brief The text of an error that outlives the call that made it. */
static void set_problem(const char *s) {
    int i = 0;
    for (; s[i] && i < (int) sizeof A.problem - 1; i++)
        A.problem[i] = s[i];
    A.problem[i] = 0;
}

/* ------------------------------------------------------------------ *
 *  Small helpers                                                      *
 * ------------------------------------------------------------------ */

/** @brief "1.0 GiB", "512 MiB": a size in sectors, for people. */
static void fmt_size(char *out, int n, inst_u32 sectors) {
    inst_u32 mib = sectors / 2048;
    if (mib >= 1024)
        snprintf(out, (size_t) n, "%u.%u GiB", mib / 1024, (mib % 1024) * 10 / 1024);
    else
        snprintf(out, (size_t) n, "%u MiB", mib);
}

static const char *bus_name(int kind) {
    switch (kind) {
    case BLK_KIND_ATA:  return "IDE";
    case BLK_KIND_AHCI: return "SATA";
    case BLK_KIND_NVME: return "NVMe";
    default:            return "disk";
    }
}

/** @brief Replace what a FAT label may not hold, and cut to 11 characters. */
static void clean_label(char *dst, const char *src) {
    int n = 0;
    for (int i = 0; src[i] && n < 11; i++) {
        char c = src[i];
        if (c < ' ' || c == '"' || c == '*' || c == '+' || c == ',' || c == '.' || c == '/' ||
            c == ':' || c == ';' || c == '<' || c == '=' || c == '>' || c == '?' || c == '[' ||
            c == '\\' || c == ']' || c == '|')
            c = '-';
        dst[n++] = c;
    }
    if (n == 0)
        for (const char *d = "KOPPI-OS"; *d; d++)
            dst[n++] = *d;
    dst[n] = 0;
}

static void text_c(mu_Context *ctx, mu_Color c, const char *s) {
    mu_Color old = ctx->style->colors[MU_COLOR_TEXT];
    ctx->style->colors[MU_COLOR_TEXT] = c;
    mu_text(ctx, s);
    ctx->style->colors[MU_COLOR_TEXT] = old;
}

static void label_c(mu_Context *ctx, mu_Color c, const char *s) {
    mu_Color old = ctx->style->colors[MU_COLOR_TEXT];
    ctx->style->colors[MU_COLOR_TEXT] = c;
    mu_label(ctx, s);
    ctx->style->colors[MU_COLOR_TEXT] = old;
}

static void row1(mu_Context *ctx, int h) {
    static const int w[1] = { -1 };
    mu_layout_row(ctx, 1, w, h);
}

/**
 * Put the next widget at (x, y, w, h) of the window, whatever the flow says. Positions are
 * relative to the layout's body, which microui insets by the style's padding from the
 * window's own edge, so step back out by that much to count from the edge itself.
 */
static void place(mu_Context *ctx, int x, int y, int w, int h) {
    int pad = ctx->style->padding;
    mu_layout_set_next(ctx, mu_rect(x - pad, y - pad, w, h), 1);
}

/* ------------------------------------------------------------------ *
 *  Drawn by hand: the banner, the step list, headings, the progress bar *
 * ------------------------------------------------------------------ */

static mu_Color lerp(mu_Color a, mu_Color b, int num, int den) {
    return RGB(a.r + (b.r - a.r) * num / den, a.g + (b.g - a.g) * num / den,
               a.b + (b.b - a.b) * num / den);
}

static void draw_banner(const mui_Surface *s, mu_Rect r, void *ud) {
    (void) ud;
    mu_Color top = RGB(30, 66, 134), bot = RGB(18, 34, 76);
    for (int y = 0; y < r.h; y++)
        mui_fill(s, mu_rect(r.x, r.y + y, r.w, 1), lerp(top, bot, y, r.h));
    mui_fill(s, mu_rect(r.x, r.y + r.h - 3, r.w, 3), C_ACCENT);

    /* the mark: a small disc with a bite out of it, next to the name */
    mui_disc(s, r.x + 34, r.y + 28, 17, RGB(255, 255, 255));
    mui_disc(s, r.x + 34, r.y + 28, 12, RGB(30, 66, 134));
    mui_disc(s, r.x + 34, r.y + 28, 5, RGB(255, 255, 255));

    mui_string_scaled(s, r.x + 64, r.y + 12, "koppi's os", 2, RGB(255, 255, 255));
    const char *sub = "Installer";
    mui_string(s, r.x + r.w - 16 - mui_text_width(sub, -1), r.y + 18, sub, RGB(190, 208, 240));
}

static void draw_heading(const mui_Surface *s, mu_Rect r, void *ud) {
    mui_string_scaled(s, r.x, r.y + 2, (const char *) ud, 2, C_TEXT);
}

static void draw_sidebar(const mui_Surface *s, mu_Rect r, void *ud) {
    int page = *(const int *) ud;
    mui_fill(s, r, RGB(17, 21, 31));
    mui_fill(s, mu_rect(r.x + r.w - 1, r.y, 1, r.h), C_BORDER);

    for (int i = 0; i < PG_COUNT; i++) {
        int y = r.y + 18 + i * 40;
        int cx = r.x + 26, cy = y + 12;
        mu_Color txt = i == page ? C_TEXT : (i < page ? C_DIM : RGB(96, 108, 132));

        if (i == page)
            mui_fill(s, mu_rect(r.x, y - 6, r.w - 1, 36), RGB(30, 40, 62));
        if (i == page)
            mui_fill(s, mu_rect(r.x, y - 6, 3, 36), C_ACCENT);

        if (i < page) {
            mui_disc(s, cx, cy, 11, C_GOOD);
            mui_line(s, cx - 5, cy, cx - 1, cy + 4, 2, RGB(17, 21, 31));
            mui_line(s, cx - 1, cy + 4, cx + 6, cy - 4, 2, RGB(17, 21, 31));
        } else if (i == page) {
            mui_disc(s, cx, cy, 11, C_ACCENT);
            char d[2] = { (char) ('1' + i), 0 };
            mui_string(s, cx - 4, cy - 8, d, RGB(255, 255, 255));
        } else {
            mui_ring(s, cx, cy, 11, 2, RGB(70, 82, 108));
            char d[2] = { (char) ('1' + i), 0 };
            mui_string(s, cx - 4, cy - 8, d, RGB(96, 108, 132));
        }
        mui_string(s, r.x + 48, y + 4, STEP_NAMES[i], txt);
    }
}

typedef struct { int pct; int failed; } bar_t;

static void draw_bar(const mui_Surface *s, mu_Rect r, void *ud) {
    const bar_t *b = ud;
    mui_fill(s, r, RGB(14, 18, 27));
    mui_frame(s, r, C_BORDER);
    int w = (r.w - 4) * b->pct / 100;
    mu_Color c = b->failed ? C_BAD : C_ACCENT;
    if (w > 0) {
        mui_fill(s, mu_rect(r.x + 2, r.y + 2, w, r.h - 4), c);
        mui_fill(s, mu_rect(r.x + 2, r.y + 2, w, (r.h - 4) / 2), lerp(c, RGB(255, 255, 255), 1, 6));
    }
}

static void heading(mu_Context *ctx, const char *text) {
    row1(ctx, 36);
    mui_draw_custom(ctx, mu_layout_next(ctx), draw_heading, (void *) text);
}

/* ------------------------------------------------------------------ *
 *  Disks                                                              *
 * ------------------------------------------------------------------ */

/** Build the engine's view of disk @p d for a plan. */
static void disk_dev(const disk_t *d, inst_dev *dev) {
    sys_blk_dev(dev, (unsigned) d->idx, d->bi.sectors, 1);
}

static void plan_opts(inst_opts *o, inst_u32 part_mib) {
    o->part_mib = part_mib;
    clean_label(o->label, A.label);
    o->unix_time = sys_unix();
    o->random = (void (*)(void *, inst_u32)) sys_random;
}

/** Look for installable disks and find out what is on each. */
static void scan_disks(void) {
    A.ndisks = 0;
    A.pl_idx = A.rd_idx = -1;
    for (int i = 0; i < BLK_MAX; i++) {
        struct blk_info bi;
        if (!sys_blk_info((unsigned) i, &bi))
            continue;
        if (bi.kind == BLK_KIND_PAYLOAD) A.pl_idx = i;
        if (bi.kind == BLK_KIND_RAMDISK) A.rd_idx = i;
        if (!(bi.flags & BLK_F_TARGET) || A.ndisks >= MAX_DISKS)
            continue;

        disk_t *d = &A.disks[A.ndisks++];
        d->idx = i;
        d->bi = bi;
        inst_dev dev;
        disk_dev(d, &dev);
        inst_probe(&dev, d->contents, sizeof d->contents);
    }
    A.sel = -1;
}

/** What each disk can hold, now that the payload is known. */
static void size_disks(void) {
    for (int i = 0; i < A.ndisks; i++) {
        disk_t *d = &A.disks[i];
        inst_dev dev;
        inst_opts o;
        disk_dev(d, &dev);
        plan_opts(&o, 0);
        d->fits = inst_plan(&g_inst, &dev, &o) == 0;
        d->min_mib = g_inst.min_mib;
        d->why[0] = 0;
        if (d->fits && A.ndisks == 1)
            A.sel = 0;                  /* the only disk, and it will do */
        if (!d->fits) {
            int k = 0;
            for (; g_inst.err[k] && k < (int) sizeof d->why - 1; k++)
                d->why[k] = g_inst.err[k];
            d->why[k] = 0;
        }
    }
}

/** Open the payload and the RAM disk, then look at the disks. */
static void startup(void) {
    scan_disks();
    A.ready = 0;
    if (A.pl_idx < 0) {
        set_problem("This is not the installer's boot: there is no installation payload. "
                    "Restart and choose \"Install koppi's os to a disk\" in the boot menu.");
        return;
    }
    if (A.rd_idx < 0) {
        set_problem("The boot RAM disk is missing, so there is nothing to install.");
        return;
    }
    struct blk_info pi, ri;
    sys_blk_info((unsigned) A.pl_idx, &pi);
    sys_blk_info((unsigned) A.rd_idx, &ri);
    inst_dev pl, rd;
    sys_blk_dev(&pl, (unsigned) A.pl_idx, pi.sectors, 0);
    sys_blk_dev(&rd, (unsigned) A.rd_idx, ri.sectors, 0);
    if (inst_open(&g_inst, &pl, &rd)) {
        set_problem(g_inst.err);
        return;
    }
    A.ready = 1;
    size_disks();
}

/* ------------------------------------------------------------------ *
 *  Installing                                                         *
 * ------------------------------------------------------------------ */

static void log_line(const char *s) {
    if (A.nlog == (int) (sizeof A.log / sizeof A.log[0])) {
        for (int i = 1; i < A.nlog; i++)
            for (int k = 0; k < (int) sizeof A.log[0]; k++)
                A.log[i - 1][k] = A.log[i][k];
        A.nlog--;
    }
    int k = 0;
    for (; s[k] && k < (int) sizeof A.log[0] - 1; k++)
        A.log[A.nlog][k] = s[k];
    A.log[A.nlog][k] = 0;
    A.nlog++;
}

/** Plan with what the user finally chose, and begin. */
static int start_install(void) {
    disk_t *d = &A.disks[A.sel];
    inst_dev dev;
    inst_opts o;
    disk_dev(d, &dev);
    plan_opts(&o, A.whole ? 0 : (inst_u32) A.size_mib);
    if (inst_plan(&g_inst, &dev, &o)) {
        A.result = -1;
        set_problem(g_inst.err);
        return -1;
    }
    A.running = 1;
    A.result = 0;
    A.nlog = 0;
    A.last_phase = "";
    A.t_start = mui_ms();
    return 0;
}

/** Do as much of the install as fits in one frame. */
static void pump_install(void) {
    unsigned t0 = mui_ms();
    int r = 1;
    while (r == 1 && mui_ms() - t0 < 10) {
        r = inst_step(&g_inst);
        if (g_inst.phase != A.last_phase && g_inst.phase[0]) {
            log_line(g_inst.phase);
            A.last_phase = g_inst.phase;
        }
    }
    if (r == 1)
        return;

    A.t_end = mui_ms();
    A.running = 0;
    if (r < 0) {
        A.result = -1;
        set_problem(g_inst.err);
        printf("install: FAILED: %s\n", g_inst.err);
    } else {
        A.result = 1;
        /* Does the kernel see what was written? Its own partition and FAT code is
         * the independent check on the work the engine just did. */
        A.mounted_check = sys_blk_rescan((unsigned) A.disks[A.sel].idx);
        printf("install: done in %u ms; the kernel %s the new volume\n",
               A.t_end - A.t_start, A.mounted_check ? "mounted" : "could not mount");
    }
    A.page = PG_FINISH;
}

/** Esc and the close box: not while the disk is half written. */
static int may_quit(void) {
    return !A.running;
}

/* ------------------------------------------------------------------ *
 *  Pages                                                              *
 * ------------------------------------------------------------------ */

static void page_welcome(mu_Context *ctx) {
    heading(ctx, "Welcome");
    row1(ctx, 0);
    mu_text(ctx, "This installs koppi's os on a disk of this computer. It partitions "
                 "the disk, installs the GRUB boot loader so the machine starts from it "
                 "-- on BIOS and on UEFI alike -- and copies the system across.");
    row1(ctx, 8);
    mu_layout_next(ctx);
    row1(ctx, 0);
    text_c(ctx, C_DIM, "Nothing is written until you confirm on the review page. "
                       "Whatever is on the disk you choose will be erased.");
    row1(ctx, 14);
    mu_layout_next(ctx);

    if (A.ready) {
        static char line[96];
        snprintf(line, sizeof line, "Installation files: %s",
                 g_inst.version[0] ? g_inst.version : "present");
        row1(ctx, 20);
        label_c(ctx, C_GOOD, line);
        snprintf(line, sizeof line, "Disks found: %d", A.ndisks);
        row1(ctx, 20);
        label_c(ctx, A.ndisks ? C_TEXT : C_WARN, line);
        if (!A.ndisks) {
            row1(ctx, 0);
            text_c(ctx, C_WARN, "No disk the installer can write to was found. It handles IDE, "
                                "SATA (AHCI) and NVMe disks; USB drives are not supported yet.");
        }
    } else {
        row1(ctx, 0);
        text_c(ctx, C_BAD, A.problem);
    }
    row1(ctx, 14);
    mu_layout_next(ctx);
    row1(ctx, 0);
    text_c(ctx, C_DIM, "Enter goes to the next page. Esc quits the installer.");
}

static void disk_card(mu_Context *ctx, int i) {
    disk_t *d = &A.disks[i];
    static const int w[1] = { -1 };
    mu_layout_row(ctx, 1, w, 50);
    mu_Rect r = mu_layout_next(ctx);
    mu_Id id = mu_get_id(ctx, &d->idx, sizeof d->idx);
    mu_update_control(ctx, id, r, 0);
    if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id && d->fits) {
        A.sel = i;
        A.agree = 0;
    }

    int selected = A.sel == i;
    mu_Color bg = selected ? RGB(36, 56, 98) : (ctx->hover == id ? C_PANEL2 : RGB(26, 32, 46));
    mu_draw_rect(ctx, r, selected ? C_ACCENT : C_BORDER);
    mu_draw_rect(ctx, mu_rect(r.x + 1, r.y + 1, r.w - 2, r.h - 2), bg);

    /* a little drive */
    mu_Color body = d->fits ? RGB(150, 164, 190) : RGB(90, 98, 118);
    mu_draw_rect(ctx, mu_rect(r.x + 10, r.y + 14, 34, 22), body);
    mu_draw_rect(ctx, mu_rect(r.x + 12, r.y + 16, 30, 3), RGB(70, 80, 104));
    mu_draw_rect(ctx, mu_rect(r.x + 36, r.y + 28, 4, 4), d->fits ? C_GOOD : C_BAD);

    static char l1[96], l2[112];
    char size[16];
    fmt_size(size, sizeof size, d->bi.sectors);
    snprintf(l1, sizeof l1, "%s   %s", d->bi.name, d->bi.model[0] ? d->bi.model : "disk");
    snprintf(l2, sizeof l2, "%s  %s  holds: %s", size, bus_name((int) d->bi.kind), d->contents);

    mu_Font f = ctx->style->font;
    mu_draw_text(ctx, f, l1, -1, mu_vec2(r.x + 56, r.y + 7), C_TEXT);
    mu_draw_text(ctx, f, d->fits ? l2 : d->why, -1, mu_vec2(r.x + 56, r.y + 27),
                 d->fits ? C_DIM : C_BAD);
}

static void page_disk(mu_Context *ctx) {
    heading(ctx, "Choose a disk");
    row1(ctx, 0);
    text_c(ctx, C_DIM, "koppi's os takes a whole disk. Everything on it is erased.");
    row1(ctx, 6);
    mu_layout_next(ctx);

    static const int w[1] = { -1 };
    mu_layout_row(ctx, 1, w, 178);
    mu_begin_panel_ex(ctx, "disks", 0);
    if (A.ndisks == 0) {
        row1(ctx, 0);
        text_c(ctx, C_WARN, "No installable disk found.");
    }
    for (int i = 0; i < A.ndisks; i++)
        disk_card(ctx, i);
    mu_end_panel(ctx);

    row1(ctx, 28);
    static const int w2[2] = { 100, -1 };
    mu_layout_row(ctx, 2, w2, 26);
    if (mu_button_ex(ctx, "Rescan", 0, MU_OPT_ALIGNCENTER)) {
        scan_disks();
        if (A.ready)
            size_disks();
    }
    if (A.sel >= 0)
        label_c(ctx, C_DIM, "Selected: click Next to continue.");
    else
        label_c(ctx, C_DIM, "Click a disk to select it.");
}

static void page_settings(mu_Context *ctx) {
    disk_t *d = &A.disks[A.sel];
    heading(ctx, "Settings");

    /* The plan for this disk gives the real bounds of the volume. */
    inst_u32 mn = d->min_mib, mx = d->min_mib;
    {
        inst_dev dev;
        inst_opts o;
        disk_dev(d, &dev);
        plan_opts(&o, 0);
        if (inst_plan(&g_inst, &dev, &o) == 0) {
            mn = g_inst.min_mib;
            mx = g_inst.max_mib;
        }
    }
    if (A.whole || A.size_mib < (float) mn || A.size_mib > (float) mx)
        A.size_mib = (float) mx;

    char line[112];
    snprintf(line, sizeof line, "System volume size, %u to %u MiB", mn, mx);
    row1(ctx, 22);
    mu_label(ctx, line);

    static const int ws[1] = { -1 };
    mu_layout_row(ctx, 1, ws, 26);
    mu_Real v = A.size_mib;
    if (mu_slider_ex(ctx, &v, (mu_Real) mn, (mu_Real) mx, 1, "%.0f MiB",
                     A.whole ? MU_OPT_NOINTERACT | MU_OPT_ALIGNCENTER : MU_OPT_ALIGNCENTER) & MU_RES_CHANGE)
        A.size_mib = v;

    row1(ctx, 24);
    mu_checkbox(ctx, "Use all the space a FAT16 volume can have", &A.whole);

    row1(ctx, 10);
    mu_layout_next(ctx);

    static const int wl[2] = { 150, 180 };
    mu_layout_row(ctx, 2, wl, 26);
    mu_label(ctx, "Volume name");
    mu_textbox_ex(ctx, A.label, sizeof A.label, 0);

    row1(ctx, 14);
    mu_layout_next(ctx);
    row1(ctx, 0);
    text_c(ctx, C_DIM, "The volume holds the kernel, the boot menu and the RAM disk image, and "
                       "afterwards it is /hda, the persistent storage of the installed system.");
    /* Only worth a word when it is FAT16's ceiling that stops the volume, not the disk. */
    inst_u32 disk_mib = d->bi.sectors / 2048;
    if (mx >= 2047 && disk_mib > mx + 64) {
        row1(ctx, 8);
        mu_layout_next(ctx);
        row1(ctx, 0);
        char big[96], sz[16];
        fmt_size(sz, sizeof sz, (disk_mib - mx - 4) * 2048);
        snprintf(big, sizeof big, "A FAT16 volume holds at most 2 GiB; the other %s is left unpartitioned.", sz);
        text_c(ctx, C_WARN, big);
    }
}

static void summary_row(mu_Context *ctx, const char *key, const char *val) {
    static const int w[2] = { 92, -1 };
    mu_layout_row(ctx, 2, w, 20);
    label_c(ctx, C_DIM, key);
    mu_label(ctx, val);
}

static void page_review(mu_Context *ctx) {
    disk_t *d = &A.disks[A.sel];
    heading(ctx, "Ready to install");

    /* the real numbers come from the plan */
    inst_dev dev;
    inst_opts o;
    disk_dev(d, &dev);
    plan_opts(&o, A.whole ? 0 : (inst_u32) A.size_mib);
    int ok = inst_plan(&g_inst, &dev, &o) == 0;

    char sz[16], a[112], b[112];
    fmt_size(sz, sizeof sz, d->bi.sectors);
    snprintf(a, sizeof a, "%s  %s  (%s, %s)", d->bi.name, d->bi.model[0] ? d->bi.model : "disk", sz,
             bus_name((int) d->bi.kind));
    summary_row(ctx, "Disk", a);
    if (ok) {
        snprintf(a, sizeof a, "GPT: 1 MiB BIOS boot + %u MiB system volume", g_inst.part_mib);
        summary_row(ctx, "Layout", a);
        clean_label(b, A.label);
        snprintf(a, sizeof a, "FAT16 \"%s\", %u KiB clusters", b, g_inst.cs / 2);
        summary_row(ctx, "Volume", a);
    }
    summary_row(ctx, "Boot", "GRUB, for BIOS and for UEFI");
    if (g_inst.version[0])
        summary_row(ctx, "System", g_inst.version);

    row1(ctx, 10);
    mu_layout_next(ctx);

    static const int w[1] = { -1 };
    mu_layout_row(ctx, 1, w, 78);
    mu_begin_panel_ex(ctx, "warn", 0);
    row1(ctx, 0);
    snprintf(a, sizeof a, "%s currently holds: %s.", d->bi.name, d->contents);
    text_c(ctx, C_WARN, a);
    row1(ctx, 0);
    snprintf(a, sizeof a, "ALL OF IT WILL BE ERASED when you click Install.");
    text_c(ctx, C_BAD, a);
    mu_end_panel(ctx);

    row1(ctx, 28);
    snprintf(a, sizeof a, "I understand that everything on %s will be erased", d->bi.name);
    mu_checkbox(ctx, a, &A.agree);
    if (!ok) {
        row1(ctx, 0);
        text_c(ctx, C_BAD, g_inst.err);
    }
}

static void page_install(mu_Context *ctx) {
    heading(ctx, "Installing");

    char line[96];
    row1(ctx, 22);
    label_c(ctx, C_TEXT, g_inst.phase && g_inst.phase[0] ? g_inst.phase : "Starting");

    static bar_t bar;
    bar.pct = inst_percent(&g_inst);
    bar.failed = 0;
    row1(ctx, 22);
    mui_draw_custom(ctx, mu_layout_next(ctx), draw_bar, &bar);

    static const int w[2] = { 120, -1 };
    mu_layout_row(ctx, 2, w, 20);
    snprintf(line, sizeof line, "%d%%", bar.pct);
    mu_label(ctx, line);
    unsigned secs = (mui_ms() - A.t_start) / 1000;
    snprintf(line, sizeof line, "%u:%02u elapsed", secs / 60, secs % 60);
    label_c(ctx, C_DIM, line);

    row1(ctx, 10);
    mu_layout_next(ctx);
    for (int i = 0; i < A.nlog; i++) {
        row1(ctx, 18);
        snprintf(line, sizeof line, "%s %s", i + 1 < A.nlog ? "[x]" : "[>]", A.log[i]);
        label_c(ctx, i + 1 < A.nlog ? C_DIM : C_TEXT, line);
    }
    row1(ctx, 14);
    mu_layout_next(ctx);
    row1(ctx, 0);
    text_c(ctx, C_WARN, "Do not switch off or reset the computer until this is finished.");
}

static void page_finish(mu_Context *ctx) {
    char line[128];
    if (A.result == 1) {
        heading(ctx, "Installation complete");
        row1(ctx, 0);
        snprintf(line, sizeof line, "koppi's os is installed on %s.", A.disks[A.sel].bi.name);
        text_c(ctx, C_GOOD, line);
        row1(ctx, 8);
        mu_layout_next(ctx);
        row1(ctx, 0);
        mu_text(ctx, "Remove the installation medium, then restart. If the machine starts the "
                     "medium again, pick the new disk in the firmware's boot menu.");
        row1(ctx, 10);
        mu_layout_next(ctx);
        unsigned ms = A.t_end - A.t_start;
        snprintf(line, sizeof line, "Installed and verified in %u.%u seconds.", ms / 1000, (ms % 1000) / 100);
        row1(ctx, 20);
        label_c(ctx, C_DIM, line);
        row1(ctx, 20);
        if (A.mounted_check)
            label_c(ctx, C_DIM, "The kernel mounts the new volume: it will be /hda.");
        else
            label_c(ctx, C_WARN, "The kernel could not mount the new volume right now.");
    } else {
        heading(ctx, "Installation failed");
        row1(ctx, 0);
        text_c(ctx, C_BAD, A.problem);
        row1(ctx, 8);
        mu_layout_next(ctx);
        row1(ctx, 0);
        mu_text(ctx, "The disk may be partly written and is not bootable. Go back and try "
                     "again, or choose another disk.");
    }
}

/* ------------------------------------------------------------------ *
 *  The frame                                                          *
 * ------------------------------------------------------------------ */

/** A button in the bar at the bottom. @return non-zero when it was pressed. */
static int bar_button(mu_Context *ctx, int x, int w, const char *label, int enabled, int primary) {
    mu_Style *st = ctx->style;
    mu_Color b = st->colors[MU_COLOR_BUTTON], h = st->colors[MU_COLOR_BUTTONHOVER],
             f = st->colors[MU_COLOR_BUTTONFOCUS], t = st->colors[MU_COLOR_TEXT];
    if (!enabled) {
        st->colors[MU_COLOR_BUTTON] = st->colors[MU_COLOR_BUTTONHOVER] =
            st->colors[MU_COLOR_BUTTONFOCUS] = RGB(34, 40, 54);
        st->colors[MU_COLOR_TEXT] = RGB(90, 100, 122);
    } else if (primary) {
        st->colors[MU_COLOR_BUTTON] = C_ACCENT2;
        st->colors[MU_COLOR_BUTTONHOVER] = C_ACCENT;
        st->colors[MU_COLOR_BUTTONFOCUS] = RGB(100, 164, 255);
        st->colors[MU_COLOR_TEXT] = RGB(255, 255, 255);
    }
    place(ctx, x, WIN_H - BAR_H + 10, w, 32);
    int r = mu_button_ex(ctx, label, 0, MU_OPT_ALIGNCENTER | (enabled ? 0 : MU_OPT_NOINTERACT)) & MU_RES_SUBMIT;
    st->colors[MU_COLOR_BUTTON] = b;
    st->colors[MU_COLOR_BUTTONHOVER] = h;
    st->colors[MU_COLOR_BUTTONFOCUS] = f;
    st->colors[MU_COLOR_TEXT] = t;
    return r && enabled;
}

static void draw_rule(const mui_Surface *s, mu_Rect r, void *ud) {
    (void) ud;
    mui_fill(s, r, RGB(17, 21, 31));
    mui_fill(s, mu_rect(r.x, r.y, r.w, 1), C_BORDER);
}

static void go(int page) {
    A.page = page;
}

static void frame(mu_Context *ctx, void *udata) {
    (void) udata;
    static int themed;
    if (!themed) {
        apply_theme(ctx);
        themed = 1;
    }

    if (A.page == PG_INSTALL && A.running)
        pump_install();

    /* Enter is "Next", unless a text box has the keyboard; Space ticks the box on the review page. */
    int enter = (ctx->key_pressed & MU_KEY_RETURN) && ctx->focus == 0;
    if (A.page == PG_REVIEW && ctx->focus == 0 && ctx->input_text[0] == ' ')
        A.agree = !A.agree;

    /* -- chrome ------------------------------------------------------ */
    place(ctx, 0, 0, WIN_W, BANNER_H);
    mui_draw_custom(ctx, mu_layout_next(ctx), draw_banner, 0);
    place(ctx, 0, BANNER_H, SIDE_W, WIN_H - BANNER_H - BAR_H);
    mui_draw_custom(ctx, mu_layout_next(ctx), draw_sidebar, &A.page);
    place(ctx, 0, WIN_H - BAR_H, WIN_W, BAR_H);
    mui_draw_custom(ctx, mu_layout_next(ctx), draw_rule, 0);

    /* -- the page ---------------------------------------------------- */
    char name[8] = { 'p', 'g', (char) ('0' + A.page), 0 };
    place(ctx, SIDE_W, BANNER_H, WIN_W - SIDE_W, WIN_H - BANNER_H - BAR_H);
    mu_begin_panel_ex(ctx, name, MU_OPT_NOFRAME | MU_OPT_NOSCROLL);
    switch (A.page) {
    case PG_WELCOME:  page_welcome(ctx);  break;
    case PG_DISK:     page_disk(ctx);     break;
    case PG_SETTINGS: page_settings(ctx); break;
    case PG_REVIEW:   page_review(ctx);   break;
    case PG_INSTALL:  page_install(ctx);  break;
    default:          page_finish(ctx);   break;
    }
    mu_end_panel(ctx);

    /* -- the buttons ------------------------------------------------- */
    int back_ok = A.page == PG_DISK || A.page == PG_SETTINGS || A.page == PG_REVIEW ||
                  (A.page == PG_FINISH && A.result < 0);
    int next_ok = 0;
    const char *next_label = "Next >";
    switch (A.page) {
    case PG_WELCOME:  next_ok = A.ready && A.ndisks > 0; break;
    case PG_DISK:     next_ok = A.sel >= 0 && A.disks[A.sel].fits; break;
    case PG_SETTINGS: next_ok = 1; break;
    case PG_REVIEW:   next_ok = A.agree; next_label = "Install"; break;
    case PG_FINISH:   next_ok = A.result == 1; next_label = "Restart"; break;
    default: break;
    }

    int quit_ok = !A.running;
    if (bar_button(ctx, 16, 96, A.page == PG_FINISH ? "Quit" : "Cancel", quit_ok, 0))
        mui_quit();
    int back = bar_button(ctx, WIN_W - 238, 108, "< Back", back_ok, 0);
    int next = bar_button(ctx, WIN_W - 122, 108, next_label, next_ok, 1) || (enter && next_ok);

    if (back) {
        if (A.page == PG_FINISH) {
            A.result = 0;
            go(PG_REVIEW);
        } else {
            go(A.page - 1);
        }
    }
    if (next) {
        switch (A.page) {
        case PG_WELCOME:  go(A.ndisks == 1 ? PG_DISK : PG_DISK); break;
        case PG_DISK:     go(PG_SETTINGS); break;
        case PG_SETTINGS: go(PG_REVIEW); break;
        case PG_REVIEW:
            if (start_install() == 0)
                go(PG_INSTALL);
            else
                go(PG_FINISH);
            break;
        case PG_FINISH:
            sys_reboot();
            break;
        default: break;
        }
    }
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++)
        if (argv[i] && argv[i][0] == '-' && argv[i][1] == 'f')
            mui_fullscreen(1);

    startup();
    mui_set_quit_guard(may_quit);

    if (mui_run("Install koppi's os", WIN_W, WIN_H, frame, 0) < 0) {
        printf("install: no window and no screen -- is there a framebuffer, and "
               "is another program holding it?\n");
        return 1;
    }
    return 0;
}
