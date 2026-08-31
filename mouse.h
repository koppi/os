/**
 * @file mouse.h
 * @brief PS/2 mouse driver: 3-byte packet decode, position tracking and
 *        button edge helpers used by the UI layer.
 */
#pragma once

#include <types.h>

/** @name PS/2 controller ports and status bits */
///@{
#define MOUSE_PORT   0x60
#define MOUSE_STATUS 0x64
#define MOUSE_ABIT   0x02  /**< Input buffer full. */
#define MOUSE_BBIT   0x01  /**< Output buffer full. */
#define MOUSE_WRITE  0xD4  /**< "next byte goes to the mouse" command. */
#define MOUSE_F_BIT  0x20  /**< Status: data is from the mouse. */
#define MOUSE_V_BIT  0x08  /**< Packet byte 0: always-1 sync bit. */
///@}

#define LEFT_CLICK      0x1
#define RIGHT_CLICK     0x2
#define MIDDLE_CLICK    0x4

#define MOUSE_LEFT_BUTTON(flag)   (flag & 0x1) /**< Left button held in @p flag. */
#define MOUSE_RIGHT_BUTTON(flag)  (flag & 0x2) /**< Right button held in @p flag. */
#define MOUSE_MIDDLE_BUTTON(flag) (flag & 0x4) /**< Middle button held in @p flag. */

/** Current pointer position and this/previous button masks. */
typedef struct mouse_info {
    int x;                          /**< Pointer X, clamped to the screen. */
    int y;                          /**< Pointer Y, clamped to the screen. */
    volatile uint32_t prev_button;  /**< Button mask from the previous packet. */
    volatile uint32_t curr_button;  /**< Button mask from the latest packet. */
} mouse_info_t;

/** @brief Poll the PS/2 status register for the given direction bit. */
void mouse_wait(uint8_t type);
/** @brief Send one byte to the mouse. */
void mouse_write(uint8_t write);
/** @brief Read one byte from the PS/2 data port. */
uint8_t mouse_read();
/** @return Pointer to the shared @ref mouse_info. */
mouse_info_t *get_mouse_info();
/** @brief IRQ handler: assemble a packet and update position/buttons. */
void mouse_handler();
/** @brief Clamp @ref mouse_info x/y to the screen bounds. */
void mouse_check_bounds();
/** @brief Enable the PS/2 mouse and install its IRQ handler. */
void mouse_init();

int mouse_left_button_down();   /**< @return true on a left-button press edge. */
int mouse_left_button_up();     /**< @return true on a left-button release edge. */
int mouse_right_button_down();  /**< @return true on a right-button press edge. */
int mouse_right_button_up();    /**< @return true on a right-button release edge. */

extern mouse_info_t mouse_info; /**< The one pointer-state instance. */

