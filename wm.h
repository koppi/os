/**
 * @file wm.h
 * @brief The window manager: ring-3 programs as windows on the desktop.
 *
 * Before this, a userspace program that wanted to draw took the whole screen
 * (@ref video_grab) and the desktop was parked until it exited -- fine for
 * Doom, wrong for a calculator. A window here is the other arrangement: the
 * program owns an 8-bpp surface and a palette of its own, hands over a frame
 * whenever it has one, and the desktop composites that surface into a microui
 * window like any other piece of the UI. Both arrangements stay: a program
 * asks for a window first and falls back to the exclusive grab when there is
 * no desktop to put one on.
 *
 * The split is that the **window manager owns the frame and the program owns
 * the content**. microui already does the hard parts of a window manager --
 * dragging, z-order, focus, the title bar and its close box -- so the desktop
 * draws one `mu_begin_window` per program and blits the program's surface into
 * its body (@ref r_draw_surface). The program draws no chrome and does not
 * know where on screen it is.
 *
 * Input is routed, not shared. Only the focused window's program receives
 * keystrokes, and it receives them as raw scancodes with make *and* break, the
 * same encoding `getscan` uses; the pointer arrives as a position in the
 * window's own coordinates, which is what a program that does not know where
 * it is needs. Everything reaches ring 3 through one 32-bit word per event
 * (@ref wm_event), so there is no struct ABI across the syscall boundary.
 *
 * Threading: the table is touched by the desktop's draw thread, by syscalls on
 * whatever CPU the program runs on, and by the keyboard IRQ, so it is behind
 * one spinlock. Frame data is the exception -- a blit copies into the window's
 * buffer without the lock, and the desktop may read it mid-copy. The cost of
 * that is a torn frame now and then; the cost of the alternative is a 250 KiB
 * memcpy with interrupts off.
 */
#pragma once

#include <types.h>

/**
 * How many program windows can exist at once, and how big one may be.
 *
 * Each window's frame buffer is w*h bytes of the 4 MiB kernel heap
 * (@ref KHEAP_SIZE), so the ceiling here is a memory budget: four windows at
 * the maximum size is 1 MiB, a quarter of the heap, which is as much as this
 * is worth. A program asking for more than the maximum is refused and falls
 * back to the full-screen grab.
 */
#define WM_MAX_WINDOWS 4
#define WM_MAX_W       800
#define WM_MAX_H       600
#define WM_TITLE_MAX   32

/** One ring-3 program's window. */
typedef struct {
    int      used;                /**< Slot is live. */
    int      zombie;              /**< Closed; the desktop frees it next frame. */
    int      pid;                 /**< Owning process (thread id of its main thread). */
    int      w, h;                /**< Surface size, fixed at open. */
    uint8_t *pix;                 /**< w*h palette indices. */
    uint32_t pal[256];            /**< 0x00RRGGBB per index. */
    char     title[WM_TITLE_MAX]; /**< What the title bar says. */
    int      init_x, init_y;      /**< Where it is first placed (microui then owns it). */
    int      vx, vy;              /**< Where its content landed this frame. */
    int      mapped;              /**< The desktop drew it this frame. */
    int      painted;             /**< A frame has arrived at least once. */
} wm_window_t;

/** @name Called from the syscall gate (see syscall.c) */
///@{
/** @brief Open a @p w x @p h window for @p pid. @return 1, or 0 if refused. */
int      wm_open(int pid, uint32_t w, uint32_t h, const char *title);
/** @brief Close @p pid's window, if it has one. */
void     wm_close(int pid);
/** @brief Copy one frame of @p pid's surface in. @return 0, or -1. */
int      wm_blit(int pid, const uint8_t *pix);
/** @brief Install @p pid's 256-entry palette. @return 0, or -1. */
int      wm_palette(int pid, const uint32_t *pal);
/** @brief Pop one event for @p pid, or 0 when the queue is empty. */
uint32_t wm_event(int pid);
///@}

/**
 * @name Event words (@ref wm_event)
 *
 * One 32-bit word per event: bit 31 marks it valid, bits 28..30 say what it
 * is, and the rest is that kind's payload.
 *
 *   key      bits 0..8   scancode, | @c WM_KEY_BREAK on release,
 *                        | @c WM_KEY_E0 for the grey keys -- the same
 *                        encoding `getscan` (#26) uses, so a program can
 *                        decode both with one table
 *   pointer  bits 0..11  x, 12..23 y, both in the window's own coordinates
 *                        and clamped to it; bits 24..26 the button mask
 *   window   bits 0..7   @c WM_WIN_CLOSE when the title bar's box was clicked
 */
///@{
#define WM_EV_VALID   0x80000000u
#define WM_EV_KIND(e) (((e) >> 28) & 7u)
#define WM_EV_KEY     0u
#define WM_EV_POINTER 1u
#define WM_EV_WINDOW  2u

#define WM_KEY_BREAK  0x0080u
#define WM_KEY_E0     0x0100u

#define WM_WIN_CLOSE  1u
///@}

/** @name Called from the desktop's draw thread (see graphics.c) */
///@{
/** @return Non-zero if any program window is live (the desktop must draw). */
int          wm_active(void);
/** @brief Free windows closed since the last frame. Call once, before painting. */
void         wm_frame_begin(void);
/** @return Slot @p i, or NULL when it holds no live window. */
wm_window_t *wm_slot(int i);
/** @brief Note where slot @p i's content was drawn this frame. */
void         wm_viewport(int i, int x, int y);
/** @brief Give slot @p i the keyboard (-1: nobody -- the shell keeps it). */
void         wm_set_focus(int i);
/** @return The focused slot, or -1. */
int          wm_focus(void);
/** @brief Tell slot @p i's program that the user asked it to close. */
void         wm_request_close(int i);
/** @brief Feed the desktop pointer to the focused window, in its coordinates. */
void         wm_pointer(int gx, int gy, uint32_t buttons);
///@}

/** @name Called from the keyboard IRQ (see keyboard.c) */
///@{
/** @return Non-zero while a program window holds the keyboard. */
int  wm_wants_keys(void);
/** @brief Queue one raw scancode event for the focused window. */
void wm_key(uint16_t raw);
///@}

/** @brief Release whatever window @p pid held. Called when a process is reaped. */
void wm_reap(int pid);
