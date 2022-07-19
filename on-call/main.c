#include <assert.h>
#include <stdio.h>
#include <time.h>

#define GB_IMPLEMENTATION
#define GB_STATIC
#include "../vendor/gb/gb.h"

static const u16 hourly_week_end_rate = 15;
static const u16 hourly_week_rate = 10;

#define DATE_TIME(year, month, day, hour)                            \
    (struct tm) {                                                    \
        .tm_year = year - 1900, .tm_mon = month - 1, .tm_mday = day, \
        .tm_hour = hour,                                             \
    }

typedef struct {
    struct tm start, end;
} datetime_range_t;

typedef struct {
    int month, year;  // In ISO range
    u16 week_money, week_hours, week_end_money, week_end_hours, total_hours,
        total_money;
} bill_summary_t;

static bool datetime_is_week_end(const struct tm* d) {
    return d->tm_wday == /* Sunday */ 0 || d->tm_wday == /* Saturday */ 6;
}

static bool datetime_is_working_hour(const struct tm* d) {
    // Work hours in local time: 9AM-6PM
    return !datetime_is_week_end(d) && 9 <= d->tm_hour && d->tm_hour < 18;
}

static void shift_bill_hour(bill_summary_t* summary, time_t timestamp) {
    struct tm* hour = localtime(&timestamp);
    if (datetime_is_working_hour(hour)) return;

    summary->total_hours += 1;
    if (datetime_is_week_end(hour)) {
        summary->week_end_hours += 1;
        summary->week_end_money += hourly_week_end_rate;
        summary->total_money += hourly_week_end_rate;
    } else {
        summary->week_hours += 1;
        summary->week_money += hourly_week_rate;
        summary->total_money += hourly_week_rate;
    }
}

static time_t get_start_of_next_month(const struct tm* d) {
    struct tm tmp = *d;
    tmp.tm_mday = 1;
    tmp.tm_mon = d->tm_mon + 1;
    tmp.tm_hour = 0;
    return timelocal(&tmp);
}

static void shift_bill_monthly(gbArray(bill_summary_t) summaries,
                               datetime_range_t* shift) {
    bill_summary_t* summary = NULL;
    // Upsert
    {
        const u64 summaries_len = gb_array_count(summaries);
        if (summaries_len > 0 &&
            (summaries[summaries_len - 1].month - 1) == shift->start.tm_mon &&
            (summaries[summaries_len - 1].year - 1900) ==
                shift->start.tm_year) {
            summary = &summaries[summaries_len - 1];
        } else {
            gb_array_append(
                summaries,
                ((bill_summary_t){.month = shift->start.tm_mon + 1,
                                  .year = shift->start.tm_year + 1900}));
            summary = &summaries[gb_array_count(summaries) - 1];
        }
    }

    time_t timestamp = timelocal(&shift->start);
    const time_t end_timestamp = timelocal(&shift->end);
    assert(timestamp < end_timestamp);
    const time_t start_of_next_month = get_start_of_next_month(&shift->start);

    // Bill from the start of the shift to the end of the month
    while (timestamp < MIN(end_timestamp, start_of_next_month)) {
        shift_bill_hour(summary, timestamp);
        timestamp += 3600;
    }
    if (shift->start.tm_mon == shift->end.tm_mon) return;

    // Handle the case of a shift spanning 2 months so it appears in 2 monthly
    // bills
    assert(shift->start.tm_mon + 1 == shift->end.tm_mon);
    gb_array_append(summaries,
                    ((bill_summary_t){.month = shift->end.tm_mon + 1,
                                      .year = shift->end.tm_year + 1900}));
    summary = &summaries[gb_array_count(summaries) - 1];

    // Bill from the first of the month to the end of the shift
    while (timestamp < end_timestamp) {
        shift_bill_hour(summary, timestamp);
        timestamp += 3600;
    }
}

static void bill(datetime_range_t* work, u64 work_len) {
    assert(work != NULL);
    assert(work_len > 0);

    gbArray(bill_summary_t) summaries = NULL;
    gb_array_init_reserve(summaries, gb_heap_allocator(), 100);

    for (int i = 0; i < work_len; i++) {
        shift_bill_monthly(summaries, &work[i]);
    }

    // TODO: better formatting
    for (int i = 0; i < gb_array_count(summaries); i++) {
        __builtin_dump_struct(&summaries[i], &printf);
    }
}

int main() {
    datetime_range_t work[] = {
        {DATE_TIME(2021, 9, 18, 9), DATE_TIME(2021, 9, 24, 9)},
        {DATE_TIME(2021, 10, 1, 18), DATE_TIME(2021, 10, 7, 9)},
        {DATE_TIME(2021, 11, 21, 9), DATE_TIME(2021, 11, 26, 9)},
        {DATE_TIME(2021, 12, 20, 18), DATE_TIME(2021, 12, 26, 9)},
        {DATE_TIME(2022, 2, 26, 18), DATE_TIME(2022, 3, 7, 9)},
        {DATE_TIME(2022, 3, 25, 18), DATE_TIME(2022, 3, 28, 9)},
        {DATE_TIME(2022, 4, 1, 18), DATE_TIME(2022, 4, 6, 18)},
        {DATE_TIME(2022, 4, 7, 18), DATE_TIME(2022, 4, 11, 9)},
        {DATE_TIME(2022, 5, 2, 18), DATE_TIME(2022, 5, 9, 9)},
        {DATE_TIME(2022, 6, 11, 9), DATE_TIME(2022, 6, 17, 9)},
        {DATE_TIME(2022, 7, 3, 9), DATE_TIME(2022, 7, 4, 9)},
        {DATE_TIME(2022, 7, 6, 18), DATE_TIME(2022, 7, 9, 0)},
        {DATE_TIME(2022, 7, 10, 0), DATE_TIME(2022, 7, 11, 9)},
        {DATE_TIME(2022, 7, 20, 18), DATE_TIME(2022, 7, 22, 9)},
        {DATE_TIME(2022, 7, 22, 20), DATE_TIME(2022, 7, 23, 9)},
    };
    bill(work, sizeof(work) / sizeof(work[0]));
}
