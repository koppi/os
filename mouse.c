#include <mouse.h>
#include <io.h>
#include <idt.h>

mouse_info_t mouse_info;

uint8_t mouse_cycle = 0;
char mouse_byte[3];

extern void mouse_int();

void mouse_wait(uint8_t type) {
    uint32_t timeout = 1000; // was 100000
    uint32_t expect = (type == 1) ? 1 : 0;
    while(timeout--) {
        if((inportb(MOUSE_STATUS) & type) == expect)
            return;
    }
    return;
}

void mouse_write(uint8_t write) {
    mouse_wait(1);
    outportb(MOUSE_STATUS, MOUSE_WRITE);
    mouse_wait(1);
    outportb(MOUSE_PORT, write);
}

uint8_t mouse_read() {
    mouse_wait(0);
    return inportb(MOUSE_PORT);
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
    uint8_t status = inportb(MOUSE_STATUS);
    while(status & MOUSE_BBIT) {
        char mouse_in = inportb(MOUSE_PORT);
        if(status & MOUSE_F_BIT) {
            switch(mouse_cycle) {
                case 0:
                    mouse_byte[0] = mouse_in;
                    if (MOUSE_LEFT_BUTTON(mouse_byte[0])) {
                        mouse_info.curr_button = LEFT_CLICK;
                    } else {
                        mouse_info.curr_button = 0;
                    }
                    if (MOUSE_RIGHT_BUTTON(mouse_byte[0])) {
                        mouse_info.curr_button = RIGHT_CLICK;
                    } else {
                        mouse_info.curr_button = 0;
                    }
                    if(!(mouse_in & MOUSE_V_BIT))
                        goto read_next;
                    mouse_cycle++;
                    break;
                case 1:
                    mouse_byte[1] = mouse_in;
                    mouse_cycle++;
                    break;
                case 2:
                    mouse_byte[2] = mouse_in;
                    if(mouse_byte[0] & 0x80 || mouse_byte[0] & 0x40)
                        goto read_next;
                    mouse_info.x += mouse_byte[1];
                    mouse_info.y -= mouse_byte[2];
                    mouse_check_bounds();
                    
                    if(mouse_byte[0] & LEFT_CLICK)
                        mouse_info.curr_button = LEFT_CLICK;
                    else if(mouse_byte[0] & RIGHT_CLICK)
                        mouse_info.curr_button = RIGHT_CLICK;
                    else if(mouse_byte[0] & MIDDLE_CLICK)
                        mouse_info.curr_button = MIDDLE_CLICK;
                    else
                        mouse_info.curr_button = 0;
                    mouse_cycle = 0;

                    //printf("mouse_handler x %d y %d\n", mouse_info.x, mouse_info.y);
                    break;
            }
        }

read_next:
        status = inportb(MOUSE_STATUS);
    }
}

void mouse_check_bounds() {
    if(mouse_info.x > 1280-1)
        mouse_info.x = 1280-1;
    else if(mouse_info.x < 0)
        mouse_info.x = 0;
    if(mouse_info.y > 1024-1)
        mouse_info.y = 1024-1;
    else if(mouse_info.y < 0)
        mouse_info.y = 0;
}

void mouse_init() {
    mouse_info.x = 0;
    mouse_info.y = 0;
    uint8_t status;
    mouse_wait(1);
    outportb(MOUSE_STATUS, 0xA8);
    mouse_wait(1);
    outportb(MOUSE_STATUS, 0x20);
    mouse_wait(2);
    status = inportb(0x60) | 2;
    mouse_wait(1);
    outportb(MOUSE_STATUS, 0x60);
    mouse_wait(1);
    outportb(MOUSE_PORT, status);
    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();
    install_ir(44, 0x80 | 0x0E, 0x8, &mouse_int);
}
