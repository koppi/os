/**
 * @file wm.c
 * @brief The window manager: the table of ring-3 program windows, their
 *        frames, and the input routed to them. See wm.h for the model.
 */
#include <wm.h>

#include <kheap.h>
#include <lib/string.h>
#include <mm.h>
#include <spinlock.h>
#include <video.h>
#include <printf.h>
#include <log.h>

static wm_window_t g_win[WM_MAX_WINDOWS];

/**
 * Per-window event queue. 64 entries is about a second of a user typing fast
 * plus the pointer samples the desktop takes between two of the program's
 * frames; a program that stops draining it has stopped drawing too, and the
 * oldest events are the ones worth dropping.
 */
#define WM_EVQ_SIZE 64
#define WM_EVQ_MASK (WM_EVQ_SIZE - 1)

static uint32_t g_evq[WM_MAX_WINDOWS][WM_EVQ_SIZE];
static uint32_t g_evq_head[WM_MAX_WINDOWS], g_evq_tail[WM_MAX_WINDOWS];

/** The window with the keyboard, or -1 for none (the shell keeps it then). */
static int g_focus = -1;

/** Last pointer state delivered, so an idle pointer queues nothing. */
static int g_ptr_x = -1, g_ptr_y = -1;
static uint32_t g_ptr_buttons;

/**
 * One lock for the table, taken by the desktop thread, by syscalls and by the
 * keyboard IRQ. spin_lock() saves and clears IF, so the IRQ cannot deadlock
 * against a holder on the same CPU.
 */
static spinlock_t wm_lock;

/* ------------------------------------------------------------------ *
 *  Slots                                                             *
 * ------------------------------------------------------------------ */

/** @brief Find @p pid's live window. Call with the lock held. @return -1 if none. */
static int slot_of(int pid) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        if (g_win[i].used && !g_win[i].zombie && g_win[i].pid == pid)
            return i;
    return -1;
}

/** @brief Queue one event for slot @p i. Call with the lock held. */
static void push_event(int i, uint32_t ev) {
    uint32_t next = (g_evq_head[i] + 1) & WM_EVQ_MASK;
    if (next == g_evq_tail[i])                /* full: drop the oldest */
        g_evq_tail[i] = (g_evq_tail[i] + 1) & WM_EVQ_MASK;
    g_evq[i][g_evq_head[i]] = ev;
    g_evq_head[i] = next;
}

/* ------------------------------------------------------------------ *
 *  The ring-3 side                                                   *
 * ------------------------------------------------------------------ */

int wm_open(int pid, uint32_t w, uint32_t h, const char *title) {
    /* No framebuffer means no desktop to put a window on; the caller falls
     * back to the full-screen grab, which fails the same way. */
    if (!video_has_shadow() || !vbemem.xres)
        return 0;
    if (w == 0 || h == 0 || w > WM_MAX_W || h > WM_MAX_H)
        return 0;
    /* The window has to fit the screen with room for its own chrome, or the
     * user could never reach its title bar to move it. */
    if (w + 16 > vbemem.xres || h + 48 > vbemem.yres)
        return 0;

    uint8_t *pix = (uint8_t *) kmalloc(w * h);
    if (!pix)
        return 0;
    memset(pix, 0, w * h);

    uint32_t f = spin_lock(&wm_lock);

    int i = -1;
    for (int k = 0; k < WM_MAX_WINDOWS; k++)
        if (!g_win[k].used) { i = k; break; }
    if (i < 0 || slot_of(pid) >= 0) {       /* no slot, or already has one */
        spin_unlock(&wm_lock, f);
        kfree(pix);
        return 0;
    }

    wm_window_t *win = &g_win[i];
    memset(win, 0, sizeof *win);
    win->used = 1;
    win->pid  = pid;
    win->w    = (int) w;
    win->h    = (int) h;
    win->pix  = pix;

    /* A palette of greys until the program installs its own, so a window that
     * is mapped before its first blit is a grey rectangle rather than noise. */
    for (int k = 0; k < 256; k++)
        win->pal[k] = (uint32_t) k * 0x010101u;

    if (title && (uint32_t) (uintptr_t) title >= KERNEL_SPACE_END) {
        uint32_t n = 0;
        while (title[n] && n < WM_TITLE_MAX - 1) {
            win->title[n] = title[n];
            n++;
        }
        win->title[n] = 0;
    }
    if (!win->title[0])
        strcpy(win->title, "program");

    /* Cascade, so a second window does not land exactly on the first. */
    win->init_x = 60 + i * 28;
    win->init_y = 60 + i * 28;

    g_evq_head[i] = g_evq_tail[i] = 0;
    g_focus = i;                            /* a new window takes the keyboard */

    spin_unlock(&wm_lock, f);
    klogf(LOG_INFO, "wm: window %d '%s' %ux%u for pid %d\n", i, win->title, w, h, pid);
    return 1;
}

