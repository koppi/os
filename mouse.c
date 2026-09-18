/**
 * @file mouse.c
 * @brief PS/2 pointer driver.
 *
 * Two protocols:
 *   - plain PS/2: the standard 3-byte relative packet (a USB-attached mouse
 *     behind the i8042, or a non-Synaptics touchpad).
 *   - Synaptics (every ThinkPad ClickPad): the 6-byte absolute packet, plus
 *     the *pass-through* sub-packet (W == 3) that carries the TrackPoint's
 *     own 3-byte PS/2 packet. Without decoding this the red TrackPoint does
 *     nothing -- it is wired to the touchpad's guest port, not the i8042.
 *
 * Absolute finger positions are turned into relative cursor motion here.
 */
#include <mouse.h>
#include <io.h>
#include <idt.h>
#include <video.h>
#include <cmdline.h>
#include <log.h>

mouse_info_t mouse_info;

uint8_t mouse_cycle = 0;
char mouse_byte[3];

extern void mouse_int();

/* i8042 status bits: 0x01 = output buffer full, 0x02 = input buffer full. */
static int i8042_wait_write(void) {
    for (int i = 0; i < 100000; i++)
        if (!(inportb(MOUSE_STATUS) & 0x02))
            return 1;
    return 0;
}
static int i8042_wait_read(void) {
    for (int i = 0; i < 100000; i++)
        if (inportb(MOUSE_STATUS) & 0x01)
            return 1;
    return 0;
}

void mouse_wait(uint8_t type) {
    if (type == 1) i8042_wait_read();
    else           i8042_wait_write();
}

void mouse_write(uint8_t write) {
    i8042_wait_write();
    outportb(MOUSE_STATUS, MOUSE_WRITE);   /* 0xD4: next data byte -> aux port */
    i8042_wait_write();
    outportb(MOUSE_PORT, write);
}

uint8_t mouse_read() {
    return i8042_wait_read() ? inportb(MOUSE_PORT) : 0;
}

/** @brief Send @p cmd to the aux device, return its reply (0xFA = ACK). */
static uint8_t aux_cmd(uint8_t cmd) {
    for (int t = 0; t < 3; t++) {
        mouse_write(cmd);
        uint8_t r = mouse_read();
        if (r != 0xFE)
            return r;
    }
    return 0;
}

static void aux_drain(void) {
    for (int i = 0; i < 16 && (inportb(MOUSE_STATUS) & 0x01); i++)
        (void) inportb(MOUSE_PORT);
}

/* ------------------------------------------------------------------ *
 *  Synaptics                                                          *
 * ------------------------------------------------------------------ */
static int      syn_mode;          /* 1 once the touchpad is in absolute mode */
static int      syn_passthrough;   /* 1 if it forwards a guest (TrackPoint) */
static int      pad_down;          /* finger currently on the pad */
static int      pad_px, pad_py;    /* last absolute finger position */

/** @brief Encode an 8-bit value into four SET-RESOLUTION commands. */
static void syn_encode(uint8_t v) {
    aux_cmd(0xE8); aux_cmd((v >> 6) & 3);
    aux_cmd(0xE8); aux_cmd((v >> 4) & 3);
    aux_cmd(0xE8); aux_cmd((v >> 2) & 3);
    aux_cmd(0xE8); aux_cmd(v & 3);
}

/** @brief Run a Synaptics query @p q, returning its 3 status bytes. */
static int syn_query(uint8_t q, uint8_t out[3]) {
    syn_encode(q);
    if (aux_cmd(0xE9) != 0xFA)
        return 0;
    out[0] = mouse_read();
    out[1] = mouse_read();
    out[2] = mouse_read();
    return 1;
}

/** @brief Set the Synaptics mode byte (query 0x14 = "set mode"). */
static void syn_set_mode(uint8_t mode) {
    syn_encode(mode);
    aux_cmd(0xF3);
    aux_cmd(0x14);
}

