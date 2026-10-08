/*
 * gamepad-test.c -- the game-controller driver's logic, tested as a plain
 * program: no kernel, no QEMU, no hardware.
 *
 *   make test-gamepad
 *
 * gamepad.c touches no hardware (the host controllers do the transfers), so it
 * builds here against the stand-in headers in test/hoststubs. What this checks
 * is the part QEMU cannot vary: report descriptors of every shape the pads out
 * there use, and the state each report turns into.
 *
 * The descriptors are written out byte by byte, modelled on the four styles of
 * pad there are (a plain DirectInput pad, a DualShock 4, an Xbox pad over
 * Bluetooth HID, an Xbox pad in DirectInput mode) plus the corner cases that
 * parse differently: signed axes, a four-way hat, a hat that counts from 1,
 * D-pad "buttons", Push/Pop, long items. They are *modelled on* those pads, not
 * dumped from them -- the point is each parsing path, and when a real pad is
 * misread the console's `pad` command and `pad dump` give a descriptor to add
 * here.
 *
 * The last test feeds the parser random garbage under AddressSanitizer: it reads
 * bytes from a device nobody has vetted, so it must never read out of bounds,
 * whatever it is told.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gamepad.h>

static int checks, failures;

#define CHECK(cond) do { \
        checks++; \
        if(!(cond)) { \
            failures++; \
            printf("  FAIL line %d: %s\n", __LINE__, #cond); \
        } \
    } while(0)

#define SECTION(name) printf("%s\n", name)

/* A descriptor under construction. */
typedef struct { uint8_t b[1024]; int n; } desc_t;

static void put(desc_t *d, int count, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, count);
    for(int i = 0; i < count; i++)
        d->b[d->n++] = (uint8_t) __builtin_va_arg(ap, int);
    __builtin_va_end(ap);
}
#define PUT(d, ...) put(d, (int) (sizeof((int[]){__VA_ARGS__}) / sizeof(int)), __VA_ARGS__)

static int attach(const desc_t *d) {
    return gamepad_attach(0x1234, 0x5678, d->b, d->n);
}

static void detach_all(void) {
    for(int i = 0; i < GAMEPAD_MAX; i++)
        gamepad_detach(i);
}

static gamepad_state_t state(int pad) {
    gamepad_state_t s;
    memset(&s, 0xAA, sizeof(s));
    CHECK(gamepad_get(pad, &s) == 1);
    return s;
}

static int near(int v, int want, int tol) { return v >= want - tol && v <= want + tol; }

/* ------------------------------------------------------------------ *
 *  Descriptors                                                        *
 * ------------------------------------------------------------------ */

/* A plain DirectInput pad: Game Pad, X Y Z Rz as 8-bit axes, an eight-way hat,
 * 12 buttons, no report IDs. 7-byte reports. */
static void generic_pad(desc_t *d) {
    PUT(d, 0x05,0x01, 0x09,0x05, 0xA1,0x01);
    PUT(d, 0x15,0x00, 0x26,0xFF,0x00, 0x75,0x08, 0x95,0x04);
    PUT(d, 0x09,0x30, 0x09,0x31, 0x09,0x32, 0x09,0x35, 0x81,0x02);
    PUT(d, 0x25,0x07, 0x35,0x00, 0x46,0x3B,0x01, 0x65,0x14);
    PUT(d, 0x75,0x04, 0x95,0x01, 0x09,0x39, 0x81,0x42);
    PUT(d, 0x65,0x00, 0x75,0x04, 0x95,0x01, 0x81,0x03);
    PUT(d, 0x05,0x09, 0x19,0x01, 0x29,0x0C, 0x15,0x00, 0x25,0x01, 0x75,0x01, 0x95,0x0C, 0x81,0x02);
    PUT(d, 0x75,0x01, 0x95,0x04, 0x81,0x03);
    PUT(d, 0xC0);
}

/* A DualShock 4's shape: report ID 1, X Y Z Rz sticks, hat, 14 buttons, a
 * vendor counter, Rx Ry triggers, 54 vendor bytes -- 64-byte reports -- then
 * feature reports and a second, vendor application collection. */
