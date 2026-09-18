/**
 * @file pic.c
 * @brief 8259A PIC remap and command/data register access.
 */
#include <pic.h>
#include <io.h>

/** @brief Send a command byte to the master (@p pic 0) or slave (1) PIC. */
void pic_send_command(uint8_t cmd, uint8_t pic) {
    if(pic == 0)
        outportb(PIC1_REG_COMMAND, cmd);
    else if(pic == 1)
        outportb(PIC2_REG_COMMAND, cmd);
}

/** @brief Send a data byte to the master (@p pic 0) or slave (1) PIC. */
void pic_send_data(uint8_t data, uint8_t pic) {
    if(pic == 0)
        outportb(PIC1_REG_DATA, data);
    else if(pic == 1)
        outportb(PIC2_REG_DATA, data);
}

/** @brief Read the master (@p pic 0) or slave (1) PIC data register (0 if bad). */
uint8_t pic_read_data(uint8_t pic) {
    if(pic == 0)
        return inportb(PIC1_REG_DATA);
    else if(pic == 1)
        return inportb(PIC2_REG_DATA);
    else
        return 0;
}

/**
 * @brief Reinitialise both PICs with new vector offsets (see @ref pic.h).
 *
 * Runs the ICW1-ICW4 sequence with interrupts disabled, wires the master/slave
 * cascade on IRQ2, selects 8086 mode, and restores the previous interrupt
 * masks.
 */
void pic_init(uint8_t base0, uint8_t base1) {

    //Get current masks.
    uint8_t mask1 = pic_read_data(0);
    uint8_t mask2 = pic_read_data(1);

    uint8_t icw = 0;

    disable_int();

    icw = (icw & ~PIC_INIT_MASK) | PIC_INIT_YES;
    icw = (icw & ~PIC_IC4_MASK)  | PIC_IC4_EXPECT;

    // ICW1 Begin initilaization of PICs.
    pic_send_command(icw, 0);
    pic_send_command(icw, 1);

    // ICW2 Set new offsets
    pic_send_data(base0, 0);
    pic_send_data(base1, 1);

    // ICW3 Enable cascading.
    pic_send_data(0x04, 0);
    pic_send_data(0x02, 1);

    icw = (icw & ~PIC_UPM_MASK) | PIC_UPM_86MODE;

    // ICW4 Place PICs in 8086 mode.
    pic_send_data(icw, 0);
    pic_send_data(icw, 1);

    // Set original masks.
    pic_send_data(mask1, 0);
    pic_send_data(mask2, 1);
}