/** @return 1 if a Synaptics touchpad answered and absolute mode is armed. */
static int syn_init(void) {
    if (cmdline_has("nosyn"))
        return 0;

    uint8_t id[3];
    /* Identify (query 0x00): a Synaptics pad returns 0x47 as the middle byte. */
    if (!syn_query(0x00, id) || id[1] != 0x47)
        return 0;

    uint8_t cap[3];
    /* Read Capabilities (query 0x02): the 24-bit value is cap[0]<<16 |
     * cap[1]<<8 | cap[2]; cap[1] must read back 0x47 for the answer to be
     * valid, and SYN_CAP_PASS_THROUGH is bit 7 of the low byte. */
    if (syn_query(0x02, cap) && cap[1] == 0x47)
        syn_passthrough = (cap[2] & 0x80) != 0;

    /* Mode byte: absolute (0x80) | high report rate (0x40) |
     * disable-gesture (0x04) | W-mode / extended packets (0x01).
     * W-mode is what makes the pad emit pass-through (W==3) packets. */
    syn_set_mode(0x80 | 0x40 | 0x04 | 0x01);
    aux_cmd(0xF4);                 /* enable reporting */
    aux_drain();

    syn_mode = 1;
    klogf(LOG_INFO, "mouse: Synaptics touchpad, pass-through %s\n",
          syn_passthrough ? "on (TrackPoint)" : "off");
    return 1;
}

/* ------------------------------------------------------------------ *
 *  Button / motion helpers                                            *
 * ------------------------------------------------------------------ */
mouse_info_t *get_mouse_info() { return &mouse_info; }

int mouse_left_button_down()  { return (mouse_info.prev_button != LEFT_CLICK)  && (mouse_info.curr_button == LEFT_CLICK); }
int mouse_right_button_down() { return (mouse_info.prev_button != RIGHT_CLICK) && (mouse_info.curr_button == RIGHT_CLICK); }
int mouse_left_button_up()    { return (mouse_info.prev_button == LEFT_CLICK)  && (mouse_info.curr_button != LEFT_CLICK); }
int mouse_right_button_up()   { return (mouse_info.prev_button == RIGHT_CLICK) && (mouse_info.curr_button != RIGHT_CLICK); }

static void apply_rel(int dx, int dy, uint32_t buttons) {
    mouse_info.x += dx;
    mouse_info.y += dy;
    mouse_check_bounds();
    mouse_info.curr_button = buttons;
}

/* ------------------------------------------------------------------ *
 *  Packet decode                                                      *
 * ------------------------------------------------------------------ */
static void decode_plain(const uint8_t *p) {
    if (p[0] & 0xC0)                          /* X / Y overflow */
        return;
    apply_rel((signed char) p[1], -(signed char) p[2],
              ((p[0] & 1) ? LEFT_CLICK   : 0) |
              ((p[0] & 2) ? RIGHT_CLICK  : 0) |
              ((p[0] & 4) ? MIDDLE_CLICK : 0));
}

static void decode_synaptics(const uint8_t *b) {
    int w = ((b[0] & 0x30) >> 2) | ((b[0] & 0x04) >> 1) | ((b[3] & 0x04) >> 2);

    if (w == 3 && syn_passthrough) {
        /* Pass-through packet: the TrackPoint's own 3-byte PS/2 packet lives
         * in bytes 1, 4, 5. */
        uint8_t g0 = b[1], g1 = b[4], g2 = b[5];
        int dx = (g0 & 0x10) ? (int) g1 - 256 : (int) g1;
        int dy = (g0 & 0x20) ? (int) g2 - 256 : (int) g2;
        apply_rel(dx, -dy,
                  ((g0 & 1) ? LEFT_CLICK   : 0) |
                  ((g0 & 2) ? RIGHT_CLICK  : 0) |
                  ((g0 & 4) ? MIDDLE_CLICK : 0));
        return;
    }

    int x = ((b[3] & 0x10) << 8) | ((b[1] & 0x0F) << 8) | b[4];
    int y = ((b[3] & 0x20) << 7) | ((b[1] & 0xF0) << 4) | b[5];
    int z = b[2];

    /* ClickPad: no discrete buttons -- a physical click shows as byte0 bit0
     * differing from byte3 bit0. (The TrackPoint's own buttons arrive in the
     * w == 3 pass-through packet above.) */
    int left = ((b[0] ^ b[3]) & 0x01) ? LEFT_CLICK : 0;
    mouse_info.curr_button = (mouse_info.curr_button & ~(LEFT_CLICK | RIGHT_CLICK)) | left;

    /* Only a single finger (w in 4..15) steers the cursor. w in {0,1,2} is
     * multi-finger / advanced-gesture data whose x/y fields mean something
     * else -- decoding it as a position is what made a resting second finger
     * fling the pointer across the screen. */
    if (w >= 4 && z > 30) {
        if (pad_down) {
            int dx = (x - pad_px) / 6;        /* Synaptics units -> pixels */
            int dy = (pad_py - y) / 6;        /* pad Y grows upward */
            if (dx || dy)
                apply_rel(dx, dy, mouse_info.curr_button);
        }
        pad_px = x; pad_py = y;
        pad_down = 1;
    } else {
        pad_down = 0;
    }
}