static void ds4_pad(desc_t *d) {
    PUT(d, 0x05,0x01, 0x09,0x05, 0xA1,0x01, 0x85,0x01);
    PUT(d, 0x09,0x30, 0x09,0x31, 0x09,0x32, 0x09,0x35);
    PUT(d, 0x15,0x00, 0x26,0xFF,0x00, 0x75,0x08, 0x95,0x04, 0x81,0x02);
    PUT(d, 0x09,0x39, 0x15,0x00, 0x25,0x07, 0x35,0x00, 0x46,0x3B,0x01, 0x65,0x14);
    PUT(d, 0x75,0x04, 0x95,0x01, 0x81,0x42);
    PUT(d, 0x65,0x00, 0x05,0x09, 0x19,0x01, 0x29,0x0E, 0x15,0x00, 0x25,0x01);
    PUT(d, 0x75,0x01, 0x95,0x0E, 0x81,0x02);
    PUT(d, 0x06,0x00,0xFF, 0x09,0x20, 0x75,0x06, 0x95,0x01, 0x15,0x00, 0x25,0x7F, 0x81,0x02);
    PUT(d, 0x05,0x01, 0x09,0x33, 0x09,0x34, 0x15,0x00, 0x26,0xFF,0x00, 0x75,0x08, 0x95,0x02, 0x81,0x02);
    PUT(d, 0x06,0x00,0xFF, 0x09,0x21, 0x95,0x36, 0x81,0x02);
    PUT(d, 0x85,0x05, 0x09,0x22, 0x95,0x1F, 0x91,0x02);
    for(int id = 2; id < 0x1E; id++)
        PUT(d, 0x85,id, 0x09,0x24, 0x95,0x24, 0xB1,0x02);
    PUT(d, 0xC0, 0x06,0xF0,0xFF, 0x09,0x40, 0xA1,0x01);
    for(int id = 0xF0; id < 0xF8; id++)
        PUT(d, 0x85,id, 0x09,0x47, 0x95,0x3F, 0xB1,0x02);
    PUT(d, 0xC0);
}

/* An Xbox pad over Bluetooth HID: report ID 1, X Y and Z Rz as 16-bit unsigned
 * sticks (each pair in a physical collection), Brake and Accelerator as 10-bit
 * triggers, a hat counting 1..8, 15 buttons, a consumer-page bit. */
static void xbox_hid_pad(desc_t *d) {
    PUT(d, 0x05,0x01, 0x09,0x05, 0xA1,0x01, 0x85,0x01);
    PUT(d, 0x09,0x01, 0xA1,0x00);
    PUT(d, 0x09,0x30, 0x09,0x31, 0x15,0x00, 0x27,0xFF,0xFF,0x00,0x00, 0x95,0x02, 0x75,0x10, 0x81,0x02);
    PUT(d, 0xC0);
    PUT(d, 0x09,0x01, 0xA1,0x00);
    PUT(d, 0x09,0x32, 0x09,0x35, 0x15,0x00, 0x27,0xFF,0xFF,0x00,0x00, 0x95,0x02, 0x75,0x10, 0x81,0x02);
    PUT(d, 0xC0);
    PUT(d, 0x05,0x02, 0x09,0xC5, 0x15,0x00, 0x26,0xFF,0x03, 0x95,0x01, 0x75,0x0A, 0x81,0x02);
    PUT(d, 0x15,0x00, 0x25,0x00, 0x75,0x06, 0x95,0x01, 0x81,0x03);
    PUT(d, 0x05,0x02, 0x09,0xC4, 0x15,0x00, 0x26,0xFF,0x03, 0x95,0x01, 0x75,0x0A, 0x81,0x02);
    PUT(d, 0x15,0x00, 0x25,0x00, 0x75,0x06, 0x95,0x01, 0x81,0x03);
    PUT(d, 0x05,0x01, 0x09,0x39, 0x15,0x01, 0x25,0x08, 0x35,0x00, 0x46,0x3B,0x01, 0x66,0x14,0x00);
    PUT(d, 0x75,0x04, 0x95,0x01, 0x81,0x42);
    PUT(d, 0x15,0x00, 0x25,0x00, 0x75,0x04, 0x95,0x01, 0x81,0x03);
    PUT(d, 0x05,0x09, 0x19,0x01, 0x29,0x0F, 0x15,0x00, 0x25,0x01, 0x75,0x01, 0x95,0x0F, 0x81,0x02);
    PUT(d, 0x15,0x00, 0x25,0x00, 0x75,0x01, 0x95,0x01, 0x81,0x03);
    PUT(d, 0x05,0x0C, 0x0A,0x24,0x02, 0x15,0x00, 0x25,0x01, 0x95,0x01, 0x75,0x01, 0x81,0x02);
    PUT(d, 0x15,0x00, 0x25,0x00, 0x75,0x07, 0x95,0x01, 0x81,0x03);
    PUT(d, 0xC0);
}

