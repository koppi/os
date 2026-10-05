/**
 * @file apps/clock/main.c
 * @brief An analog clock, in ring 3, drawn with microui.
 *
 * The window, its controls and its layout are the vendored microui toolkit --
 * the same one the kernel's own desktop is built from -- running in a
 * userspace process over the full-screen grab; ../microui/mui.h describes
 * that runtime.
 *
 * The face itself is not a microui widget, because microui has no widget that
 * could draw it: its command list carries rectangles, text and four built-in
 * icons, and nothing else. It goes in as a custom-draw command
 * (@ref mui_draw_custom) instead, which is painted in command-list order with
 * the clip rect a widget in the same place would have had -- so the face sits
 * inside the window, scrolls and layers with it, and is not something painted
 * behind microui's back afterwards.
 *
 * The clock reads the `time` syscall, which is the CMOS RTC. That is one
 * second of resolution, so a second hand driven by it alone would tick; this
 * one notes the uptime millisecond at which the second last changed (the
 * `clock` syscall) and interpolates between, which is what makes the sweep
 * smooth without needing a finer clock than the kernel has.
 *
 * Esc gives the screen back to the desktop.
 */
#include <math.h>
#include <printf.h>

#include "mui.h"

#define PI 3.14159265358979323846

/* ------------------------------------------------------------------ *
 *  Calendar                                                          *
 * ------------------------------------------------------------------ */

/** Broken-down time, the fields of @c struct tm this program needs. */
typedef struct {
    int year, mon, day;   /**< mon is 1..12, day 1..31. */
    int hour, min, sec;
    int wday;             /**< 0 = Sunday. */
} fields;

