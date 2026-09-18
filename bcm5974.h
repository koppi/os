/**
 * @file bcm5974.h
 * @brief Apple BCM5974 (Wellspring) multi-touch trackpad driver interface.
 */
#pragma once

#include <types.h>

/** Shortest packet that can hold a TYPE3 report: the 38-byte header plus one
 *  28-byte finger block. Anything shorter arrived in some other format — a
 *  boot-protocol mouse report, typically — and is not a trackpad packet. */
#define BCM5974_MIN_REPORT 66

/** @brief Initialize BCM5974 trackpad parsing state. */
void bcm5974_init(void);

/** @brief Parse a BCM5974 TYPE3 trackpad report and update mouse cursor.
 *  @param data Pointer to the trackpad data (binary format, not HID).
 *  @param len  Length of the data in bytes. */
void bcm5974_parse_report(const uint8_t *data, int len);