/* An Xbox pad in DirectInput mode: X Y and Rx Ry as signed 16-bit sticks and
 * ONE Z axis for both triggers (8-bit, resting at the middle), a hat, 10
 * buttons. */
static void xbox_dinput_pad(desc_t *d) {
    PUT(d, 0x05,0x01, 0x09,0x04, 0xA1,0x01);                    /* Joystick */
    PUT(d, 0x09,0x30, 0x09,0x31, 0x16,0x00,0x80, 0x26,0xFF,0x7F, 0x75,0x10, 0x95,0x02, 0x81,0x02);
    PUT(d, 0x09,0x32, 0x15,0x00, 0x26,0xFF,0x00, 0x75,0x08, 0x95,0x01, 0x81,0x02);
    PUT(d, 0x09,0x33, 0x09,0x34, 0x16,0x00,0x80, 0x26,0xFF,0x7F, 0x75,0x10, 0x95,0x02, 0x81,0x02);
    PUT(d, 0x09,0x39, 0x15,0x00, 0x25,0x07, 0x75,0x04, 0x95,0x01, 0x81,0x42);
    PUT(d, 0x75,0x04, 0x95,0x01, 0x81,0x03);
    PUT(d, 0x05,0x09, 0x19,0x01, 0x29,0x0A, 0x15,0x00, 0x25,0x01, 0x75,0x01, 0x95,0x0A, 0x81,0x02);
    PUT(d, 0x75,0x06, 0x95,0x01, 0x81,0x03);
    PUT(d, 0xC0);
}

/* A joystick with signed 8-bit axes (Usage Joystick) and a four-way hat counting 0..3. */
static void signed_pad(desc_t *d) {
    PUT(d, 0x05,0x01, 0x09,0x04, 0xA1,0x01);
    PUT(d, 0x09,0x30, 0x09,0x31, 0x15,0x81, 0x25,0x7F, 0x75,0x08, 0x95,0x02, 0x81,0x02);
    PUT(d, 0x09,0x39, 0x15,0x00, 0x25,0x03, 0x75,0x04, 0x95,0x01, 0x81,0x42);
    PUT(d, 0x75,0x04, 0x95,0x01, 0x81,0x03);
    PUT(d, 0x05,0x09, 0x19,0x01, 0x29,0x08, 0x15,0x00, 0x25,0x01, 0x75,0x01, 0x95,0x08, 0x81,0x02);
    PUT(d, 0xC0);
}

/* A pad whose D-pad is four buttons (Generic Desktop usages 0x90..0x93, listed
 * one by one), with Push / Pop around a size change and a long item to skip. */
static void dpad_buttons_pad(desc_t *d) {
    PUT(d, 0x05,0x01, 0x09,0x05, 0xA1,0x01);
    PUT(d, 0xFE,0x04,0x00,0x11, 1,2,3,4);                       /* a long item */
    PUT(d, 0x05,0x01, 0x15,0x00, 0x25,0x01, 0x75,0x01);
    PUT(d, 0xA4);                                               /* Push */
    PUT(d, 0x09,0x90, 0x09,0x91, 0x09,0x92, 0x09,0x93, 0x95,0x04, 0x81,0x02);
    PUT(d, 0x75,0x04, 0x95,0x01, 0x81,0x03);                    /* padding nibble */
    PUT(d, 0xB4);                                               /* Pop: back to 1-bit, 0..1 */
    PUT(d, 0x05,0x09, 0x19,0x01, 0x29,0x08, 0x95,0x08, 0x81,0x02);
    PUT(d, 0xC0);
}