static const char *const g_wday[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
static const char *const g_month[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

/**
 * @brief Break a Unix timestamp into calendar fields.
 *
 * Howard Hinnant's civil-from-days, which counts from an era beginning on
 * 1 March 0000 so that the leap day lands at the end of a year and the
 * month-length arithmetic becomes a single multiply -- no lookup table and no
 * special case for February.
 *
 * No time zone is applied, deliberately: the `time` syscall is
 * rtc_now_unix(), which reads the CMOS RTC's fields and converts them as
 * though they were UTC, and this kernel has no notion of a zone to convert
 * them into. Whatever the RTC holds is what shows -- UTC on a machine that
 * reached an NTP server during boot (ntp.c sets the RTC to UTC), local time
 * on one whose firmware keeps local time and never synced.
 */
static void civil(unsigned t, fields *o) {
    unsigned days = t / 86400u, secs = t % 86400u;
    o->hour = (int) (secs / 3600u);
    o->min  = (int) ((secs / 60u) % 60u);
    o->sec  = (int) (secs % 60u);
    o->wday = (int) ((days + 4u) % 7u);        /* 1 Jan 1970 was a Thursday */

    long z = (long) days + 719468;             /* shift the epoch to 0000-03-01 */
    long era = (z >= 0 ? z : z - 146096) / 146097;
    long doe = z - era * 146097;               /* day of era,   0..146096 */
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long y   = yoe + era * 400;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp  = (5 * doy + 2) / 153;            /* month, March = 0 */
    o->day  = (int) (doy - (153 * mp + 2) / 5 + 1);
    o->mon  = (int) (mp < 10 ? mp + 3 : mp - 9);
    o->year = (int) (y + (o->mon <= 2));
}

/* ------------------------------------------------------------------ *
 *  State                                                             *
 * ------------------------------------------------------------------ */

/** Everything the frame callback and the face painter share. */
typedef struct {
    fields now;
    double subsec;      /**< 0..1 through the current second. */
    int sweep;          /**< Sweep the second hand instead of ticking it. */
    int numerals;       /**< Draw 12/3/6/9 on the dial. */
    int show_24h;       /**< 24-hour digital readout. */
    unsigned last_unix; /**< The second we last saw change. */
    unsigned last_ms;   /**< Uptime millisecond at which it changed. */
    int verbose;        /**< -v: echo every second to the console. */
    char digital[32];
    char date[48];
} clock_state;

/** @brief Re-read the clock and rebuild the readout strings. */
static void tick(clock_state *c) {
    unsigned t = mui_unix(), ms = mui_ms();
    unsigned before = c->last_unix;

    if (t != c->last_unix) {
        c->last_unix = t;
        c->last_ms = ms;
    }
    /* Clamped: an RTC that has not ticked for a while (a paused VM, say) must
     * not let the hand run past the second it is still in. */
    unsigned into = ms - c->last_ms;
    c->subsec = into >= 1000 ? 0.999 : into / 1000.0;

    civil(t, &c->now);

    int h = c->now.hour;
    if (c->show_24h) {
        sprintf(c->digital, "%02d:%02d:%02d", h, c->now.min, c->now.sec);
    } else {
        int h12 = h % 12;
        if (h12 == 0)
            h12 = 12;
        sprintf(c->digital, "%2d:%02d:%02d %s", h12, c->now.min, c->now.sec,
                h < 12 ? "am" : "pm");
    }
    sprintf(c->date, "%s, %d %s %d", g_wday[c->now.wday], c->now.day,
            g_month[c->now.mon - 1], c->now.year);

    /* The console is behind the grabbed screen, so this is for the serial log
     * -- which is how the port is checked without a display in front of it
     * (test/microui-boot.sh). Off unless asked for: one line a second would
     * otherwise bury everything else in that log. */
    if (c->verbose && t != before)
        printf("clock: %s  %s\n", c->digital, c->date);
}

/* ------------------------------------------------------------------ *
 *  The face                                                          *
 * ------------------------------------------------------------------ */

/*
 * The dial, in units of the face radius, so one set of numbers describes the
 * clock at whatever size the window gives it.
 */
#define BEZEL      1.00
#define TICK_OUT   0.94
#define MINOR_IN   0.89
#define MAJOR_IN   0.80
#define NUMERAL_R  0.70
#define HOUR_LEN   0.52
#define MINUTE_LEN 0.78
#define SECOND_LEN 0.86
#define TAIL_HM    0.09   /* counterweight on the hour and minute hands */
#define TAIL_SEC   0.22   /* the second hand's is longer, as on a real dial */

/** @brief Round to nearest, away from zero on a tie. */
static int nearest(double v) {
    return (int) (v < 0 ? v - 0.5 : v + 0.5);
}

/** @brief Point at @p frac of the way clockwise round the dial, radius @p rad. */
static void polar(int cx, int cy, double rad, double frac, int *x, int *y) {
    double a = frac * 2.0 * PI;            /* 0 at 12 o'clock, clockwise */
    *x = cx + nearest(rad * sin(a));
    *y = cy - nearest(rad * cos(a));       /* screen y grows downwards */
}

/** @brief Draw a hand from the centre out to @p len, with a short tail. */
static void hand(const mui_Surface *s, int cx, int cy, double r, double frac,
                 double len, double tail, int width, mu_Color col) {
    int x0, y0, x1, y1;
    polar(cx, cy, r * len, frac, &x1, &y1);
    polar(cx, cy, r * tail, frac + 0.5, &x0, &y0);
    mui_line(s, x0, y0, x1, y1, width, col);
}

/** @brief Paint the clock face into @p r. */
static void draw_face(const mui_Surface *s, mu_Rect r, void *udata) {
    const clock_state *c = (const clock_state *) udata;

    int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    int rad = (r.w < r.h ? r.w : r.h) / 2 - 2;
    if (rad < 24)
        return;                            /* too small to be a clock */
    double R = rad;

    const mu_Color dial   = mu_color(244, 243, 238, 255);
    const mu_Color bezel  = mu_color(58,  62,  72,  255);
    const mu_Color minor  = mu_color(150, 152, 158, 255);
    const mu_Color major  = mu_color(40,  42,  48,  255);
    const mu_Color ink    = mu_color(28,  30,  36,  255);
    const mu_Color accent = mu_color(206, 68,  58,  255);

    mui_disc(s, cx, cy, rad, dial);
    mui_ring(s, cx, cy, rad, (int) (R * (BEZEL - TICK_OUT)) + 2, bezel);

    /* 60 marks; every fifth one is the long, dark hour mark. */
    for (int i = 0; i < 60; i++) {
        int hour_mark = (i % 5) == 0;
        double frac = i / 60.0;
        int x0, y0, x1, y1;
        polar(cx, cy, R * (hour_mark ? MAJOR_IN : MINOR_IN), frac, &x0, &y0);
        polar(cx, cy, R * TICK_OUT, frac, &x1, &y1);
        mui_line(s, x0, y0, x1, y1, hour_mark ? 3 : 1,
                 hour_mark ? major : minor);
    }

    if (c->numerals) {
        static const struct { const char *s; int hour; } marks[4] = {
            {"12", 12}, {"3", 3}, {"6", 6}, {"9", 9}
        };
        for (int i = 0; i < 4; i++) {
            int x, y;
            polar(cx, cy, R * NUMERAL_R, marks[i].hour / 12.0, &x, &y);
            mui_string(s, x - mui_text_width(marks[i].s, -1) / 2,
                       y - mui_text_height() / 2, marks[i].s, ink);
        }
    }

    double sec_frac  = (c->now.sec + (c->sweep ? c->subsec : 0.0)) / 60.0;
    double min_frac  = (c->now.min + sec_frac) / 60.0;
    double hour_frac = ((c->now.hour % 12) + min_frac) / 12.0;

    hand(s, cx, cy, R, hour_frac, HOUR_LEN,   TAIL_HM,  6, ink);
    hand(s, cx, cy, R, min_frac,  MINUTE_LEN, TAIL_HM,  4, ink);
    hand(s, cx, cy, R, sec_frac,  SECOND_LEN, TAIL_SEC, 1, accent);

    /* Cap last, so it sits over the three hands' shared pivot. */
    mui_disc(s, cx, cy, 5, ink);
    mui_disc(s, cx, cy, 2, accent);
}

/* ------------------------------------------------------------------ *
 *  The window                                                        *
 * ------------------------------------------------------------------ */

/* The content size: the face, two readout lines, the options row and a hint
 * line, plus microui's padding and row spacing. The title bar is not in it --
 * whoever owns the window frame draws that (see ../microui/mui.h). */
#define WIN_W 332
#define WIN_H 360

static void frame(mu_Context *ctx, void *udata) {
    clock_state *c = (clock_state *) udata;

    tick(c);

    mu_layout_row(ctx, 1, (int[]) { -1 }, 248);
    mui_draw_custom(ctx, mu_layout_next(ctx), draw_face, c);

    mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
    mu_Rect r = mu_layout_next(ctx);
    mu_draw_control_text(ctx, c->digital, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);

    mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
    r = mu_layout_next(ctx);
    mu_draw_control_text(ctx, c->date, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);

    mu_layout_row(ctx, 3, (int[]) { 104, 104, -1 }, 0);
    mu_checkbox(ctx, "sweep", &c->sweep);
    mu_checkbox(ctx, "24h", &c->show_24h);
    mu_checkbox(ctx, "numerals", &c->numerals);

    mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
    mu_label(ctx, "Esc quits");
}

/**
 * @brief Entry point. `clock -v` echoes each second to the console;
 *        `clock -f` takes the whole screen instead of a window.
 */
int main(int argc, char **argv) {
    static clock_state c = { .sweep = 1, .numerals = 1 };

    for (int i = 1; i < argc; i++)
        if (argv[i] && argv[i][0] == '-' && argv[i][1] == 'v')
            c.verbose = 1;
        else if (argv[i] && argv[i][0] == '-' && argv[i][1] == 'f')
            mui_fullscreen(1);

    if (mui_run("Clock", WIN_W, WIN_H, frame, &c) < 0) {
        printf("clock: no window and no screen -- is there a framebuffer, and "
               "is another program holding it?\n");
        return 1;
    }
    return 0;
}