/**
 * @brief Give up a window.
 *
 * The slot is marked a zombie rather than freed here: the desktop's draw
 * thread reads @c pix without the lock while it composites, so the one place
 * it is safe to release the memory is in that thread, between frames
 * (@ref wm_frame_begin).
 */
static void close_slot(int i) {
    g_win[i].zombie = 1;
    if (g_focus == i)
        g_focus = -1;
}

void wm_close(int pid) {
    uint32_t f = spin_lock(&wm_lock);
    int i = slot_of(pid);
    if (i >= 0)
        close_slot(i);
    spin_unlock(&wm_lock, f);
}

void wm_reap(int pid) {
    wm_close(pid);
}

int wm_blit(int pid, const uint8_t *pix) {
    if (!pix || (uint32_t) (uintptr_t) pix < KERNEL_SPACE_END)
        return -1;

    uint32_t f = spin_lock(&wm_lock);
    int i = slot_of(pid);
    if (i < 0) {
        spin_unlock(&wm_lock, f);
        return -1;
    }
    uint8_t *dst = g_win[i].pix;
    uint32_t n = (uint32_t) g_win[i].w * (uint32_t) g_win[i].h;
    spin_unlock(&wm_lock, f);

    /* Outside the lock: a quarter-megabyte memcpy with interrupts off would
     * cost more than the torn frame it would prevent (see wm.h). */
    memcpy(dst, pix, n);
    g_win[i].painted = 1;
    return 0;
}

int wm_palette(int pid, const uint32_t *pal) {
    if (!pal || (uint32_t) (uintptr_t) pal < KERNEL_SPACE_END)
        return -1;

    uint32_t f = spin_lock(&wm_lock);
    int i = slot_of(pid);
    if (i < 0) {
        spin_unlock(&wm_lock, f);
        return -1;
    }
    for (int k = 0; k < 256; k++)
        g_win[i].pal[k] = pal[k] & 0xFFFFFF;
    spin_unlock(&wm_lock, f);
    return 0;
}

uint32_t wm_event(int pid) {
    uint32_t f = spin_lock(&wm_lock);
    int i = slot_of(pid);
    uint32_t ev = 0;
    if (i >= 0 && g_evq_head[i] != g_evq_tail[i]) {
        ev = g_evq[i][g_evq_tail[i]];
        g_evq_tail[i] = (g_evq_tail[i] + 1) & WM_EVQ_MASK;
    }
    spin_unlock(&wm_lock, f);
    return ev;
}

/* ------------------------------------------------------------------ *
 *  The desktop side                                                  *
 * ------------------------------------------------------------------ */

int wm_active(void) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        if (g_win[i].used && !g_win[i].zombie)
            return 1;
    return 0;
}

void wm_frame_begin(void) {
    uint32_t f = spin_lock(&wm_lock);
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_win[i].used && g_win[i].zombie) {
            uint8_t *pix = g_win[i].pix;
            memset(&g_win[i], 0, sizeof g_win[i]);
            spin_unlock(&wm_lock, f);
            kfree(pix);                      /* kfree may take its own lock */
            f = spin_lock(&wm_lock);
        } else if (g_win[i].used) {
            g_win[i].mapped = 0;
        }
    }
    spin_unlock(&wm_lock, f);
}