/* A hat counting from 1 (1..8) with the null state 0. */
static void hat1_pad(desc_t *d) {
    PUT(d, 0x05,0x01, 0x09,0x05, 0xA1,0x01);
    PUT(d, 0x09,0x39, 0x15,0x01, 0x25,0x08, 0x75,0x08, 0x95,0x01, 0x81,0x42);
    PUT(d, 0x05,0x09, 0x19,0x01, 0x29,0x04, 0x15,0x00, 0x25,0x01, 0x75,0x01, 0x95,0x04, 0x81,0x02);
    PUT(d, 0x75,0x04, 0x95,0x01, 0x81,0x03);
    PUT(d, 0xC0);
}

/* 32 buttons, one byte per report, two axes with a 32-bit range. */
static void wide_pad(desc_t *d) {
    PUT(d, 0x05,0x01, 0x09,0x04, 0xA1,0x01);
    PUT(d, 0x09,0x30, 0x09,0x31, 0x17,0x00,0x00,0x00,0x80, 0x27,0xFF,0xFF,0xFF,0x7F, 0x75,0x20, 0x95,0x02, 0x81,0x02);
    PUT(d, 0x05,0x09, 0x19,0x01, 0x29,0x20, 0x15,0x00, 0x25,0x01, 0x75,0x01, 0x95,0x20, 0x81,0x02);
    PUT(d, 0xC0);
}

/* A standard boot keyboard and boot mouse: neither is a pad. */
static void boot_keyboard(desc_t *d) {
    PUT(d, 0x05,0x01, 0x09,0x06, 0xA1,0x01, 0x05,0x07, 0x19,0xE0, 0x29,0xE7, 0x15,0x00, 0x25,0x01);
    PUT(d, 0x75,0x01, 0x95,0x08, 0x81,0x02, 0x95,0x01, 0x75,0x08, 0x81,0x01);
    PUT(d, 0x95,0x06, 0x75,0x08, 0x15,0x00, 0x25,0x65, 0x05,0x07, 0x19,0x00, 0x29,0x65, 0x81,0x00, 0xC0);
}
static void boot_mouse(desc_t *d) {
    PUT(d, 0x05,0x01, 0x09,0x02, 0xA1,0x01, 0x09,0x01, 0xA1,0x00, 0x05,0x09, 0x19,0x01, 0x29,0x03);
    PUT(d, 0x15,0x00, 0x25,0x01, 0x95,0x03, 0x75,0x01, 0x81,0x02, 0x95,0x01, 0x75,0x05, 0x81,0x01);
    PUT(d, 0x05,0x01, 0x09,0x30, 0x09,0x31, 0x15,0x81, 0x25,0x7F, 0x75,0x08, 0x95,0x02, 0x81,0x06);
    PUT(d, 0xC0, 0xC0);
}

/* ------------------------------------------------------------------ *
 *  Tests                                                              *
 * ------------------------------------------------------------------ */

static void test_generic(void) {
    SECTION("generic DirectInput pad");
    desc_t d = {0}; generic_pad(&d);
    int p = attach(&d);
    CHECK(p == 0);

    /* centred, hat released (15 is outside 0..7: the null state) */
    const uint8_t rest[7] = {128,128,128,128, 0x0F, 0x00, 0x00};
    gamepad_report(p, rest, sizeof(rest));
    gamepad_state_t s = state(p);
    CHECK(near(s.lx, 0, 200) && near(s.ly, 0, 200) && near(s.rx, 0, 200) && near(s.ry, 0, 200));
    CHECK(s.dpad == 0 && s.buttons == 0 && s.lt == 0 && s.rt == 0);

    /* X full right, Y full up (0), Z/Rz are the right stick, hat north-east, buttons 1 3 12 */
    const uint8_t r1[7] = {255, 0, 0, 255, 0x01, 0x05, 0x08};
    gamepad_report(p, r1, sizeof(r1));
    s = state(p);
    CHECK(s.lx == 32767 && s.ly == -32768);
    CHECK(s.rx == -32768 && s.ry == 32767);
    CHECK(s.dpad == (GAMEPAD_DPAD_UP | GAMEPAD_DPAD_RIGHT));
    CHECK(s.buttons == (1u | 4u | (1u << 11)));

    /* every hat position, clockwise from north */
    static const uint8_t want[8] = {
        GAMEPAD_DPAD_UP, GAMEPAD_DPAD_UP | GAMEPAD_DPAD_RIGHT, GAMEPAD_DPAD_RIGHT,
        GAMEPAD_DPAD_DOWN | GAMEPAD_DPAD_RIGHT, GAMEPAD_DPAD_DOWN,
        GAMEPAD_DPAD_DOWN | GAMEPAD_DPAD_LEFT, GAMEPAD_DPAD_LEFT,
        GAMEPAD_DPAD_UP | GAMEPAD_DPAD_LEFT };
    for(int h = 0; h < 8; h++) {
        uint8_t r[7] = {128,128,128,128, (uint8_t) h, 0, 0};
        gamepad_report(p, r, sizeof(r));
        CHECK(state(p).dpad == want[h]);
    }

    /* a report too short to hold the buttons leaves the state alone */
    gamepad_report(p, r1, 4);
    CHECK(state(p).dpad == want[7]);

    char buf[160];
    CHECK(gamepad_describe(p, buf, sizeof(buf)) == 1);
    CHECK(strstr(buf, "12 buttons") && strstr(buf, "hat") && strstr(buf, "lx ly rx ry"));
    detach_all();
}

