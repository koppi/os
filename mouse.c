/**
 * @file mouse.c
 * @brief PS/2 mouse driver: 3-byte packet assembly in the IRQ handler,
 *        position/button tracking and press/release edge helpers.
 */
#include <mouse.h>
#include <io.h>
#include <idt.h>
#include <video.h>

mouse_info_t mouse_info;

uint8_t mouse_cycle = 0;
char mouse_byte[3];

extern void mouse_int();

/* i8042 status bits: 0x01 = output buffer full (data to read),
 *                    0x02 = input buffer full (controller busy, don't write). */
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

/* Kept for the header/ABI; `type` 1 = wait-readable, anything else = wait-writable. */
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

/** @brief Send @p cmd to the aux device and return its reply (0xFA = ACK).
 *         Retries once on 0xFE (resend). */
static uint8_t aux_cmd(uint8_t cmd) {
    for (int try = 0; try < 3; try++) {
        mouse_write(cmd);
        uint8_t r = mouse_read();
        if (r != 0xFE)
            return r;
    }
    return 0;
}

/** @brief Drain any bytes the aux device left in the output buffer. */
static void aux_drain(void) {
    for (int i = 0; i < 16 && (inportb(MOUSE_STATUS) & 0x01); i++)
        (void) inportb(MOUSE_PORT);
}

mouse_info_t *get_mouse_info() {
    return &mouse_info;
}

int mouse_left_button_down() {
    return (mouse_info.prev_button != LEFT_CLICK) && (mouse_info.curr_button == LEFT_CLICK);
}

int mouse_right_button_down() {
    return (mouse_info.prev_button != RIGHT_CLICK) && (mouse_info.curr_button == RIGHT_CLICK);
}

int mouse_left_button_up() {
    return (mouse_info.prev_button == LEFT_CLICK) && (mouse_info.curr_button != LEFT_CLICK);
}

int mouse_right_button_up() {
    return (mouse_info.prev_button == RIGHT_CLICK) && (mouse_info.curr_button != RIGHT_CLICK);
}

void mouse_handler() {
    uint8_t status;
    while((status = inportb(MOUSE_STATUS)) & MOUSE_BBIT) {
        uint8_t b = inportb(MOUSE_PORT);
        if(!(status & MOUSE_F_BIT))
            continue;               /* byte came from the keyboard, not the mouse */

        switch(mouse_cycle) {
            case 0:
                /* Byte 0 always has bit 3 set. If it isn't, the stream is out
                 * of phase (leftover Synaptics bytes, a lost IRQ) -- drop the
                 * byte and stay at cycle 0 until sync comes back. */
                if(!(b & MOUSE_V_BIT))
                    break;
                mouse_byte[0] = b;
                mouse_cycle = 1;
                break;
            case 1:
                mouse_byte[1] = b;
                mouse_cycle = 2;
                break;
            case 2:
                mouse_byte[2] = b;
                mouse_cycle = 0;
                if(mouse_byte[0] & 0xC0)          /* X / Y overflow: ignore */
                    break;
                mouse_info.x += (signed char) mouse_byte[1];
                mouse_info.y -= (signed char) mouse_byte[2];  /* PS/2 +Y is up */
                mouse_check_bounds();

                mouse_info.curr_button =
                    ((mouse_byte[0] & 1) ? LEFT_CLICK   : 0) |
                    ((mouse_byte[0] & 2) ? RIGHT_CLICK  : 0) |
                    ((mouse_byte[0] & 4) ? MIDDLE_CLICK : 0);
                break;
        }
    }
}

void mouse_check_bounds() {
    int w = vbemem.xres ? vbemem.xres : 1280;
    int h = vbemem.yres ? vbemem.yres : 1024;
    if(mouse_info.x > w - 1)
        mouse_info.x = w - 1;
    else if(mouse_info.x < 0)
        mouse_info.x = 0;
    if(mouse_info.y > h - 1)
        mouse_info.y = h - 1;
    else if(mouse_info.y < 0)
        mouse_info.y = 0;
}

void mouse_init() {
    mouse_info.x = 0;
    mouse_info.y = 0;
    mouse_cycle = 0;

    /* Enable the aux (mouse) port and set the controller config byte:
     * bit 1 = IRQ12 enable, bit 5 = disable aux clock (clear it). */
    i8042_wait_write(); outportb(MOUSE_STATUS, 0xA8);
    i8042_wait_write(); outportb(MOUSE_STATUS, 0x20);
    uint8_t cfg = mouse_read();
    cfg |= (1u << 1);
    cfg &= ~(1u << 5);
    i8042_wait_write(); outportb(MOUSE_STATUS, 0x60);
    i8042_wait_write(); outportb(MOUSE_PORT, cfg);

    /* Reset. A ThinkPad ClickPad powers up in the Synaptics absolute protocol
     * (6-byte packets); 0xFF returns it -- and the TrackPoint -- to the
     * standard 3-byte PS/2 relative mode this driver decodes. Reply is
     * ACK (FA) then BAT-ok (AA) then device id (00). */
    aux_cmd(0xFF);
    aux_drain();

    aux_cmd(0xF6);              /* load defaults                     */
    aux_cmd(0xF3); aux_cmd(100);/* sample rate 100 Hz                */
    aux_cmd(0xE8); aux_cmd(0x02);/* resolution 4 counts/mm           */
    aux_cmd(0xF4);              /* enable data reporting             */
    aux_drain();

    install_ir(44, 0x80 | 0x0E, 0x8, &mouse_int);
}