void mouse_handler() {
    /* MUST be static: the i8042 raises one IRQ per byte received, so a 6-byte
     * Synaptics packet is assembled over six separate calls to this handler.
     * A local array would be a fresh (uninitialised) stack slot every call and
     * every packet would decode from garbage -- the cursor jumps at random for
     * both the touchpad and the pass-through TrackPoint. */
    static uint8_t pkt[6];
    int need = syn_mode ? 6 : 3;
    uint8_t status;

    while ((status = inportb(MOUSE_STATUS)) & MOUSE_BBIT) {
        uint8_t b = inportb(MOUSE_PORT);
        if (!(status & MOUSE_F_BIT))
            continue;                         /* keyboard byte */

        if (syn_mode) {
            /* Absolute packets: byte 0 is 1 0 x x 0 x x x  ((b & 0xC8) == 0x80),
             * byte 3 is 1 1 x x 0 x x x  ((b & 0xC8) == 0xC0). A byte that
             * fails its slot's test means a byte was dropped -- realign by
             * reconsidering it as a fresh byte 0. */
            if (mouse_cycle == 0 && (b & 0xC8) != 0x80)
                continue;
            if (mouse_cycle == 3 && (b & 0xC8) != 0xC0) {
                mouse_cycle = 0;
                if ((b & 0xC8) != 0x80)
                    continue;
            }
        } else if (mouse_cycle == 0 && !(b & MOUSE_V_BIT)) {
            continue;                         /* plain byte 0 lost sync */
        }

        pkt[mouse_cycle++] = b;
        if (mouse_cycle < need)
            continue;
        mouse_cycle = 0;

        if (syn_mode) decode_synaptics(pkt);
        else          decode_plain(pkt);
    }
}

void mouse_check_bounds() {
    int w = vbemem.xres ? vbemem.xres : 1280;
    int h = vbemem.yres ? vbemem.yres : 1024;
    if (mouse_info.x > w - 1) mouse_info.x = w - 1;
    else if (mouse_info.x < 0) mouse_info.x = 0;
    if (mouse_info.y > h - 1) mouse_info.y = h - 1;
    else if (mouse_info.y < 0) mouse_info.y = 0;
}

void mouse_init() {
    mouse_info.x = 0;
    mouse_info.y = 0;
    mouse_cycle = 0;

    /* Enable the aux port; config byte: IRQ12 on (bit 1), aux clock on (bit 5=0). */
    i8042_wait_write(); outportb(MOUSE_STATUS, 0xA8);
    i8042_wait_write(); outportb(MOUSE_STATUS, 0x20);
    uint8_t cfg = mouse_read();
    cfg |= (1u << 1);
    cfg &= ~(1u << 5);
    i8042_wait_write(); outportb(MOUSE_STATUS, 0x60);
    i8042_wait_write(); outportb(MOUSE_PORT, cfg);

    aux_cmd(0xFF);                 /* reset (-> FA AA 00) */
    aux_drain();
    aux_cmd(0xF6);                 /* load defaults */

    if (!syn_init()) {
        /* Plain PS/2 relative mode. */
        aux_cmd(0xF3); aux_cmd(100);
        aux_cmd(0xE8); aux_cmd(0x02);
        aux_cmd(0xF4);
        aux_drain();
    }

    install_ir(44, 0x80 | 0x0E, 0x8, &mouse_int);
}