static void test_ds4(void) {
    SECTION("DualShock-4-shaped pad (report IDs, vendor collections, long descriptor)");
    desc_t d = {0}; ds4_pad(&d);
    CHECK(d.n > 400);
    int p = attach(&d);
    CHECK(p == 0);

    uint8_t r[64] = {0};
    r[0] = 1;
    r[1] = 255; r[2] = 0; r[3] = 128; r[4] = 128;          /* LX right, LY up */
    r[5] = 0x02 | (0x05 << 4);                              /* hat east; buttons 1, 3 */
    r[6] = 0x80 | 0x01;                                     /* buttons 5 and 12 */
    r[7] = 0x01 | (9 << 2);                                 /* button 13, counter 9 */
    r[8] = 200; r[9] = 50;                                  /* L2, R2 analog */
    gamepad_report(p, r, sizeof(r));
    gamepad_state_t s = state(p);
    CHECK(s.lx == 32767 && s.ly == -32768);
    CHECK(s.dpad == GAMEPAD_DPAD_RIGHT);
    CHECK(s.buttons == (1u | 4u | (1u << 4) | (1u << 11) | (1u << 12)));
    CHECK(near(s.lt, 200, 1) && near(s.rt, 50, 1));         /* Rx, Ry are the triggers here */

    /* the hat's null state on this pad is 8 */
    r[5] = 0x08;
    gamepad_report(p, r, sizeof(r));
    CHECK(state(p).dpad == 0);

    /* another report ID (a Bluetooth-style 0x11) changes nothing */
    uint8_t other[64] = {0}; other[0] = 0x11; other[1] = 0; other[5] = 0xFF;
    gamepad_report(p, other, sizeof(other));
    CHECK(state(p).dpad == 0 && state(p).lx == 32767);

    /* the vendor collection's feature items are not read as input */
    char buf[160];
    CHECK(gamepad_describe(p, buf, sizeof(buf)) == 1);
    CHECK(strstr(buf, "14 buttons") && strstr(buf, "report IDs") && strstr(buf, "lt rt"));
    detach_all();
}

static void test_xbox_hid(void) {
    SECTION("Xbox pad over Bluetooth HID (16-bit sticks, Brake/Accelerator, hat 1..8)");
    desc_t d = {0}; xbox_hid_pad(&d);
    int p = attach(&d);
    CHECK(p == 0);

    uint8_t r[17] = {0};
    r[0] = 1;
    r[1] = 0xFF; r[2] = 0xFF;                   /* X max */
    r[3] = 0x00; r[4] = 0x00;                   /* Y min */
    r[5] = 0x00; r[6] = 0x80;                   /* Z centre-ish */
    r[7] = 0xFF; r[8] = 0xFF;                   /* Rz max */
    r[9] = 0xFF; r[10] = 0x03;                  /* Brake 1023 */
    r[11] = 0x00; r[12] = 0x02;                 /* Accelerator 512 */
    r[13] = 0x08;                               /* hat 8 = north-west (1 is north) */
    r[14] = 0x03; r[15] = 0x40;                 /* buttons 1, 2 and 15 */
    gamepad_report(p, r, sizeof(r));
    gamepad_state_t s = state(p);
    CHECK(s.lx == 32767 && s.ly == -32768);
    CHECK(near(s.rx, 0, 2) && s.ry == 32767);
    CHECK(s.lt == 255 && near(s.rt, 128, 1));
    CHECK(s.dpad == (GAMEPAD_DPAD_UP | GAMEPAD_DPAD_LEFT));
    CHECK(s.buttons == (3u | (1u << 14)));

    r[13] = 0x00;                               /* hat released: 0 is out of 1..8 */
    gamepad_report(p, r, sizeof(r));
    CHECK(state(p).dpad == 0);
    r[13] = 0x01;
    gamepad_report(p, r, sizeof(r));
    CHECK(state(p).dpad == GAMEPAD_DPAD_UP);
    detach_all();
}

