/**
 * @file rtc.h
 * @brief MC146818 real-time clock / CMOS access (date & time in BCD or binary).
 */
#pragma once

#include <types.h>

#define CMOS_ADDR 0x70  /**< CMOS index port. */
#define CMOS_DATA 0x71  /**< CMOS data port. */

/** A wall-clock date/time (fields already decoded to binary). */
typedef struct datetime {
    uint8_t century;
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} datetime_t;

/** @return Non-zero while the RTC is mid-update (registers unstable). */
int is_updating_rtc();

/** @brief Read CMOS register @p reg_num. */
uint8_t get_rtc_register(int reg_num);

/** @brief Write @p val to CMOS register @p reg_num. */
void set_rtc_register(uint16_t reg_num, uint8_t val);

/** @brief Read the current date/time into the module's static @ref datetime_t. */
void rtc_read_datetime();

/** @brief Write @p dt back to the CMOS clock. */
void rtc_write_datetime(datetime_t * dt);

/** @brief Format @p dt as a static "YYYY-MM-DD HH:MM:SS" string. */
char * datetime_to_str(datetime_t * dt);

/** @brief Read the clock and return it formatted (see @ref datetime_to_str). */
char * get_current_datetime_str();

/** @return Day of week (0-6) for the date in @p dt (Zeller's congruence). */
int get_weekday_from_date(datetime_t * dt);

/** @return Non-zero if @p year is a leap year. */
int is_leap_year(int year, int month);

/**
 * @brief Read the clock and return it as seconds since the Unix epoch.
 *
 * The CMOS registers are read as UTC (QEMU seeds them from the host with
 * @c -rtc base=localtime, but @ref ntp_sync overwrites them with UTC).
 */
uint32_t rtc_now_unix(void);

/** @brief Set the CMOS clock (and the cached datetime) from Unix time @p secs (UTC). */
void rtc_set_unix(uint32_t secs);

/**
 * @brief Format Unix time @p secs as "Ddd YYYY-MM-DD HH:MM:SS UTC" into @p buf.
 * @return @p buf.
 */
char *unix_to_str(uint32_t secs, char *buf, size_t n);

/** @brief Enable the periodic RTC interrupt. */
void rtc_init();
