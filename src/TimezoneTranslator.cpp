/*
 Name:        TimezoneTranslator.cpp
 Created:     2/21/2026 2:21:53 PM
 Author:      Costin
 Editor:      http://www.visualmicro.com
*/
/*
Library for converting a time stamp between UTC and local
Takes into account the time zone and some basic DST rules
(C)2010-2026 Costin Bobes

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "TimezoneTranslator.h"

static const uint8_t MONTH_DAYS[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

// ---- Constructor ----

TimezoneTranslator::TimezoneTranslator() {
    // Default timezone: UTC, no DST
    _tz = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    _cache = { 0, 0, 0 };
}

// ---- Public API ----

bool TimezoneTranslator::setLocalTimezone(const TimezoneDefinition& tz) {
    // Basic validation: if DST is defined, months must be 1-12
    if (tz.dst_start_month > 12 || tz.dst_end_month > 12) {
        return false;
    }
    if (tz.dst_start_month != 0 && tz.dst_end_month == 0) {
        return false;
    }
    _tz = tz;
    _cache = { 0, 0, 0 }; // invalidate cache
    return true;
}

uint64_t TimezoneTranslator::utcToLocal(uint64_t utcMs, const TimezoneDefinition& tz) {
    if (tz.dst_start_month == 0) {
        return utcMs + (int64_t)tz.offset_min * 60000LL;
    }
    DstCache tempCache = { 0, 0, 0 };
    int16_t offsetMin = getOffsetForUtc(utcMs, tz, tempCache);
    return utcMs + (int64_t)offsetMin * 60000LL;
}

uint64_t TimezoneTranslator::utcToLocal(uint64_t utcMs) {
    int16_t offsetMin = getOffsetForUtc(utcMs, _tz, _cache);
    return utcMs + (int64_t)offsetMin * 60000LL;
}

uint64_t TimezoneTranslator::localToUtc(uint64_t localMs, const TimezoneDefinition& tz) {
    if (tz.dst_start_month == 0) {
        return localMs - (int64_t)tz.offset_min * 60000LL;
    }
    DstCache tempCache = { 0, 0, 0 };
    int16_t offsetMin = getOffsetForLocal(localMs, tz, tempCache);
    return localMs - (int64_t)offsetMin * 60000LL;
}

uint64_t TimezoneTranslator::localToUtc(uint64_t localMs) {
    int16_t offsetMin = getOffsetForLocal(localMs, _tz, _cache);
    return localMs - (int64_t)offsetMin * 60000LL;
}

uint64_t TimezoneTranslator::utcToLocal(uint32_t utcSec, const TimezoneDefinition& tz) {
    return utcToLocal(normalize32(utcSec), tz);
}

uint64_t TimezoneTranslator::utcToLocal(uint32_t utcSec) {
    return utcToLocal(normalize32(utcSec));
}

uint64_t TimezoneTranslator::localToUtc(uint32_t localSec, const TimezoneDefinition& tz) {
    return localToUtc(normalize32(localSec), tz);
}

uint64_t TimezoneTranslator::localToUtc(uint32_t localSec) {
    return localToUtc(normalize32(localSec));
}

// ---- 32-bit rollover normalization ----

uint64_t TimezoneTranslator::normalize32(uint32_t utcSec) {
    // If the 32-bit timestamp is before Jan 1, 2020 assume 32-bit rollover
    // happened and the real time is utcSec + 2^32 seconds into the future.
    if (utcSec < UNIX_OFFSET_2020) {
        return ((uint64_t)utcSec + 4294967296ULL) * 1000ULL;
    }
    return (uint64_t)utcSec * 1000ULL;
}

// ---- Utility helpers ----

int8_t TimezoneTranslator::isLeapYear(uint16_t year) {
    if (year % 400 == 0) return 1;
    if (year % 100 == 0) return 0;
    if (year % 4   == 0) return 1;
    return 0;
}

uint8_t TimezoneTranslator::getDaysInMonth(uint8_t month, uint16_t year) {
    if (month < 1 || month > 12) return 0;
    if (isLeapYear(year) && month == 2) return 29;
    return MONTH_DAYS[month - 1];
}

// Returns weekday for a UTC ms timestamp. 0=Sunday, 6=Saturday.
uint8_t TimezoneTranslator::getWeekday(uint64_t utcMs) {
    return getWeekdayFromDays((uint32_t)(utcMs / 86400000ULL));
}

// Returns weekday from days since epoch. 0=Sunday, 6=Saturday. Pure 32-bit.
uint8_t TimezoneTranslator::getWeekdayFromDays(uint32_t daysSinceEpoch) {
    // 1970-01-01 was Thursday (4)
    return (uint8_t)((daysSinceEpoch + 4) % 7);
}

// Convert Unix millisecond timestamp to broken-down time structure
void TimezoneTranslator::toTimeStruct(TimeStruct* dest, uint64_t utcMs) {
    if (!dest) return;

    // Single 64-bit division; derive all fields from days + remainder
    uint32_t daysSinceEpoch = (uint32_t)(utcMs / 86400000ULL);
    uint32_t remainderMs = (uint32_t)(utcMs - (uint64_t)daysSinceEpoch * 86400000ULL);

    dest->ms = (uint16_t)(remainderMs % 1000U);
    uint32_t remainderSec = remainderMs / 1000U;
    dest->second = (uint8_t)(remainderSec % 60U);
    dest->minute = (uint8_t)((remainderSec / 60U) % 60U);
    dest->hour = (uint8_t)(remainderSec / 3600U);

    // Calculate year from days (pure 32-bit binary search)
    uint16_t year = yearFromDays(daysSinceEpoch);
    uint32_t yearOffset = year - 1970;
    uint32_t leaps = (yearOffset + 1) / 4 - (yearOffset + 69) / 100 + (yearOffset + 369) / 400;
    uint32_t daysRemaining = daysSinceEpoch - (yearOffset * 365 + leaps);
    dest->year = year;

    // Calculate month and day
    uint8_t month = 1;
    uint32_t daysInM;
    while (daysRemaining >= MONTH_DAYS[month - 1]) {
        daysInM = MONTH_DAYS[month - 1];
        if (month == 2 && isLeapYear(year)) {
            daysInM = 29;
        }
        if (daysRemaining >= daysInM) {
            daysRemaining -= daysInM;
            month++;
        } else {
            break;
        }
    }
    dest->month = month;
    dest->day = (uint8_t)(daysRemaining + 1);

    // Calculate weekday (32-bit)
    dest->weekday = getWeekdayFromDays(daysSinceEpoch);
}

// ---- Internal: date <-> ms conversions ----

uint32_t TimezoneTranslator::dateToDays(uint16_t year, uint8_t month, uint8_t day) {
    // Fast leap-year arithmetic for days since 1970-01-01
    uint32_t years = year - 1970;
    uint32_t leaps = (years + 1) / 4 - (years + 69) / 100 + (years + 369) / 400;
    uint32_t days = years * 365 + leaps;

    for (uint8_t m = 1; m < month; m++) {
        days += MONTH_DAYS[m - 1];
        if (m == 2 && isLeapYear(year)) days += 1;
    }

    days += (day - 1);
    return days;
}

uint64_t TimezoneTranslator::dateToMs(uint16_t year, uint8_t month, uint8_t day,
                                       uint8_t hour, uint8_t minute, uint8_t second) {
    uint64_t ms = (uint64_t)dateToDays(year, month, day) * 86400000ULL;
    ms += (uint32_t)hour * 3600000UL;
    ms += (uint32_t)minute * 60000UL;
    ms += (uint32_t)second * 1000UL;
    return ms;
}

uint16_t TimezoneTranslator::yearFromMs(uint64_t utcMs) {
    return yearFromDays((uint32_t)(utcMs / 86400000ULL));
}

uint16_t TimezoneTranslator::yearFromDays(uint32_t days) {
    // Binary search: find first Y where daysToYear(Y) > days, return Y-1.
    // Pure 32-bit comparisons. Supports timestamps up to year 2500.
    uint16_t lo = 1971, hi = 2501;
    while (lo < hi) {
        uint16_t mid = lo + (hi - lo) / 2;
        uint32_t y   = mid - 1970;
        uint32_t leaps = (y + 1) / 4 - (y + 69) / 100 + (y + 369) / 400;
        if (y * 365UL + leaps > days) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    return lo - 1;
}

// ---- Internal: DST switch day calculation ----

uint8_t TimezoneTranslator::getDstSwitchDay(uint16_t year, uint8_t month,
                                             const TimezoneDefinition& tz, bool isStart) {
    int8_t targetWeek = isStart ? tz.dst_start_week : tz.dst_end_week;

    // Get weekday of the 1st of the month (pure 32-bit, no 64-bit division)
    uint8_t firstWeekday = getWeekdayFromDays(dateToDays(year, month, 1));

    if (targetWeek > 0) {
        // Find the first occurrence of dst_weekday
        uint8_t switchDay = 1;
        uint8_t wd = firstWeekday;
        while (wd != tz.dst_weekday) {
            switchDay++;
            wd = (wd + 1) % 7;
        }
        // Advance to the nth occurrence
        switchDay += 7 * (targetWeek - 1);
        return switchDay;
    }
    else {
        // Last occurrence: derive last-day weekday from first-day weekday + days in month
        uint8_t daysInM = getDaysInMonth(month, year);
        uint8_t lastWeekday = (firstWeekday + daysInM - 1) % 7;
        uint8_t switchDay = daysInM;
        while (lastWeekday != tz.dst_weekday) {
            switchDay--;
            lastWeekday = (lastWeekday == 0) ? 6 : lastWeekday - 1;
        }
        return switchDay;
    }
}

// ---- Internal: compute DST transitions for a year ----

void TimezoneTranslator::computeDstTransitions(uint16_t year,
                                                const TimezoneDefinition& tz,
                                                uint64_t& outStartMs,
                                                uint64_t& outEndMs) {
    outStartMs = computeDstStartMs(year, tz);
    outEndMs   = computeDstEndMs(year, tz);
}

uint64_t TimezoneTranslator::computeDstStartMs(uint16_t year, const TimezoneDefinition& tz) {
    uint8_t startDay = getDstSwitchDay(year, tz.dst_start_month, tz, true);
    uint64_t startLocal = dateToMs(year, tz.dst_start_month, startDay, tz.dst_start_hour, 0, 0);
    return startLocal - (int64_t)tz.offset_min * 60000LL;
}

uint64_t TimezoneTranslator::computeDstEndMs(uint16_t year, const TimezoneDefinition& tz) {
    uint8_t endDay = getDstSwitchDay(year, tz.dst_end_month, tz, false);
    uint64_t endLocal = dateToMs(year, tz.dst_end_month, endDay, tz.dst_end_hour, 0, 0);
    return endLocal - (int64_t)tz.offset_dst_min * 60000LL;
}

// ---- Internal: get offset for a UTC timestamp ----

int16_t TimezoneTranslator::getOffsetForUtc(uint64_t utcMs,
                                             const TimezoneDefinition& tz,
                                             DstCache& cache) {
    if (tz.dst_start_month == 0) {
        return tz.offset_min;
    }

    // O(1) hit: two comparisons, no year calculation
    if (utcMs >= cache.valid_from_ms && utcMs < cache.valid_until_ms) {
        return cache.current_offset;
    }

    // Cache miss: compute current year's transitions
    uint16_t year = yearFromMs(utcMs);
    uint64_t dstStartMs, dstEndMs;
    computeDstTransitions(year, tz, dstStartMs, dstEndMs);

    // Set cache to the exact period between adjacent transitions.
    // For the DST period the bounds are known; for standard-time periods
    // we compute the neighbouring year's transition so the cache spans the
    // full winter without a spurious year-boundary miss.
    if (dstStartMs < dstEndMs) {
        // Northern hemisphere
        if (utcMs < dstStartMs) {
            cache = { computeDstEndMs(year - 1, tz), dstStartMs, tz.offset_min };
        } else if (utcMs < dstEndMs) {
            cache = { dstStartMs, dstEndMs, tz.offset_dst_min };
        } else {
            cache = { dstEndMs, computeDstStartMs(year + 1, tz), tz.offset_min };
        }
    } else {
        // Southern hemisphere: DST wraps the year boundary
        if (utcMs < dstEndMs) {
            cache = { computeDstStartMs(year - 1, tz), dstEndMs, tz.offset_dst_min };
        } else if (utcMs < dstStartMs) {
            cache = { dstEndMs, dstStartMs, tz.offset_min };
        } else {
            cache = { dstStartMs, computeDstEndMs(year + 1, tz), tz.offset_dst_min };
        }
    }

    return cache.current_offset;
}

// ---- Internal: get offset for a local timestamp ----

int16_t TimezoneTranslator::getOffsetForLocal(uint64_t localMs,
                                               const TimezoneDefinition& tz,
                                               DstCache& cache) {
    if (tz.dst_start_month == 0) {
        return tz.offset_min;
    }

    // Approximate UTC using standard offset (exact outside transition windows)
    uint64_t approxUtc = localMs - (int64_t)tz.offset_min * 60000LL;

    // O(1) hit check; see note on fall-back overlap below
    if (approxUtc >= cache.valid_from_ms && approxUtc < cache.valid_until_ms) {
        return cache.current_offset;
    }

    // Cache miss: compute current year's transitions and set period bounds
    uint16_t year = yearFromMs(approxUtc);
    uint64_t dstStartMs, dstEndMs;
    computeDstTransitions(year, tz, dstStartMs, dstEndMs);

    if (dstStartMs < dstEndMs) {
        if (approxUtc < dstStartMs) {
            cache = { computeDstEndMs(year - 1, tz), dstStartMs, tz.offset_min };
        } else if (approxUtc < dstEndMs) {
            cache = { dstStartMs, dstEndMs, tz.offset_dst_min };
        } else {
            cache = { dstEndMs, computeDstStartMs(year + 1, tz), tz.offset_min };
        }
    } else {
        if (approxUtc < dstEndMs) {
            cache = { computeDstStartMs(year - 1, tz), dstEndMs, tz.offset_dst_min };
        } else if (approxUtc < dstStartMs) {
            cache = { dstEndMs, dstStartMs, tz.offset_min };
        } else {
            cache = { dstStartMs, computeDstEndMs(year + 1, tz), tz.offset_dst_min };
        }
    }

    // Full local-time comparison to correctly handle DST transitions.
    // approxUtc (using standard offset) lands outside the DST UTC range during
    // the fall-back overlap hour, so this miss path is reached for those cases
    // and the local comparison returns the correct DST offset.
    uint64_t dstStartLocal = dstStartMs + (int64_t)tz.offset_min * 60000LL;
    uint64_t dstEndLocal   = dstEndMs   + (int64_t)tz.offset_dst_min * 60000LL;

    if (dstStartMs < dstEndMs) {
        if (localMs >= dstStartLocal && localMs < dstEndLocal) {
            return tz.offset_dst_min;
        }
    } else {
        if (localMs >= dstStartLocal || localMs < dstEndLocal) {
            return tz.offset_dst_min;
        }
    }

    return tz.offset_min;
}