static void test_xbox_dinput(void) {
    SECTION("Xbox pad in DirectInput mode (signed sticks, one Z axis for both triggers)");
    desc_t d = {0}; xbox_dinput_pad(&d);
    int p = attach(&d);
    CHECK(p == 0);

    /* X Y (s16), Z (u8), Rx Ry (s16), hat+pad, buttons (10) + pad -> 2+2+1+2+2+1+2 */
    uint8_t r[12] = {0};
    r[0] = 0xFF; r[1] = 0x7F;                   /* X = 32767 */
    r[2] = 0x00; r[3] = 0x80;                   /* Y = -32768 */
    r[4] = 255;                                 /* Z full: left trigger */
    r[5] = 0x00; r[6] = 0x80;                   /* Rx = -32768 */
    r[7] = 0xFF; r[8] = 0x7F;                   /* Ry = 32767 */
    r[9] = 0x0F;                                /* hat released */
    r[10] = 0x01; r[11] = 0x02;                 /* buttons 1 and 10 */
    gamepad_report(p, r, sizeof(r));
    gamepad_state_t s = state(p);
    CHECK(s.lx == 32767 && s.ly == -32768 && s.rx == -32768 && s.ry == 32767);
    CHECK(s.lt == 255 && s.rt == 0);
    CHECK(s.buttons == (1u | (1u << 9)));

    r[4] = 0;                                   /* Z the other way: right trigger */
    gamepad_report(p, r, sizeof(r));
    s = state(p);
    CHECK(s.rt == 255 && s.lt == 0);            /* -32768 used to wrap to 0 */

    r[4] = 128;                                 /* rest */
    gamepad_report(p, r, sizeof(r));
    s = state(p);
    CHECK(s.lt <= 2 && s.rt <= 2);
    detach_all();
}

static void test_signed_and_fourway(void) {
    SECTION("signed axes, four-way hat");
    desc_t d = {0}; signed_pad(&d);
    int p = attach(&d);
    CHECK(p == 0);
    uint8_t r[3] = { 0x7F, 0x81, 0x00 };        /* X +127, Y -127, hat 0 = north */
    r[2] = 0x00 | (0 << 4);
    gamepad_report(p, r, sizeof(r));
    gamepad_state_t s = state(p);
    CHECK(s.lx == 32767 && s.ly == -32768);
    CHECK(s.dpad == GAMEPAD_DPAD_UP);
    r[2] = 0x02;                                /* 4-way: 2 = south */
    gamepad_report(p, r, sizeof(r));
    CHECK(state(p).dpad == GAMEPAD_DPAD_DOWN);
    r[2] = 0x03;
    gamepad_report(p, r, sizeof(r));
    CHECK(state(p).dpad == GAMEPAD_DPAD_LEFT);
    r[2] = 0x0F;                                /* released */
    gamepad_report(p, r, sizeof(r));
    CHECK(state(p).dpad == 0);
    detach_all();
}

static void test_dpad_buttons(void) {
    SECTION("D-pad as buttons, Push/Pop, long item");
    desc_t d = {0}; dpad_buttons_pad(&d);
    int p = attach(&d);
    CHECK(p == 0);
    /* byte 0: up(0x01) down(0x02) right(0x04) left(0x08) | pad nibble; byte 1: buttons */
    uint8_t r[2] = { 0x01 | 0x08, 0x81 };
    gamepad_report(p, r, sizeof(r));
    gamepad_state_t s = state(p);
    CHECK(s.dpad == (GAMEPAD_DPAD_UP | GAMEPAD_DPAD_LEFT));
    CHECK(s.buttons == (1u | (1u << 7)));
    r[0] = 0x04;
    gamepad_report(p, r, sizeof(r));
    CHECK(state(p).dpad == GAMEPAD_DPAD_RIGHT);
    detach_all();
}

