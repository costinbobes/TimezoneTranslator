/**
 * @file    TimezoneTranslator.h
 * @brief   Ultra-fast timezone translation for Arduino (AVR, ESP8266, ESP32).
 * @author  Costin Bobes
 * @date    2026-02-21
 * @version 1.0.0
 *
 * Converts UTC timestamps to local time and vice-versa, handling arbitrary
 * DST rules for both northern and southern hemispheres.  All outputs are
 * 64-bit milliseconds.  32-bit second inputs are accepted and automatically
 * extended via the Jan-1-2020 rollover heuristic (see normalize32()).
 *
 * @par Caching
 * Each TimezoneTranslator instance caches the UTC boundaries of the current
 * offset period.  Repeated lookups within the same DST/standard season resolve
 * in O(1) — two uint64_t comparisons, no year calculation.
 *
 * @par Thread safety
 * Each instance carries its own cache.  Concurrent reads/writes to the same
 * instance from different threads (e.g. ESP32 dual-core) require external
 * synchronization (mutex).  Using separate instances per core is safe without
 * locking.
 *
 * @copyright (C) 2010-2026 Costin Bobes — MIT License
 */

#ifndef _TimezoneTranslator_h
#define _TimezoneTranslator_h

#include <inttypes.h>

/**
 * @brief Seconds from 1970-01-01 to 2020-01-01 (Unix epoch).
 *
 * Used as the cutoff for 32-bit rollover detection in normalize32().
 * Any uint32_t seconds value below this threshold is assumed to have
 * wrapped past 2^32 (Jan 19, 2038) and is treated as a future date.
 */
static const uint32_t UNIX_OFFSET_2020 = 1577836800UL;

/**
 * @brief Timezone definition with DST rules.
 *
 * Describes the UTC offset for standard time and DST, plus the rules that
 * govern when DST begins and ends each year.  Set @c dst_start_month to 0
 * to indicate that DST is not observed (fixed-offset timezone).
 *
 * @par Northern hemisphere example — US Eastern (America/New_York):
 * @code
 * TimezoneDefinition tdEST = {
 *     3,     // dst_start_month  — March
 *     2,     // dst_start_week   — 2nd occurrence
 *     11,    // dst_end_month    — November
 *     1,     // dst_end_week     — 1st occurrence
 *     0,     // dst_weekday      — Sunday
 *     2,     // dst_start_hour   — 02:00 local standard
 *     2,     // dst_end_hour     — 02:00 local DST
 *     -300,  // offset_min       — UTC-5 h  (standard)
 *     -240   // offset_dst_min   — UTC-4 h  (DST)
 * };
 * @endcode
 *
 * @par Southern hemisphere example — Australia/Sydney (AEST/AEDT):
 * @code
 * TimezoneDefinition tdAEST = {
 *     10,    // dst_start_month  — October
 *     1,     // dst_start_week   — 1st occurrence
 *     4,     // dst_end_month    — April
 *     1,     // dst_end_week     — 1st occurrence
 *     0,     // dst_weekday      — Sunday
 *     2,     // dst_start_hour   — 02:00 local standard
 *     3,     // dst_end_hour     — 03:00 local DST
 *     600,   // offset_min       — UTC+10 h (standard)
 *     660    // offset_dst_min   — UTC+11 h (DST)
 * };
 * @endcode
 *
 * @par Fixed-offset example — India Standard Time (Asia/Kolkata):
 * @code
 * TimezoneDefinition tdIST = { 0, 0, 0, 0, 0, 0, 0, 330, 330 };
 * @endcode
 */
struct TimezoneDefinition {
	uint8_t dst_start_month;   ///< Month DST begins, 1-12.  Set to 0 for no DST.
	int8_t  dst_start_week;    ///< Weekday occurrence: >0 = nth, <=0 = last in month.
	uint8_t dst_end_month;     ///< Month DST ends, 1-12.
	int8_t  dst_end_week;      ///< Weekday occurrence: >0 = nth, <=0 = last in month.
	uint8_t dst_weekday;       ///< Day of week for DST switch: 0=Sun, 1=Mon … 6=Sat.
	uint8_t dst_start_hour;    ///< Local standard-time hour when DST begins (0-23).
	uint8_t dst_end_hour;      ///< Local DST-time hour when DST ends (0-23).
	int16_t offset_min;        ///< UTC offset in minutes when DST is NOT active.
	int16_t offset_dst_min;    ///< UTC offset in minutes when DST IS active.
};

/**
 * @brief Cached DST period boundaries (milliseconds since Unix epoch).
 *
 * Stores the UTC interval [valid_from_ms, valid_until_ms) for the current
 * offset period.  A cache hit is two uint64_t comparisons; yearFromMs()
 * is only called on a miss.
 *
 * Users do not need to interact with this struct directly.
 */
