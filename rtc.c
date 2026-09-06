/**
 * @file rtc.c
 * @brief MC146818 RTC / CMOS access: read & write the wall-clock date/time
 *        (handling BCD vs binary and 12/24-hour mode) and derive the weekday.
 */
#include <rtc.h>
#include <lib/string.h>
#include <kheap.h>
#include <io.h>
#include <printf.h>
#include <spinlock.h>

// Global var, store current date and time
datetime_t current_datetime;

char * weekday_map[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
/*
 * Check if rtc is updating time currently
 * */
int is_updating_rtc() {
    outportb(CMOS_ADDR, 0x0A);
    uint32_t status = inportb(CMOS_DATA);
    return (status & 0x80);
}

/** @brief Spin (bounded) until the RTC is not mid-update. The UIP bit clears
 *         within ~2 ms on real hardware; the bound stops a wedged/absent RTC
 *         from hanging the caller (and the boot). */
static void rtc_wait_ready(void) {
    for (int i = 0; i < 1000000 && is_updating_rtc(); i++)
        __asm__ volatile("pause");
}

/*
 * Get the value of a specific rtc register
 * */
uint8_t get_rtc_register(int reg_num) {
    outportb(CMOS_ADDR, reg_num);
    return inportb(CMOS_DATA);
}

/*
 * Set the value of a specific rtc register
 * */
void set_rtc_register(uint16_t reg_num, uint8_t val) {
    outportb(CMOS_ADDR, reg_num);
    outportb(CMOS_DATA, val);
}

/*
 * Read current date and time from rtc, store in global var current_datetime
 * */
void rtc_read_datetime() {
    /* paint_desktop() reads the clock every frame while `date` / ntp may read
     * it from another CPU: serialise the index+data port pair. */
    uint32_t f = spin_lock(&cmos_lock);
    rtc_wait_ready();

    current_datetime.second = get_rtc_register(0x00);
    current_datetime.minute = get_rtc_register(0x02);
    current_datetime.hour = get_rtc_register(0x04);
    current_datetime.day = get_rtc_register(0x07);
    current_datetime.month = get_rtc_register(0x08);
    current_datetime.year = get_rtc_register(0x09);

    uint8_t registerB = get_rtc_register(0x0B);

    // Convert BCD to binary values if necessary
    if (!(registerB & 0x04)) {
        current_datetime.second = (current_datetime.second & 0x0F) + ((current_datetime.second / 16) * 10);
        current_datetime.minute = (current_datetime.minute & 0x0F) + ((current_datetime.minute / 16) * 10);
        current_datetime.hour = ( (current_datetime.hour & 0x0F) + (((current_datetime.hour & 0x70) / 16) * 10) ) | (current_datetime.hour & 0x80);
        current_datetime.day = (current_datetime.day & 0x0F) + ((current_datetime.day / 16) * 10);
        current_datetime.month = (current_datetime.month & 0x0F) + ((current_datetime.month / 16) * 10);
        current_datetime.year = (current_datetime.year & 0x0F) + ((current_datetime.year / 16) * 10);
    }
    spin_unlock(&cmos_lock, f);
}

/*
 * Write a datetime struct to rtc
 *
 * Encodes each field to match the clock's current data mode (register B bit 2:
 * clear = BCD, the QEMU default), so this is the exact inverse of
 * rtc_read_datetime(). 24-hour mode is assumed.
 * */
static uint8_t to_bcd(uint8_t v) {
    return (uint8_t)(((v / 10) << 4) | (v % 10));
}

void rtc_write_datetime(datetime_t * dt) {
    uint32_t f = spin_lock(&cmos_lock);
    int bcd = !(get_rtc_register(0x0B) & 0x04);

    uint8_t sec = dt->second, min = dt->minute, hr = dt->hour;
    uint8_t day = dt->day, mon = dt->month, yr = dt->year;
    if (bcd) {
        sec = to_bcd(sec); min = to_bcd(min); hr  = to_bcd(hr);
        day = to_bcd(day); mon = to_bcd(mon); yr  = to_bcd(yr);
    }

    rtc_wait_ready();

    set_rtc_register(0x00, sec);
    set_rtc_register(0x02, min);
    set_rtc_register(0x04, hr);
    set_rtc_register(0x07, day);
    set_rtc_register(0x08, mon);
    set_rtc_register(0x09, yr);
    spin_unlock(&cmos_lock, f);
}

/* ------------------------------------------------------------------ *
 *  Unix time <-> calendar (Howard Hinnant's days-from-civil)          *
 * ------------------------------------------------------------------ */
/** @return Days from 1970-01-01 to @p y-@p m-@p d (@p m in 1..12). */
static long days_from_civil(int y, int m, int d) {
    y -= (m <= 2);
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;                                   /* [0, 399]    */
    int doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1; /* [0, 365]    */
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;           /* [0, 146096] */
    return (long)era * 146097 + doe - 719468;
}

/** @brief Split day number @p z (days since 1970-01-01) into @p yr / @p mo / @p dy. */
static void civil_from_days(long z, int *yr, int *mo, int *dy) {
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    int doe = (int)(z - era * 146097);                              /* [0, 146096] */
    int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;/* [0, 399]    */
    int y = yoe + (int)era * 400;
    int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);              /* [0, 365]    */
    int mp = (5 * doy + 2) / 153;                                   /* [0, 11]     */
    *dy = doy - (153 * mp + 2) / 5 + 1;                             /* [1, 31]     */
    *mo = mp < 10 ? mp + 3 : mp - 9;                                /* [1, 12]     */
    *yr = y + (*mo <= 2);
}