static void test_hat_from_one(void) {
    SECTION("hat counting from 1, null state 0");
    desc_t d = {0}; hat1_pad(&d);
    int p = attach(&d);
    CHECK(p == 0);
    uint8_t r[2] = { 3, 0x0A };                 /* hat 3 = east; buttons 2 and 4 */
    gamepad_report(p, r, sizeof(r));
    gamepad_state_t s = state(p);
    CHECK(s.dpad == GAMEPAD_DPAD_RIGHT && s.buttons == 0x0A);
    r[0] = 0;
    gamepad_report(p, r, sizeof(r));
    CHECK(state(p).dpad == 0);
    detach_all();
}

static void test_wide(void) {
    SECTION("32 buttons, 32-bit axes");
    desc_t d = {0}; wide_pad(&d);
    int p = attach(&d);
    CHECK(p == 0);
    uint8_t r[12] = {0};
    r[0]=0xFF; r[1]=0xFF; r[2]=0xFF; r[3]=0x7F;     /* X = INT32_MAX */
    r[4]=0x00; r[5]=0x00; r[6]=0x00; r[7]=0x80;     /* Y = INT32_MIN */
    r[8]=0x01; r[11]=0x80;                          /* buttons 1 and 32 */
    gamepad_report(p, r, sizeof(r));
    gamepad_state_t s = state(p);
    CHECK(s.lx == 32767 && s.ly == -32768);
    CHECK(s.buttons == 0x80000001u);
    detach_all();
}

static void test_not_a_pad(void) {
    SECTION("keyboards and mice are turned down");
    desc_t d = {0};
    boot_keyboard(&d);
    CHECK(attach(&d) == -1);
    d.n = 0; boot_mouse(&d);
    CHECK(attach(&d) == -1);
    CHECK(gamepad_count() == 0);
    d.n = 0;
    CHECK(attach(&d) == -1);                    /* empty */
    uint8_t junk[3] = { 0x05, 0x01, 0x09 };
    CHECK(gamepad_attach(1, 2, junk, 3) == -1); /* cut off mid-item */

    /* a pad's descriptor cut short after the collection opens but before any
     * input item is no pad either */
    d.n = 0; PUT(&d, 0x05,0x01, 0x09,0x05, 0xA1,0x01);
    CHECK(attach(&d) == -1);
}

static void test_slots(void) {
    SECTION("slots: fill, release, reuse, release twice");
    desc_t d = {0}; generic_pad(&d);
    int a = attach(&d), b = attach(&d), c = attach(&d), e = attach(&d);
    CHECK(a == 0 && b == 1 && c == 2 && e == 3);
    CHECK(attach(&d) == -1);                    /* GAMEPAD_MAX is 4 */
    CHECK(gamepad_count() == 4);

    const uint8_t r[7] = {255,128,128,128, 0x0F, 0x01, 0x00};
    gamepad_report(b, r, sizeof(r));
    CHECK(state(b).buttons == 1);
    gamepad_detach(b);
    gamepad_state_t s;
    CHECK(gamepad_get(b, &s) == 0);             /* gone, and nothing left to read */
    gamepad_report(b, r, sizeof(r));            /* a late report for a dead pad: ignored */
    CHECK(gamepad_get(b, &s) == 0);
    gamepad_detach(b);                          /* twice is harmless */
    CHECK(gamepad_count() == 3);

    CHECK(attach(&d) == b);                     /* the freed slot is reused ... */
    s = state(b);
    CHECK(s.buttons == 0 && s.dpad == 0);       /* ... clean */

    CHECK(gamepad_get(-1, &s) == 0 && gamepad_get(GAMEPAD_MAX, &s) == 0);
    gamepad_report(-1, r, sizeof(r));           /* out-of-range ids are ignored */
    gamepad_report(99, r, sizeof(r));
    gamepad_detach(-5);
    detach_all();
}