struct DstCache {
	uint64_t valid_from_ms;    ///< UTC ms start of current period (inclusive).
	uint64_t valid_until_ms;   ///< UTC ms end of current period (exclusive).  0 = invalid.
	int16_t  current_offset;   ///< UTC offset in minutes for this period.
};

/**
 * @brief Broken-down time with millisecond precision.
 *
 * Analogous to C's @c struct&nbsp;tm but includes a millisecond field and
 * uses fixed-width types for cross-platform consistency (AVR, ESP8266, ESP32).
 */
struct TimeStruct {
	uint16_t year;             ///< Calendar year (1970+).
	uint8_t  month;            ///< Month, 1-12.
	uint8_t  day;              ///< Day of month, 1-31.
	uint8_t  hour;             ///< Hour, 0-23.
	uint8_t  minute;           ///< Minute, 0-59.
	uint8_t  second;           ///< Second, 0-59 (60 for rare leap second).
	uint16_t ms;               ///< Millisecond, 0-999.
	uint8_t  weekday;          ///< Day of week: 0=Sunday, 1=Monday … 6=Saturday.
};

/**
 * @brief High-performance UTC ↔ local-time translator with DST support.
 *
 * Create one instance per timezone you need at runtime.  Call
 * setLocalTimezone() to configure, then use utcToLocal() / localToUtc()
 * for conversions.  Alternatively, pass a TimezoneDefinition to each call
 * (slightly slower — the per-call temporary cache cannot be reused).
 *
 * All output timestamps are **64-bit milliseconds** since the Unix epoch.
 * 32-bit second inputs are accepted and automatically extended via a
 * Jan-1-2020 rollover heuristic — see the 32-bit overloads below.
 */
class TimezoneTranslator {
public:
	/** @brief Construct with default timezone UTC (offset 0, no DST). */
	TimezoneTranslator();

	/**
	 * @brief Set the default timezone for overloads that omit the tz parameter.
	 * @param tz  Timezone definition to store.
	 * @return @c true on success, @c false if @p tz is invalid
	 *         (e.g. start month > 12, or start month set without end month).
	 *
	 * Invalidates the internal cache.  Subsequent calls to the no-tz
	 * overloads will use this definition.
	 */
	bool setLocalTimezone(const TimezoneDefinition& tz);

	/**
	 * @brief Convert a UTC millisecond timestamp to local time.
	 * @param utcMs  Milliseconds since 1970-01-01 00:00:00 UTC.
	 * @param tz     Timezone definition (uses a temporary cache per call).
	 * @return Local millisecond timestamp.
	 */
	uint64_t utcToLocal(uint64_t utcMs, const TimezoneDefinition& tz);

	/**
	 * @brief Convert a UTC millisecond timestamp to local time using the
	 *        default timezone set by setLocalTimezone().
	 * @param utcMs  Milliseconds since 1970-01-01 00:00:00 UTC.
	 * @return Local millisecond timestamp.
	 */
	uint64_t utcToLocal(uint64_t utcMs);

	/**
	 * @brief Convert a local millisecond timestamp to UTC.
	 * @param localMs    Local milliseconds (UTC + offset applied).
	 * @param tz         Timezone definition.
 * @param preferDst  Disambiguates the DST fall-back overlap (the
 *                   "ambiguous hour" where local time repeats).
 *                   - @c true (default): DST interpretation — the
 *                     first pass through the hour, yielding the
 *                     **earlier** UTC instant.
 *                   - @c false: standard-time interpretation —
 *                     the second pass, yielding the **later** UTC
 *                     instant.
 * @return UTC millisecond timestamp.
 */
uint64_t localToUtc(uint64_t localMs, const TimezoneDefinition& tz, bool preferDst = true);

	/**
	 * @brief Convert a local millisecond timestamp to UTC using the
	 *        default timezone set by setLocalTimezone().
	 * @param localMs    Local milliseconds.
* @param preferDst  See localToUtc(uint64_t, const TimezoneDefinition&, bool).
	 * @return UTC millisecond timestamp.
	 * @note For deterministic fall-back overlap resolution with the
	 *       instance cache, prefer the explicit-tz overload.
	 */
uint64_t localToUtc(uint64_t localMs, bool preferDst = true);

	/**
	 * @brief Convert a 32-bit UTC seconds timestamp to local milliseconds.
	 *
	 * The value is first run through normalize32() to extend it to 64-bit
	 * milliseconds, then converted with utcToLocal().
	 *
	 * @param utcSec  Seconds since 1970-01-01 00:00:00 UTC (uint32_t).
	 * @param tz      Timezone definition.
	 * @return Local millisecond timestamp (64-bit).
	 */
	uint64_t utcToLocal(uint32_t utcSec, const TimezoneDefinition& tz);