uint32_t rtc_now_unix(void) {
    rtc_read_datetime();
    datetime_t *dt = &current_datetime;
    long days = days_from_civil(2000 + dt->year, dt->month, dt->day);
    return (uint32_t)(days * 86400L +
                      dt->hour * 3600L + dt->minute * 60L + dt->second);
}

void rtc_set_unix(uint32_t secs) {
    long     days = (long)(secs / 86400u);
    uint32_t rem  = secs % 86400u;
    int y, m, d;
    civil_from_days(days, &y, &m, &d);

    datetime_t dt;
    dt.century = 21;
    dt.year    = (uint8_t)(y - 2000);
    dt.month   = (uint8_t)m;
    dt.day     = (uint8_t)d;
    dt.hour    = (uint8_t)(rem / 3600u);
    dt.minute  = (uint8_t)((rem / 60u) % 60u);
    dt.second  = (uint8_t)(rem % 60u);

    rtc_write_datetime(&dt);
    current_datetime = dt;
}

char *unix_to_str(uint32_t secs, char *buf, size_t n) {
    /* 1970-01-01 was a Thursday, so day 0 indexes "Thu". */
    static const char *dow[7] = { "Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed" };
    long     days = (long)(secs / 86400u);
    uint32_t rem  = secs % 86400u;
    int y, m, d;
    civil_from_days(days, &y, &m, &d);
    snprintf(buf, n, "%s %04d-%02d-%02d %02d:%02d:%02d UTC",
             dow[days % 7], y, m, d,
             (int)(rem / 3600u), (int)((rem / 60u) % 60u), (int)(rem % 60u));
    return buf;
}

/*
 * A convenient function that converts a datetime struct to string
 * Only support the format: "day hour:minute"
 * For example, "Sat 6:32"
 * */
char * datetime_to_str(datetime_t * dt) {
    char * ret = kmalloc(19);
    char * weekday = weekday_map[get_weekday_from_date(dt)];
    snprintf(ret, 19, "%s %02d:%02d:%02d PM", weekday, dt->hour, dt->minute, dt->second);
    return ret;
}

char * get_current_datetime_str() {
    return datetime_to_str(&current_datetime);
}

/*
 * Given a date, calculate it's weekday, using the algorithm described here: http://blog.artofmemory.com/how-to-calculate-the-day-of-the-week-4203.html
 * */
int get_weekday_from_date(datetime_t * dt) {
    char month_code_array[] = {0x0,0x3, 0x3, 0x6, 0x1, 0x4, 0x6, 0x2, 0x5, 0x0, 0x3, 0x5};
    char century_code_array[] = {0x4, 0x2, 0x0, 0x6, 0x4, 0x2, 0x0};    // Starting from 18 century

    // Simple fix...
    dt->century = 21;

    // Calculate year code
    int year_code = (dt->year + (dt->year / 4)) % 7;
    int month_code = month_code_array[dt->month - 1];
    int century_code = century_code_array[dt->century - 1 - 17];
    int leap_year_code = is_leap_year(dt->year, dt->month);

    int ret = (year_code + month_code + century_code + dt->day - leap_year_code) % 7;
    return ret;
}

int is_leap_year(int year, int month) {
    if(year % 4 == 0 && (month == 1 || month == 2)) return 1;
    return 0;
}

/*
 * Initialize RTC
 * */
void rtc_init() {
    /*
       current_datetime.century = 21;
       current_datetime.year = 16;
       current_datetime.month = 1;
       current_datetime.day = 1;
       current_datetime.hour = 0;
       current_datetime.minute = 0;
       current_datetime.second = 0;
       rtc_write_datetime(&current_datetime);
       */
    rtc_read_datetime();
}