static void test_xinput(void) {
    SECTION("Xbox 360 wired controller");
    int p = gamepad_attach_xinput(0x045E, 0x028E);
    CHECK(p == 0);

    uint8_t r[GAMEPAD_XINPUT_REPORT] = {0};
    r[0] = 0x00; r[1] = 0x14;
    r[2] = 0x01 | 0x10 | 0x40;                  /* d-pad up, Start, left stick click */
    r[3] = 0x10 | 0x80 | 0x01 | 0x04;           /* A, Y, LB, Guide */
    r[4] = 200; r[5] = 30;                      /* LT, RT */
    r[6] = 0xFF; r[7] = 0x7F;                   /* LX max */
    r[8] = 0xFF; r[9] = 0x7F;                   /* LY max: up, which is -y here */
    r[10] = 0x00; r[11] = 0x80;                 /* RX min */
    r[12] = 0x00; r[13] = 0x80;                 /* RY min: down */
    gamepad_report(p, r, sizeof(r));
    gamepad_state_t s = state(p);
    CHECK(s.lx == 32767 && s.ly == -32767 && s.rx == -32768 && s.ry == 32767);
    CHECK(s.lt == 200 && s.rt == 30);
    CHECK(s.dpad == GAMEPAD_DPAD_UP);
    /* A=1 B=2 X=3 Y=4 LB=5 RB=6 LT=7 RT=8 Back=9 Start=10 LS=11 RS=12 Guide=13 */
    CHECK(s.buttons == (1u | (1u << 3) | (1u << 4) | (1u << 6) | (1u << 9) |
                        (1u << 10) | (1u << 12)));

    r[0] = 0x08;                                /* a receiver's status message, not input */
    r[2] = 0; r[3] = 0;
    gamepad_report(p, r, sizeof(r));
    CHECK(state(p).buttons != 0);               /* ignored */
    r[0] = 0x00;
    gamepad_report(p, r, 6);                    /* too short */
    CHECK(state(p).buttons != 0);
    gamepad_report(p, r, sizeof(r));
    CHECK(state(p).dpad == 0 && state(p).buttons == (1u << 6));  /* LT still 200 */
    detach_all();
}

static unsigned rnd(unsigned *seed) {
    *seed = *seed * 1103515245u + 12345u;
    return (*seed >> 16) & 0x7FFF;
}

/* Random descriptors and random reports. Anything may come back; what must not
 * happen is a read or write outside a buffer, which AddressSanitizer reports. */
static void test_fuzz(void) {
    SECTION("fuzz: random descriptors and reports");
    static unsigned seed = 12345;
    #define RND() rnd(&seed)
    int attached = 0;
    for(int iter = 0; iter < 60000; iter++) {
        desc_t d = {0};
        /* Start from something shaped like a pad, so the parser gets past the
         * first collection often enough to meet the interesting items. */
        if(iter % 3 == 0)      generic_pad(&d);
        else if(iter % 3 == 1) ds4_pad(&d);
        else                   xbox_hid_pad(&d);
        int flips = 1 + (int) (RND() % 8);
        for(int f = 0; f < flips; f++)
        {
            unsigned at = RND() % (unsigned) d.n;
            d.b[at] = (uint8_t) RND();
        }
        if(RND() % 4 == 0)
            d.n = (int) (RND() % (unsigned) (d.n + 1));      /* cut anywhere */
        if(RND() % 5 == 0) {                                  /* or pure noise */
            d.n = (int) (RND() % 300);
            for(int i = 0; i < d.n; i++) d.b[i] = (uint8_t) RND();
        }
        int p = gamepad_attach(0xAAAA, 0xBBBB, d.b, d.n);
        if(p < 0)
            continue;
        attached++;
        for(int k = 0; k < 6; k++) {
            uint8_t r[80];
            int len = (int) (RND() % 80);
            for(int i = 0; i < len; i++) r[i] = (uint8_t) RND();
            gamepad_report(p, r, len);
            gamepad_state_t s;
            gamepad_get(p, &s);
        }
        char buf[200];
        gamepad_describe(p, buf, sizeof(buf));
        gamepad_detach(p);
    }
    CHECK(attached > 1000);                     /* the fuzzing did reach the report path */
    printf("  %d random descriptors parsed as pads\n", attached);
    detach_all();
}

int main(void) {
    test_generic();
    test_ds4();
    test_xbox_hid();
    test_xbox_dinput();
    test_signed_and_fourway();
    test_dpad_buttons();
    test_hat_from_one();
    test_wide();
    test_not_a_pad();
    test_slots();
    test_xinput();
    test_fuzz();

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