	/** @copydoc utcToLocal(uint32_t,const TimezoneDefinition&)
	 *  Uses the default timezone set by setLocalTimezone(). */
	uint64_t utcToLocal(uint32_t utcSec);

	/**
	 * @brief Convert a 32-bit local seconds timestamp to UTC milliseconds.
	 * @param localSec   Local seconds (uint32_t).
	 * @param tz         Timezone definition.
	 * @param prefer_dst See localToUtc(uint64_t, const TimezoneDefinition&, bool).
	 * @return UTC millisecond timestamp (64-bit).
	 */
	uint64_t localToUtc(uint32_t localSec, const TimezoneDefinition& tz,
	                    bool prefer_dst = true);

	/** @copydoc localToUtc(uint32_t,const TimezoneDefinition&,bool)
	 *  Uses the default timezone set by setLocalTimezone(). */
	uint64_t localToUtc(uint32_t localSec, bool prefer_dst = true);

	/**
	 * @brief Decompose a millisecond timestamp into a TimeStruct.
	 *
	 * Similar to @c gmtime() but with millisecond precision and
	 * fixed-width types throughout.
	 *
	 * @param[out] dest   Pointer to a TimeStruct to fill.  Ignored if NULL.
	 * @param      utcMs  Milliseconds since epoch.
	 */
	static void toTimeStruct(TimeStruct* dest, uint64_t utcMs);

	/**
	 * @brief Build a UTC millisecond timestamp from calendar components.
	 * @param year    Calendar year (1970+).
	 * @param month   Month, 1-12.
	 * @param day     Day of month, 1-31.
	 * @param hour    Hour, 0-23.
	 * @param minute  Minute, 0-59.
	 * @param second  Second, 0-59.
	 * @return Milliseconds since 1970-01-01 00:00:00 UTC.
	 */
	static uint64_t dateToMs(uint16_t year, uint8_t month, uint8_t day,
							 uint8_t hour, uint8_t minute, uint8_t second);

private:
	TimezoneDefinition _tz;    ///< Default timezone.
	DstCache           _cache; ///< DST cache for default timezone.

	/** @brief Extend 32-bit seconds to 64-bit ms, applying the 2020 rollover heuristic. */
	static uint64_t normalize32(uint32_t utcSec);

	/** @brief Return 1 if @p year is a leap year, 0 otherwise. */
	static int8_t isLeapYear(uint16_t year);

	/** @brief Day-of-week (0=Sun…6=Sat) from a UTC millisecond timestamp. */
	static uint8_t getWeekday(uint64_t utcMs);

	/** @brief Days in @p month of @p year (28-31); 0 if month out of range. */
	static uint8_t getDaysInMonth(uint8_t month, uint16_t year);

	/** @brief Days since 1970-01-01 from a calendar date (pure 32-bit). */
	static uint32_t dateToDays(uint16_t year, uint8_t month, uint8_t day);

	/** @brief Day-of-week (0=Sun…6=Sat) from days since epoch (pure 32-bit). */
	static uint8_t getWeekdayFromDays(uint32_t daysSinceEpoch);

	/** @brief Determine UTC offset in minutes for a UTC ms timestamp (cache-accelerated). */
	static int16_t getOffsetForUtc(uint64_t utcMs, const TimezoneDefinition& tz, DstCache& cache);

	/** @brief Determine UTC offset in minutes for a local ms timestamp (cache-accelerated). */
static int16_t getOffsetForLocal(uint64_t localMs, const TimezoneDefinition& tz, DstCache& cache, bool preferDst = true);

	/** @brief Compute both DST transition UTC timestamps for a given year. */
	static void computeDstTransitions(uint16_t year, const TimezoneDefinition& tz,
									  uint64_t& outStartMs, uint64_t& outEndMs);

	/** @brief Extract calendar year from a UTC millisecond timestamp. */
	static uint16_t yearFromMs(uint64_t utcMs);

	/** @brief Extract calendar year from days since epoch (32-bit binary search). */
	static uint16_t yearFromDays(uint32_t daysSinceEpoch);

	/** @brief Get the day-of-month for a DST switch event. */
	static uint8_t getDstSwitchDay(uint16_t year, uint8_t month,
								   const TimezoneDefinition& tz, bool isStart);

	/** @brief Compute DST-start transition as UTC ms for a given year. */
	static uint64_t computeDstStartMs(uint16_t year, const TimezoneDefinition& tz);

	/** @brief Compute DST-end transition as UTC ms for a given year. */
	static uint64_t computeDstEndMs(uint16_t year, const TimezoneDefinition& tz);
};

#endif /* _TimezoneTranslator_h */