wm_window_t *wm_slot(int i) {
    if (i < 0 || i >= WM_MAX_WINDOWS)
        return 0;
    return (g_win[i].used && !g_win[i].zombie) ? &g_win[i] : 0;
}

void wm_viewport(int i, int x, int y) {
    if (i < 0 || i >= WM_MAX_WINDOWS)
        return;
    g_win[i].vx = x;
    g_win[i].vy = y;
    g_win[i].mapped = 1;
}

void wm_set_focus(int i) {
    uint32_t f = spin_lock(&wm_lock);
    if (i >= 0 && i < WM_MAX_WINDOWS && (!g_win[i].used || g_win[i].zombie))
        i = -1;
    if (g_focus != i) {
        g_focus = i;
        /* The pointer state belongs to the window that had it; make the next
         * sample unconditional so the new one learns where the pointer is. */
        g_ptr_x = g_ptr_y = -1;
    }
    spin_unlock(&wm_lock, f);
}

int wm_focus(void) {
    return g_focus;
}

void wm_request_close(int i) {
    uint32_t f = spin_lock(&wm_lock);
    if (i >= 0 && i < WM_MAX_WINDOWS && g_win[i].used && !g_win[i].zombie)
        push_event(i, WM_EV_VALID | (WM_EV_WINDOW << 28) | WM_WIN_CLOSE);
    spin_unlock(&wm_lock, f);
}

/**
 * @brief Hand the desktop pointer to the focused window in its own coordinates.
 *
 * Sampled once a frame by the desktop rather than pushed from the mouse IRQ:
 * a window's position is a thing only the desktop knows, and it only knows it
 * while it is drawing. Nothing is queued unless the position or the buttons
 * changed, so a still pointer costs the program nothing.
 *
 * Positions are clamped into the window rather than dropped when the pointer
 * leaves it, so a drag that runs off the edge still tracks -- and a release
 * outside still arrives, which is what stops a widget being left stuck down.
 */
void wm_pointer(int gx, int gy, uint32_t buttons) {
    uint32_t f = spin_lock(&wm_lock);
    int i = g_focus;
    if (i < 0 || !g_win[i].used || g_win[i].zombie || !g_win[i].mapped) {
        spin_unlock(&wm_lock, f);
        return;
    }

    int lx = gx - g_win[i].vx, ly = gy - g_win[i].vy;
    lx = lx < 0 ? 0 : (lx >= g_win[i].w ? g_win[i].w - 1 : lx);
    ly = ly < 0 ? 0 : (ly >= g_win[i].h ? g_win[i].h - 1 : ly);

    if (lx != g_ptr_x || ly != g_ptr_y || buttons != g_ptr_buttons) {
        g_ptr_x = lx;
        g_ptr_y = ly;
        g_ptr_buttons = buttons;
        push_event(i, WM_EV_VALID | (WM_EV_POINTER << 28) |
                      ((buttons & 7u) << 24) |
                      (((uint32_t) ly & 0xFFFu) << 12) | ((uint32_t) lx & 0xFFFu));
    }
    spin_unlock(&wm_lock, f);
}

/* ------------------------------------------------------------------ *
 *  The keyboard side                                                 *
 * ------------------------------------------------------------------ */

int wm_wants_keys(void) {
    /* A full-screen program has taken the display and the desktop is parked,
     * so whatever window had focus is not on screen and must not be taking
     * keystrokes away from the program in front of it. The grab owns the
     * machine for as long as it lasts (video.h). */
    if(video_grabbed())
        return 0;
    int i = g_focus;
    return i >= 0 && g_win[i].used && !g_win[i].zombie;
}

void wm_key(uint16_t raw) {
    uint32_t f = spin_lock(&wm_lock);
    int i = g_focus;
    if (i >= 0 && g_win[i].used && !g_win[i].zombie)
        push_event(i, WM_EV_VALID | (WM_EV_KEY << 28) | raw);
    spin_unlock(&wm_lock, f);
}
