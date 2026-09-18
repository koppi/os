/**
 * @file pic.h
 * @brief 8259A Programmable Interrupt Controller register map and helpers.
 *
 * The bulk of this header is named constants for the ICW/OCW command words and
 * the standard IRQ line assignments; see the 8259A datasheet / the OSDev wiki.
 */
#pragma once

#include <types.h>

/** @name Master PIC IRQ lines */
///@{
#define PIC_IRQ_TIMER           0
#define PIC_IRQ_KEYBOARD        1
#define PIC_IRQ_SERIAL2         3
#define	PIC_IRQ_SERIAL1         4
#define	PIC_IRQ_PARALLEL2       5
#define	PIC_IRQ_DISKETTE        6
#define	PIC_IRQ_PARALLEL1       7
///@}

/** @name Slave PIC IRQ lines (relative to the slave) */
///@{
#define	PIC_IRQ_CMOSTIMER       0
#define	PIC_IRQ_CGARETRACE      1
#define	PIC_IRQ_AUXILIARY       4
#define	PIC_IRQ_FPU             5
#define	PIC_IRQ_HDC             6
///@}

/** @name OCW2 bits */
///@{
#define PIC_L0_MASK             1
#define PIC_L1_MASK             2
#define PIC_L2_MASK             4
#define PIC_EOI_MASK            0x20
#define PIC_SELECTION_MASK      0x40
#define PIC_ROTATE_MASK         0x80
///@}

/** @name OCW3 bits */
///@{
#define PIC_RIS_MASK             1
#define PIC_RIR_MASK             2
#define PIC_MODE_MASK            4
#define PIC_SMM_MASK             0x20
#define PIC_ESMM_MASK            0x40
#define PIC_D7_MASK              0x80
///@}

/** @name Master PIC ports */
///@{
#define PIC1_REG_COMMAND        0x20
#define PIC1_REG_STATUS         0x20
#define PIC1_REG_DATA           0x21
#define PIC1_REG_IMR            0x21
///@}

/** @name Slave PIC ports */
///@{
#define PIC2_REG_COMMAND        0xA0
#define PIC2_REG_STATUS         0xA0
#define PIC2_REG_DATA           0xA1
#define PIC2_REG_IMR            0xA1
///@}

/** @name ICW1 fields and command bits */
///@{
#define PIC_IC4_MASK            0x1
#define PIC_SNGL_MASK           0x2
#define PIC_ADI_MASK            0x4
#define PIC_LTIM_MASK           0x8
#define PIC_INIT_MASK           0x10

#define PIC_IC4_EXPECT          0x1
#define PIC_IC4_NO              0x0
#define PIC_SNGL_YES            0x2
#define PIC_SNGL_NO             0x0
#define PIC_ADI_CALLINTERVAL4   0x4
#define PIC_ADI_CALLINTERVAL8   0x0
#define PIC_LTIM_LEVELTRIGGERED 0x8
#define PIC_LTIM_EDGETRIGGERED  0x0
#define PIC_INIT_YES            0x10
#define PIC_INIT_NO             0x0
///@}

/** @name ICW4 fields and command bits */
///@{
#define PIC_UPM_MASK            0x1
#define PIC_AEOI_MASK           0x2
#define PIC_MS_MASK             0x4
#define PIC_BUF_MASK            0x8
#define PIC_SFNM_MASK           0x10

#define PIC_UPM_86MODE          0x1
#define PIC_UPM_MCSMODE         0x0
#define PIC_AEOI_AUTO_YES       0x2
#define PIC_AEOI_AUTO_NO        0x0
#define PIC_MS_BUFFERMASTER     0x4
#define PIC_MS_BUFFERSLAVE      0x0
#define PIC_BUF_MODE_YES        0x8
#define PIC_BUF_MODE_NO         0x0
#define PIC_SFNM_NESTEDMODE_YES 0x10
#define PIC_SFNM_NESTEDMODE_NO  0x0
///@}

/**
 * @brief Write a command word to a PIC.
 * @param cmd Command byte.
 * @param pic 0 for master, 1 for slave.
 */
void pic_send_command(uint8_t cmd, uint8_t pic);

/**
 * @brief Write a data byte to a PIC.
 * @param data Data byte.
 * @param pic  0 for master, 1 for slave.
 */
void pic_send_data(uint8_t data, uint8_t pic);

/**
 * @brief Read a PIC data register.
 * @param pic 0 for master, 1 for slave.
 * @return The byte read (0 for an invalid @p pic).
 */
uint8_t pic_read_data(uint8_t pic);

/**
 * @brief Remap both PICs so their IRQs land on fresh vectors clear of the CPU
 *        exceptions, and put them in 8086 mode.
 * @param base0 Base vector for master IRQ0-7.
 * @param base1 Base vector for slave IRQ8-15.
 */
void pic_init(uint8_t base0, uint8_t base1);
