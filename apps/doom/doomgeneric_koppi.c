/*
 * doomgeneric platform layer for koppi-os.
 *
 * The six DG_* entry points the engine expects, over four kernel facilities:
 *
 *   display  gfx_open/gfx_palette/gfx_blit/gfx_close (syscalls 22-25). The
 *            engine already renders into an 8-bpp indexed buffer, which is
 *            exactly what the blit syscall takes, so a frame reaches the
 *            screen as one 64 KiB copy plus a palette lookup in the kernel.
 *   keyboard getscan (#26): raw scancodes with make/break, translated here
 *            into Doom key codes. Polled once per frame, never blocking.
 *   clock    clock (#15), the free-running PIT millisecond counter.
 *   sleep    msleep (#27).
 *
 * The grab has to be undone however the game ends -- the quit menu, a fatal
 * I_Error, or a plain return from main -- so the teardown is registered both
 * with the engine's own exit list and with the shim's atexit().
 */
#include <stdlib.h>

#include "doomkeys.h"
#include "doomgeneric.h"
#include "i_system.h"
#include "i_video.h"

#include "ksys.h"

/* ------------------------------------------------------------------ *
 *  Keyboard                                                           *
 * ------------------------------------------------------------------ */

/*
 * Scancode set 1 make code -> Doom key. Index is the 7-bit make code; the
 * 0xE0-prefixed keys are handled separately below because their codes
 * collide with the numeric keypad's.
 *
 * KEY_FIRE / KEY_USE rather than KEY_RCTRL / ' ': those are the values
 * m_controls.c binds fire and use to by default, and doomgeneric's own
 * tables use them the same way.
 */
static const unsigned char sc_to_doom[128] = {
    [0x01] = KEY_ESCAPE,
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = KEY_MINUS, [0x0D] = KEY_EQUALS, [0x0E] = KEY_BACKSPACE,
    [0x0F] = KEY_TAB,
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']', [0x1C] = KEY_ENTER,
    [0x1D] = KEY_FIRE,                       /* left ctrl  */
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',
    [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2A] = KEY_RSHIFT,                     /* left shift */
    [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x36] = KEY_RSHIFT,                     /* right shift */
    [0x37] = KEYP_MULTIPLY,
    [0x38] = KEY_RALT,                       /* left alt: strafe modifier */
    [0x39] = KEY_USE,                        /* space */
    [0x3A] = KEY_CAPSLOCK,
    [0x3B] = KEY_F1, [0x3C] = KEY_F2, [0x3D] = KEY_F3, [0x3E] = KEY_F4,
    [0x3F] = KEY_F5, [0x40] = KEY_F6, [0x41] = KEY_F7, [0x42] = KEY_F8,
    [0x43] = KEY_F9, [0x44] = KEY_F10,
    [0x45] = KEY_NUMLOCK, [0x46] = KEY_SCRLCK,
    [0x47] = KEYP_7, [0x48] = KEYP_8, [0x49] = KEYP_9, [0x4A] = KEYP_MINUS,
    [0x4B] = KEYP_4, [0x4C] = KEYP_5, [0x4D] = KEYP_6, [0x4E] = KEYP_PLUS,
    [0x4F] = KEYP_1, [0x50] = KEYP_2, [0x51] = KEYP_3, [0x52] = KEYP_0,
    [0x53] = KEYP_PERIOD,
    [0x57] = KEY_F11, [0x58] = KEY_F12,
};

/** @brief Doom key for one 0xE0-prefixed make code (the grey keys). */
static unsigned char e0_to_doom(unsigned char sc) {
    switch (sc) {
    case 0x1C: return KEY_ENTER;        /* keypad enter */
    case 0x1D: return KEY_FIRE;         /* right ctrl   */
    case 0x35: return KEYP_DIVIDE;
    case 0x38: return KEY_RALT;
    case 0x47: return KEY_HOME;
    case 0x48: return KEY_UPARROW;
    case 0x49: return KEY_PGUP;
    case 0x4B: return KEY_LEFTARROW;
    case 0x4D: return KEY_RIGHTARROW;
    case 0x4F: return KEY_END;
    case 0x50: return KEY_DOWNARROW;
    case 0x51: return KEY_PGDN;
    case 0x52: return KEY_INS;
    case 0x53: return KEY_DEL;
    default:   return 0;
    }
}

