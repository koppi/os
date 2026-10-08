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
 *   gamepad  getpad (#45): the first USB game controller's buttons and sticks,
 *            turned into the same Doom key events (see "Game controller").
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
#include "doomstat.h"
#include "i_system.h"
#include "i_video.h"
#include "m_controls.h"

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

/* ------------------------------------------------------------------ *
 *  Game controller                                                    *
 * ------------------------------------------------------------------ */

/*
 * A USB pad reaches the game as key events, the same way the keyboard does:
 * every frame the first connected pad's state is turned into the set of Doom
 * keys it stands for, and what changed since last frame is queued as presses
 * and releases. The engine never knows there was a pad. That is also all Doom's
 * own joystick support amounts to (it only ever reads the sign of an axis), so
 * nothing is lost by it, and the menus, the Y/N prompts and the weapon keys
 * come for free.
 *
 * Buttons are numbered the way the pad's own report descriptor numbers them --
 * the kernel keeps no table of which pad has which -- so these masks are the
 * one thing to edit for a pad that disagrees. The console's `pad` command
 * prints the number of every button as it is pressed. The defaults follow the
 * numbering that a DualShock 4, a Logitech F310 and most generic pads share:
 *
 *    1..4   the four face buttons         7, 8    left / right trigger
 *    5, 6   left / right shoulder         9, 10   Select / Start
 *                                         11, 12  stick clicks
 *
 * (An Xbox 360 pad is renumbered by the kernel to match.) Which face button is
 * which varies, so fire and use are each on two of them: whatever the layout,
 * both are somewhere under a thumb.
 */
#define B(n) (1u << ((n) - 1))

#define PADB_FIRE   (B(1) | B(3) | B(8))    /* face buttons, right trigger */
#define PADB_USE    (B(2) | B(4))           /* the other two face buttons */
#define PADB_RUN    (B(7) | B(11))          /* left trigger, left stick click */
#define PADB_PREV   B(5)                    /* previous weapon: left shoulder */
#define PADB_NEXT   B(6)                    /* next weapon: right shoulder */
#define PADB_MAP    B(9)                    /* Select: automap */
#define PADB_START  B(10)                   /* Start: the menu */
#define PADB_OK     (B(1) | B(3))           /* in a menu: Enter / Yes */
#define PADB_BACK   (B(2) | B(4))           /* in a menu: Backspace / No */

#define PAD_STICK_ON   16000   /* a stick counts as pushed past this ... */
#define PAD_STICK_OFF  10000   /* ... until it falls back below this */
#define PAD_TRIG_ON    110
#define PAD_TRIG_OFF   70

/* Doom has no next/previous-weapon key until the controls are given one, and
 * these are codes no keyboard produces, so the keyboard is not affected. */
#define PAD_KEY_PREVWEAPON 0x01
#define PAD_KEY_NEXTWEAPON 0x02

/* Everything a pad can hold down, each tied to one Doom key. */
enum {
    PK_UP, PK_DOWN, PK_LEFT, PK_RIGHT, PK_STRAFE_L, PK_STRAFE_R,
    PK_FIRE, PK_USE, PK_RUN, PK_PREV, PK_NEXT, PK_MAP, PK_ESC,
    PK_ENTER, PK_BACKSPACE, PK_YES, PK_NO,
    PK_COUNT
};

static const unsigned char pk_doom[PK_COUNT] = {
    [PK_UP] = KEY_UPARROW,       [PK_DOWN] = KEY_DOWNARROW,
    [PK_LEFT] = KEY_LEFTARROW,   [PK_RIGHT] = KEY_RIGHTARROW,
    [PK_STRAFE_L] = KEY_STRAFE_L, [PK_STRAFE_R] = KEY_STRAFE_R,
    [PK_FIRE] = KEY_FIRE,        [PK_USE] = KEY_USE,
    [PK_RUN] = KEY_RSHIFT,
    [PK_PREV] = PAD_KEY_PREVWEAPON, [PK_NEXT] = PAD_KEY_NEXTWEAPON,
    [PK_MAP] = KEY_TAB,          [PK_ESC] = KEY_ESCAPE,
    [PK_ENTER] = KEY_ENTER,      [PK_BACKSPACE] = KEY_BACKSPACE,
    [PK_YES] = 'y',              [PK_NO] = 'n',
};

/* Defined in m_menu.c without a header: set while a Y/N prompt is up. */
extern int messageToPrint;

/* Which half of each stick / trigger is currently held, so a stick sitting on
 * the threshold does not chatter (see axis_held). */
enum { AX_LX_NEG, AX_LX_POS, AX_LY_NEG, AX_LY_POS, AX_RX_NEG, AX_RX_POS,
       AX_LT, AX_RT, AX_COUNT };
static unsigned char ax_on[AX_COUNT];

static unsigned char pk_held[PK_COUNT];
static unsigned int  pad_latch;       /* buttons to ignore until let go */
static int           pad_ctx = -1;    /* menu / prompt / play, last frame */

/** @brief Is the axis value @p mag (already signed towards the direction asked)
 *         past @p on, staying held until it falls below @p off? */
static int axis_held(int which, int mag, int on, int off) {
    if (ax_on[which]) {
        if (mag < off)
            ax_on[which] = 0;
    } else if (mag > on) {
        ax_on[which] = 1;
    }
    return ax_on[which];
}

/** @brief Work out which keys the pad @p p stands for this frame. */
static void pad_wants(const struct kpad *p, unsigned char want[PK_COUNT]) {
    int menu   = menuactive != 0;
    int prompt = menu && messageToPrint;
    int ctx    = prompt ? 2 : menu ? 1 : 0;

    /* A button that was held when the screen changed (the one that dismissed a
     * menu, say) means nothing in the new one until it has been let go, or
     * "Enter" on a menu item would go on to fire the first shot. */
    if (ctx != pad_ctx) {
        pad_latch = p->buttons;
        pad_ctx = ctx;
    }
    pad_latch &= p->buttons;
    unsigned int b = p->buttons & ~pad_latch;

    int lx_n = axis_held(AX_LX_NEG, -p->lx, PAD_STICK_ON, PAD_STICK_OFF);
    int lx_p = axis_held(AX_LX_POS,  p->lx, PAD_STICK_ON, PAD_STICK_OFF);
    int ly_n = axis_held(AX_LY_NEG, -p->ly, PAD_STICK_ON, PAD_STICK_OFF);
    int ly_p = axis_held(AX_LY_POS,  p->ly, PAD_STICK_ON, PAD_STICK_OFF);
    int rx_n = axis_held(AX_RX_NEG, -p->rx, PAD_STICK_ON, PAD_STICK_OFF);
    int rx_p = axis_held(AX_RX_POS,  p->rx, PAD_STICK_ON, PAD_STICK_OFF);
    int lt   = axis_held(AX_LT, p->lt, PAD_TRIG_ON, PAD_TRIG_OFF);
    int rt   = axis_held(AX_RT, p->rt, PAD_TRIG_ON, PAD_TRIG_OFF);

    int up    = (p->dpad & KPAD_DPAD_UP)    || ly_n;
    int down  = (p->dpad & KPAD_DPAD_DOWN)  || ly_p;
    int left  = (p->dpad & KPAD_DPAD_LEFT);
    int right = (p->dpad & KPAD_DPAD_RIGHT);

    if (menu) {
        /* Menus are driven by cursor keys, so every direction -- either stick,
         * the D-pad -- is one. */
        want[PK_UP]    = up;
        want[PK_DOWN]  = down;
        want[PK_LEFT]  = left  || lx_n || rx_n;
        want[PK_RIGHT] = right || lx_p || rx_p;
        if (prompt) {
            want[PK_YES] = (b & PADB_OK) != 0;
            want[PK_NO]  = (b & PADB_BACK) != 0;
        } else {
            want[PK_ENTER]     = (b & PADB_OK) != 0;
            want[PK_BACKSPACE] = (b & PADB_BACK) != 0;
        }
    } else {
        /* Left stick walks and strafes, right stick (and the D-pad's sides)
         * turns -- the usual shooter split. */
        want[PK_UP]       = up;
        want[PK_DOWN]     = down;
        want[PK_LEFT]     = left  || rx_n;
        want[PK_RIGHT]    = right || rx_p;
        want[PK_STRAFE_L] = lx_n;
        want[PK_STRAFE_R] = lx_p;
        want[PK_FIRE]     = (b & PADB_FIRE) != 0 || rt;
        want[PK_USE]      = (b & PADB_USE) != 0;
        want[PK_RUN]      = (b & PADB_RUN) != 0 || lt;
        want[PK_PREV]     = (b & PADB_PREV) != 0;
        want[PK_NEXT]     = (b & PADB_NEXT) != 0;
        want[PK_MAP]      = (b & PADB_MAP) != 0;
    }
    want[PK_ESC] = (b & PADB_START) != 0;
}

/**
 * @brief Turn this frame's pad state into key events.
 *
 * With no pad connected nothing is wanted, so a pad pulled out mid-game lets go
 * of everything it was holding rather than leaving the player running.
 */
static void poll_pad(void) {
    struct kpad p;
    unsigned char want[PK_COUNT] = { 0 };
    int have = 0;

    for (int i = 0; i < KPAD_MAX && !have; i++)
        have = ksys2(SYS_GETPAD, (unsigned long) i, (unsigned long) &p) == 1;

    if (have) {
        pad_wants(&p, want);
    } else {
        pad_ctx = -1;
        for (int i = 0; i < AX_COUNT; i++)
            ax_on[i] = 0;
    }

    for (int k = 0; k < PK_COUNT; k++) {
        if (want[k] != pk_held[k]) {
            queue_key(want[k], pk_doom[k]);
            pk_held[k] = want[k];
        }
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
    key_prevweapon = PAD_KEY_PREVWEAPON;
    key_nextweapon = PAD_KEY_NEXTWEAPON;

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
    poll_pad();
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