/*
 * A frame's worth of key events. The engine drains this through DG_GetKey
 * once per tick; the kernel ring behind getscan is only 64 entries, so it is
 * emptied every frame rather than left to overflow while a level loads.
 */
#define KEYQUEUE_SIZE 32

static unsigned short key_queue[KEYQUEUE_SIZE];
static unsigned int   key_wr, key_rd;

static void queue_key(int pressed, unsigned char key) {
    unsigned int next = (key_wr + 1) % KEYQUEUE_SIZE;
    if (next == key_rd)
        return;                          /* full: drop the newest */
    key_queue[key_wr] = (unsigned short) ((pressed << 8) | key);
    key_wr = next;
}

static void poll_keys(void) {
    for (;;) {
        unsigned long ev = ksys0(SYS_GETSCAN);
        if (!(ev & KSCAN_VALID))
            return;

        unsigned char sc = (unsigned char) (ev & 0x7F);
        int release = (ev & KSCAN_BREAK) != 0;
        unsigned char key = (ev & KSCAN_E0) ? e0_to_doom(sc) : sc_to_doom[sc];
        if (key)
            queue_key(!release, key);
    }
}

int DG_GetKey(int *pressed, unsigned char *doomKey) {
    if (key_rd == key_wr)
        return 0;
    unsigned short d = key_queue[key_rd];
    key_rd = (key_rd + 1) % KEYQUEUE_SIZE;
    *pressed = d >> 8;
    *doomKey = (unsigned char) (d & 0xFF);
    return 1;
}

/* ------------------------------------------------------------------ *
 *  Display                                                            *
 * ------------------------------------------------------------------ */

static int screen_held;

static void release_screen(void) {
    if (!screen_held)
        return;
    screen_held = 0;
    ksys0(SYS_GFX_CLOSE);
}

/*
 * The grab is deferred to the first frame rather than taken in DG_Init.
 * Everything between the two -- finding the IWAD, loading it, building the
 * textures -- prints its progress, and a grabbed screen shows none of it:
 * the desktop is parked and nothing but blitted frames reaches the display.
 * Waiting means a startup that fails (no IWAD, most often) reports itself on
 * a console the player can still read, and never blanks the desktop at all.
 */
static void take_screen(void) {
    if (screen_held)
        return;
    if (!ksys2(SYS_GFX_OPEN, DOOMGENERIC_RESX, DOOMGENERIC_RESY))
        I_Error("doom: cannot take the screen (no framebuffer, or another "
                "program is holding it)");
    screen_held = 1;
}

void DG_Init(void) {
    /* Two ways out of the game, so two places to hook: I_Error and the quit
     * menu unwind through the engine's list, everything else through exit(). */
    I_AtExit(release_screen, true);
    atexit(release_screen);
}

void DG_DrawFrame(void) {
    take_screen();

    if (palette_changed) {
        /* struct color is b,g,r,a bitfields; the syscall wants 0x00RRGGBB. */
        uint32_t pal[256];
        for (int i = 0; i < 256; i++)
            pal[i] = ((uint32_t) colors[i].r << 16) |
                     ((uint32_t) colors[i].g << 8) |
                      (uint32_t) colors[i].b;
        ksys1(SYS_GFX_PALETTE, (unsigned long) pal);
        palette_changed = false;
    }

    ksys1(SYS_GFX_BLIT, (unsigned long) DG_ScreenBuffer);
    poll_keys();
}

void DG_SetWindowTitle(const char *title) { (void) title; }

/* ------------------------------------------------------------------ *
 *  Time                                                               *
 * ------------------------------------------------------------------ */

uint32_t DG_GetTicksMs(void) { return (uint32_t) ksys0(SYS_CLOCK); }

void DG_SleepMs(uint32_t ms) { ksys1(SYS_MSLEEP, ms); }

/* ------------------------------------------------------------------ *
 *  Entry point                                                        *
 * ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    doomgeneric_Create(argc, argv);

    for (;;)
        doomgeneric_Tick();

    /* D_DoomMain never returns; the game leaves through exit(). */
}
