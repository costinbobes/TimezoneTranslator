/*
  Benchmark.ino
  TimezoneTranslator library — usage examples and performance benchmark.

  Part 1: Demonstrates the library API with readable output.
  Part 2: Measures raw conversion performance on the target MCU.

  Upload, open Serial Monitor at 115200 baud.
*/

#include <TimezoneTranslator.h>

// ================================================================
// Helper: print uint64_t (AVR has no 64-bit Serial.print overload)
// ================================================================
static void printU64(uint64_t v) {
    if (v == 0) { Serial.print('0'); return; }
    char buf[21];
    int8_t i = 0;
    while (v > 0) { buf[i++] = '0' + (char)(v % 10); v /= 10; }
    while (i > 0) { Serial.print(buf[--i]); }
}

// ================================================================
// Helper: print a TimeStruct as "YYYY-MM-DD HH:MM:SS.mmm  Day"
// ================================================================
static const char DNAMES[7][4] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

static void printTS(const TimeStruct& t) {
    Serial.print(t.year);   Serial.print('-');
    if (t.month  < 10) Serial.print('0'); Serial.print(t.month);  Serial.print('-');
    if (t.day    < 10) Serial.print('0'); Serial.print(t.day);    Serial.print(' ');
    if (t.hour   < 10) Serial.print('0'); Serial.print(t.hour);   Serial.print(':');
    if (t.minute < 10) Serial.print('0'); Serial.print(t.minute); Serial.print(':');
    if (t.second < 10) Serial.print('0'); Serial.print(t.second); Serial.print('.');
    if (t.ms < 100) Serial.print('0');
    if (t.ms < 10)  Serial.print('0');
    Serial.print(t.ms);
    Serial.print(F("  "));
    Serial.print(DNAMES[t.weekday]);
}

// ================================================================
// Common timezone definitions used throughout the example
// ================================================================

// UTC — no DST, zero offset
static const TimezoneDefinition TZ_UTC  = { 0, 0, 0, 0, 0, 0, 0,    0,    0 };

// US Eastern (America/New_York) — UTC-5 / UTC-4
// DST: 2nd Sunday of March 02:00 -> 1st Sunday of November 02:00
static const TimezoneDefinition TZ_EST  = { 3, 2, 11, 1, 0, 2, 2, -300, -240 };

// US Pacific (America/Los_Angeles) — UTC-8 / UTC-7
// DST: 2nd Sunday of March 02:00 -> 1st Sunday of November 02:00
static const TimezoneDefinition TZ_PST  = { 3, 2, 11, 1, 0, 2, 2, -480, -420 };

// Central European (Europe/Berlin) — UTC+1 / UTC+2
// DST: last Sunday of March 02:00 -> last Sunday of October 03:00
static const TimezoneDefinition TZ_CET  = { 3,-1, 10,-1, 0, 2, 3,   60,  120 };

// Eastern European (Europe/Bucharest) — UTC+2 / UTC+3
// DST: last Sunday of March 03:00 -> last Sunday of October 04:00
static const TimezoneDefinition TZ_EET  = { 3,-1, 10,-1, 0, 3, 4,  120,  180 };

// India Standard Time (Asia/Kolkata) — UTC+5:30, no DST
static const TimezoneDefinition TZ_IST  = { 0, 0, 0, 0, 0, 0, 0,  330,  330 };

// Japan Standard Time (Asia/Tokyo) — UTC+9, no DST
static const TimezoneDefinition TZ_JST  = { 0, 0, 0, 0, 0, 0, 0,  540,  540 };


// ================================================================
// PART 1 — USAGE EXAMPLES
// ================================================================

void runUsageExamples() {
    Serial.println(F("=== Part 1: Usage Examples ==="));
    Serial.println();

    TimezoneTranslator tz;

    // ---- 1. Basic UTC -> local conversion ----
    Serial.println(F("1. UTC -> local (explicit timezone):"));

    // 2026-07-15 12:00:00.000 UTC
    uint64_t utcMs = TimezoneTranslator::dateToMs(2026, 7, 15, 12, 0, 0);
    Serial.print(F("   UTC ms: ")); printU64(utcMs); Serial.println();

    TimeStruct ts;
    TimezoneTranslator::toTimeStruct(&ts, utcMs);
    Serial.print(F("   UTC:    ")); printTS(ts); Serial.println();

    uint64_t localMs = tz.utcToLocal(utcMs, TZ_EST);
    TimezoneTranslator::toTimeStruct(&ts, localMs);
    Serial.print(F("   EST:    ")); printTS(ts); Serial.println(F("  (UTC-4 EDT)"));

    localMs = tz.utcToLocal(utcMs, TZ_CET);
    TimezoneTranslator::toTimeStruct(&ts, localMs);
    Serial.print(F("   CET:    ")); printTS(ts); Serial.println(F("  (UTC+2 CEST)"));

    localMs = tz.utcToLocal(utcMs, TZ_IST);
    TimezoneTranslator::toTimeStruct(&ts, localMs);
    Serial.print(F("   IST:    ")); printTS(ts); Serial.println(F("  (UTC+5:30)"));

    localMs = tz.utcToLocal(utcMs, TZ_JST);
    TimezoneTranslator::toTimeStruct(&ts, localMs);
    Serial.print(F("   JST:    ")); printTS(ts); Serial.println(F("  (UTC+9)"));
    Serial.println();

    // ---- 2. Local -> UTC round-trip ----
    Serial.println(F("2. Local -> UTC round-trip:"));

    // Build local time: 2026-12-25 09:00:00 EST (winter, standard time)
    uint64_t localChristmas = TimezoneTranslator::dateToMs(2026, 12, 25, 9, 0, 0);
    uint64_t utcChristmas = tz.localToUtc(localChristmas, TZ_EST);
    TimezoneTranslator::toTimeStruct(&ts, utcChristmas);
    Serial.print(F("   Local 2026-12-25 09:00 EST -> UTC: ")); printTS(ts); Serial.println();

    // Convert back to verify
    uint64_t backToLocal = tz.utcToLocal(utcChristmas, TZ_EST);
    TimezoneTranslator::toTimeStruct(&ts, backToLocal);
    Serial.print(F("   Back to EST:                       ")); printTS(ts); Serial.println();
    Serial.println();

    // ---- 3. Default timezone (setLocalTimezone) ----
    Serial.println(F("3. Default timezone (setLocalTimezone):"));

    tz.setLocalTimezone(TZ_EET);

    uint64_t summerUtc = TimezoneTranslator::dateToMs(2026, 8, 1, 0, 0, 0);
    localMs = tz.utcToLocal(summerUtc);  // uses default TZ_EET
    TimezoneTranslator::toTimeStruct(&ts, localMs);
    Serial.print(F("   2026-08-01 00:00 UTC -> EET: ")); printTS(ts); Serial.println(F("  (UTC+3 EEST)"));

    uint64_t winterUtc = TimezoneTranslator::dateToMs(2026, 12, 1, 0, 0, 0);
    localMs = tz.utcToLocal(winterUtc);
    TimezoneTranslator::toTimeStruct(&ts, localMs);
    Serial.print(F("   2026-12-01 00:00 UTC -> EET: ")); printTS(ts); Serial.println(F("  (UTC+2 EET)"));
    Serial.println();

    // ---- 4. DST boundary demonstration ----
    Serial.println(F("4. DST boundary (EET spring-forward, 2026-03-29):"));

    // EET DST starts: last Sunday March 2026 = March 29, 03:00 local (UTC+2)
    // That is 2026-03-29 01:00 UTC = 1774746000000 ms
    uint64_t dstEdge = 1774746000000ULL;

    localMs = tz.utcToLocal(dstEdge - 1);
    TimezoneTranslator::toTimeStruct(&ts, localMs);
    Serial.print(F("   -1ms before: ")); printTS(ts); Serial.println(F("  (EET, UTC+2)"));

    localMs = tz.utcToLocal(dstEdge);
    TimezoneTranslator::toTimeStruct(&ts, localMs);
    Serial.print(F("   At boundary: ")); printTS(ts); Serial.println(F("  (EEST, UTC+3)"));

    localMs = tz.utcToLocal(dstEdge + 1);
    TimezoneTranslator::toTimeStruct(&ts, localMs);
    Serial.print(F("   +1ms after:  ")); printTS(ts); Serial.println(F("  (EEST, UTC+3)"));
    Serial.println();

    // ---- 5. 32-bit seconds input (automatic rollover handling) ----
    Serial.println(F("5. 32-bit seconds input (auto rollover):"));

    // Above Jan 1 2020 -- treated as-is, no rollover
    uint32_t sec2026 = 1767225600UL;
    localMs = tz.utcToLocal(sec2026, TZ_UTC);
    TimezoneTranslator::toTimeStruct(&ts, localMs);
    Serial.print(F("   2026-01-01 (above 2020): ")); printTS(ts); Serial.println(F("  (no rollover)"));

    // Below Jan 1 2020 -- assumed rolled over past 2038, placed in the future
    uint32_t secRolled = 100000000UL;
    localMs = tz.utcToLocal(secRolled, TZ_UTC);
    TimezoneTranslator::toTimeStruct(&ts, localMs);
    Serial.print(F("   100000000  (below 2020): ")); printTS(ts); Serial.println(F("  (rollover applied)"));

    localMs = tz.utcToLocal(sec2026, TZ_EET);
    TimezoneTranslator::toTimeStruct(&ts, localMs);
    Serial.print(F("   utcToLocal(uint32_t):    ")); printTS(ts); Serial.println(F("  (EET winter)"));
    Serial.println();
    // ---- 6. Multiple independent instances ----
    Serial.println(F("6. Multiple independent instances:"));

    TimezoneTranslator tzNewYork;
    TimezoneTranslator tzTokyo;
    tzNewYork.setLocalTimezone(TZ_EST);
    tzTokyo.setLocalTimezone(TZ_JST);

    uint64_t nowUtc = TimezoneTranslator::dateToMs(2026, 6, 15, 18, 0, 0);

    localMs = tzNewYork.utcToLocal(nowUtc);
    TimezoneTranslator::toTimeStruct(&ts, localMs);
    Serial.print(F("   New York: ")); printTS(ts); Serial.println();

    localMs = tzTokyo.utcToLocal(nowUtc);
    TimezoneTranslator::toTimeStruct(&ts, localMs);
    Serial.print(F("   Tokyo:    ")); printTS(ts); Serial.println();
    Serial.println();

}


// ================================================================
// PART 2 — RAW PERFORMANCE BENCHMARK
// ================================================================

void runBenchmark() {
    Serial.println(F("=== Part 2: Performance Benchmark ==="));
    Serial.println(F("All times in microseconds (us)."));
    Serial.println(F("Explicit-tz overloads use a temp cache (always cold)."));
    Serial.println(F("No-tz overloads use the instance cache (warm on repeat)."));
    Serial.println();

    TimezoneTranslator tz;

    uint64_t tSummer = 1625097600000ULL;  // 2021-07-01 00:00 UTC
    uint64_t tWinter = 1609459200000ULL;  // 2021-01-01 00:00 UTC
    uint64_t t2100   = 4118083200000ULL;  // 2100-07-01 00:00 UTC
    uint64_t t2400   = 13585190400000ULL; // 2400-07-01 00:00 UTC

    unsigned long start, elapsed;

    // ---- 1. No-DST fast path (pure addition) ----
    Serial.println(F("1. No-DST fast path:"));

    start = micros();
    (void)tz.utcToLocal(tSummer, TZ_UTC);
    elapsed = micros() - start;
    Serial.print(F("   UTC (0):        ")); Serial.print(elapsed); Serial.println(F(" us"));

    start = micros();
    (void)tz.utcToLocal(tSummer, TZ_IST);
    elapsed = micros() - start;
    Serial.print(F("   IST (+5:30):    ")); Serial.print(elapsed); Serial.println(F(" us"));

    // ---- 2. Explicit-tz (always cold, temp cache) ----
    Serial.println(F("2. Explicit-tz (cold, EET):"));

    start = micros();
    (void)tz.utcToLocal(tSummer, TZ_EET);
    elapsed = micros() - start;
    Serial.print(F("   call 1: ")); Serial.print(elapsed); Serial.println(F(" us"));

    start = micros();
    (void)tz.utcToLocal(tSummer + 3600000ULL, TZ_EET);
    elapsed = micros() - start;
    Serial.print(F("   call 2: ")); Serial.print(elapsed); Serial.println(F(" us  (same cost)"));

    // ---- 3. Instance cache — miss vs hit ----
    Serial.println(F("3. Instance cache (EET):"));

    tz.setLocalTimezone(TZ_EET);

    start = micros();
    (void)tz.utcToLocal(tSummer);
    elapsed = micros() - start;
    Serial.print(F("   miss (summer): ")); Serial.print(elapsed); Serial.println(F(" us"));

    start = micros();
    (void)tz.utcToLocal(tSummer + 3600000ULL);
    elapsed = micros() - start;
    Serial.print(F("   hit  (summer): ")); Serial.print(elapsed); Serial.println(F(" us"));

    start = micros();
    (void)tz.utcToLocal(tWinter);
    elapsed = micros() - start;
    Serial.print(F("   miss (winter): ")); Serial.print(elapsed); Serial.println(F(" us"));

    start = micros();
    (void)tz.utcToLocal(tWinter + 3600000ULL);
    elapsed = micros() - start;
    Serial.print(F("   hit  (winter): ")); Serial.print(elapsed); Serial.println(F(" us"));

    // ---- 4. Far future years ----
    Serial.println(F("4. Far future years (cold, explicit-tz):"));

    start = micros();
    (void)tz.utcToLocal(tSummer, TZ_EET);
    elapsed = micros() - start;
    Serial.print(F("   2021: ")); Serial.print(elapsed); Serial.println(F(" us"));

    start = micros();
    (void)tz.utcToLocal(t2100, TZ_EET);
    elapsed = micros() - start;
    Serial.print(F("   2100: ")); Serial.print(elapsed); Serial.println(F(" us"));

    start = micros();
    (void)tz.utcToLocal(t2400, TZ_EET);
    elapsed = micros() - start;
    Serial.print(F("   2400: ")); Serial.print(elapsed); Serial.println(F(" us"));

    // ---- 5. utcToLocal vs localToUtc ----
    Serial.println(F("5. utcToLocal vs localToUtc (cache hit):"));

    tz.setLocalTimezone(TZ_EET);
    (void)tz.utcToLocal(tSummer);  // warm cache

    start = micros();
    (void)tz.utcToLocal(tSummer);
    elapsed = micros() - start;
    Serial.print(F("   utcToLocal: ")); Serial.print(elapsed); Serial.println(F(" us"));

    uint64_t localEEST = tSummer + 10800000ULL;  // +3h
    start = micros();
    (void)tz.localToUtc(localEEST);
    elapsed = micros() - start;
    Serial.print(F("   localToUtc: ")); Serial.print(elapsed); Serial.println(F(" us"));

    // ---- 6. toTimeStruct ----
    Serial.println(F("6. toTimeStruct:"));
    TimeStruct ts;

    start = micros();
    TimezoneTranslator::toTimeStruct(&ts, tSummer);
    elapsed = micros() - start;
    Serial.print(F("   2021: ")); Serial.print(elapsed); Serial.println(F(" us"));

    start = micros();
    TimezoneTranslator::toTimeStruct(&ts, t2100);
    elapsed = micros() - start;
    Serial.print(F("   2100: ")); Serial.print(elapsed); Serial.println(F(" us"));

    start = micros();
    TimezoneTranslator::toTimeStruct(&ts, t2400);
    elapsed = micros() - start;
    Serial.print(F("   2400: ")); Serial.print(elapsed); Serial.println(F(" us"));

    // ---- 7. 32-bit vs 64-bit input ----
    Serial.println(F("7. 32-bit vs 64-bit input (cold, explicit-tz):"));

    start = micros();
    (void)tz.utcToLocal(tSummer, TZ_EET);
    elapsed = micros() - start;
    Serial.print(F("   64-bit:              ")); Serial.print(elapsed); Serial.println(F(" us"));

    start = micros();
    (void)tz.utcToLocal((uint32_t)1625097600UL, TZ_EET);
    elapsed = micros() - start;
    Serial.print(F("   32-bit (no rollover):")); Serial.print(elapsed); Serial.println(F(" us"));

    start = micros();
    (void)tz.utcToLocal((uint32_t)1000000000UL, TZ_EET);
    elapsed = micros() - start;
    Serial.print(F("   32-bit (rollover):   ")); Serial.print(elapsed); Serial.println(F(" us"));

    // ---- 8. Batch throughput ----
    Serial.println(F("8. Batch 100x (instance cache, EET):"));

    tz.setLocalTimezone(TZ_EET);
    (void)tz.utcToLocal(tSummer);  // prime

    start = micros();
    for (int i = 0; i < 100; i++) {
        (void)tz.utcToLocal(tSummer + (uint64_t)i * 3600000ULL);
    }
    elapsed = micros() - start;
    Serial.print(F("   same year (hit):  total ")); Serial.print(elapsed);
    Serial.print(F(" us, avg ")); Serial.print(elapsed / 100); Serial.println(F(" us"));

    start = micros();
    for (int i = 0; i < 100; i++) {
        (void)tz.utcToLocal(tSummer + (uint64_t)i * 86400000ULL * 366ULL);
    }
    elapsed = micros() - start;
    Serial.print(F("   diff year (miss): total ")); Serial.print(elapsed);
    Serial.print(F(" us, avg ")); Serial.print(elapsed / 100); Serial.println(F(" us"));

    // ---- 9. dateToMs throughput ----
    Serial.println(F("9. dateToMs (530 years, Jan 1 each):"));
    {
        uint64_t sum = 0;
        start = micros();
        for (uint16_t y = 1970; y < 2500; y++) {
            sum += TimezoneTranslator::dateToMs(y, 1, 1, 0, 0, 0);
        }
        elapsed = micros() - start;
        Serial.print(F("   total ")); Serial.print(elapsed);
        Serial.print(F(" us, avg ")); Serial.print(elapsed / 530);
        Serial.print(F(" us  (checksum ")); printU64(sum); Serial.println(')');
    }

    // ---- 10. toTimeStruct batch ----
    Serial.println(F("10. toTimeStruct batch (530 years):"));
    {
        start = micros();
        for (uint16_t y = 1970; y < 2500; y++) {
            TimezoneTranslator::toTimeStruct(&ts, TimezoneTranslator::dateToMs(y, 1, 1, 0, 0, 0));
        }
        elapsed = micros() - start;
        Serial.print(F("   total ")); Serial.print(elapsed);
        Serial.print(F(" us, avg ")); Serial.print(elapsed / 530); Serial.println(F(" us"));
    }

    Serial.println();
    Serial.println(F("=== Benchmark Complete ==="));
}


// ================================================================
// PART 3 — UNIT TESTS
// ================================================================
// Prints "PASS" / "FAIL" for each case, then a summary.
// All expected values were computed against known UTC reference times.
// ================================================================

static uint16_t g_pass = 0;
static uint16_t g_fail = 0;

static void check(const __FlashStringHelper* label, bool ok) {
    if (ok) {
        Serial.print(F("  PASS: "));
        g_pass++;
    } else {
        Serial.print(F("  FAIL: "));
        g_fail++;
    }
    Serial.println(label);
}

void runUnitTests() {
    Serial.println(F("=== Part 3: Unit Tests ==="));
    Serial.println();
    g_pass = 0;
    g_fail = 0;
    TimezoneTranslator tz;

    // ----------------------------------------------------------------
    // 1. Predefined timezone offsets on a known UTC timestamp
    //    2026-07-15 12:00:00 UTC (summer, DST active for northern-hemisphere zones)
    // ----------------------------------------------------------------
    Serial.println(F("1. UTC offset correctness (2026-07-15 12:00 UTC):"));
    uint64_t utcMs = TimezoneTranslator::dateToMs(2026, 7, 15, 12, 0, 0);
    TimeStruct ts;

    // UTC (no offset) → hour stays 12
    TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(utcMs, TZ_UTC));
    check(F("UTC  → 12:00"), ts.hour == 12 && ts.minute == 0);

    // EST (UTC-4 in summer / EDT) → 08:00
    TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(utcMs, TZ_EST));
    check(F("EST  → 08:00 (EDT)"), ts.hour == 8 && ts.minute == 0);

    // PST (UTC-7 in summer / PDT) → 05:00
    TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(utcMs, TZ_PST));
    check(F("PST  → 05:00 (PDT)"), ts.hour == 5 && ts.minute == 0);

    // CET (UTC+2 in summer / CEST) → 14:00
    TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(utcMs, TZ_CET));
    check(F("CET  → 14:00 (CEST)"), ts.hour == 14 && ts.minute == 0);

    // IST (UTC+5:30, no DST) → 17:30
    TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(utcMs, TZ_IST));
    check(F("IST  → 17:30"), ts.hour == 17 && ts.minute == 30);

    // JST (UTC+9, no DST) → 21:00
    TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(utcMs, TZ_JST));
    check(F("JST  → 21:00"), ts.hour == 21 && ts.minute == 0);

    // Winter UTC offsets: 2026-12-15 12:00 UTC
    uint64_t winterUtc = TimezoneTranslator::dateToMs(2026, 12, 15, 12, 0, 0);
    TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(winterUtc, TZ_EST));
    check(F("EST  winter → 07:00 (EST)"), ts.hour == 7 && ts.minute == 0);
    TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(winterUtc, TZ_CET));
    check(F("CET  winter → 13:00 (CET)"), ts.hour == 13 && ts.minute == 0);
    Serial.println();

    // ----------------------------------------------------------------
    // 2. DST boundary — US Eastern spring-forward 2026
    //    2026-03-08 02:00 EST = 07:00:00 UTC
    //    Before: local is in standard time (UTC-5)
    //    At/after: local is in DST (UTC-4)
    // ----------------------------------------------------------------
    Serial.println(F("2. US Eastern spring-forward 2026-03-08 07:00 UTC:"));
    {
        // 2026-03-08 07:00:00.000 UTC  (exact transition moment)
        uint64_t transition = TimezoneTranslator::dateToMs(2026, 3, 8, 7, 0, 0);

        // 1 ms before: still standard time → local = 01:59:59.999 EST
        TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(transition - 1, TZ_EST));
        check(F("1 ms before: hour=01 min=59 (EST)"), ts.hour == 1 && ts.minute == 59);

        // Exactly at transition: now DST → local = 03:00:00.000 EDT
        TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(transition, TZ_EST));
        check(F("At transition: hour=03 (EDT)"), ts.hour == 3 && ts.minute == 0);

        // 1 ms after: DST → local = 03:00:00.001 EDT
        TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(transition + 1, TZ_EST));
        check(F("1 ms after:  hour=03 (EDT)"), ts.hour == 3 && ts.minute == 0 && ts.ms == 1);
    }
    Serial.println();

    // ----------------------------------------------------------------
    // 3. DST boundary — US Eastern fall-back 2026
    //    2026-11-01 02:00 EDT = 06:00:00 UTC
    //    Before: local is in DST (UTC-4)
    //    At/after: local is in standard time (UTC-5)
    // ----------------------------------------------------------------
    Serial.println(F("3. US Eastern fall-back 2026-11-01 06:00 UTC:"));
    {
        uint64_t transition = TimezoneTranslator::dateToMs(2026, 11, 1, 6, 0, 0);

        // 1 ms before: DST → 01:59:59.999 EDT
        TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(transition - 1, TZ_EST));
        check(F("1 ms before: hour=01 min=59 (EDT)"), ts.hour == 1 && ts.minute == 59);

        // At transition: standard → 01:00:00.000 EST
        TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(transition, TZ_EST));
        check(F("At transition: hour=01 (EST)"), ts.hour == 1 && ts.minute == 0);

        // 1 ms after: standard → 01:00:00.001 EST
        TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(transition + 1, TZ_EST));
        check(F("1 ms after:  hour=01 (EST)"), ts.hour == 1 && ts.minute == 0 && ts.ms == 1);
    }
    Serial.println();

    // ----------------------------------------------------------------
    // 4. Fall-back overlap — prefer_dst behaviour
    //    01:30:00 local on 2026-11-01 is ambiguous.
    //    prefer_dst=true  → DST (EDT UTC-4) → 05:30 UTC
    //    prefer_dst=false → standard (EST UTC-5) → 06:30 UTC
    // ----------------------------------------------------------------
    Serial.println(F("4. Fall-back overlap (01:30 on 2026-11-01):"));
    {
        uint64_t localAmbig = TimezoneTranslator::dateToMs(2026, 11, 1, 1, 30, 0);

        uint64_t utcDst = tz.localToUtc(localAmbig, TZ_EST, true);   // prefer DST
        uint64_t utcStd = tz.localToUtc(localAmbig, TZ_EST, false);  // prefer standard

        uint64_t expected_dst = TimezoneTranslator::dateToMs(2026, 11, 1, 5, 30, 0);
        uint64_t expected_std = TimezoneTranslator::dateToMs(2026, 11, 1, 6, 30, 0);

        check(F("prefer_dst=true  → 05:30 UTC (EDT)"), utcDst == expected_dst);
        check(F("prefer_dst=false → 06:30 UTC (EST)"), utcStd == expected_std);

        // Outside overlap (02:30 EST Nov 1 is unambiguous standard)
        uint64_t localUnambig = TimezoneTranslator::dateToMs(2026, 11, 1, 2, 30, 0);
        uint64_t utcA = tz.localToUtc(localUnambig, TZ_EST, true);
        uint64_t utcB = tz.localToUtc(localUnambig, TZ_EST, false);
        check(F("02:30 (unambiguous): both prefer_dst agree"), utcA == utcB);
    }
    Serial.println();

    // ----------------------------------------------------------------
    // 5. Southern hemisphere DST — Australia/Sydney (AEST/AEDT)
    //    DST starts: 1st Sunday October at 02:00 AEST → UTC+11
    //    In 2026: Oct 4 02:00 AEST = Oct 3 16:00 UTC
    //    DST ends:  1st Sunday April at 03:00 AEDT → UTC+10
    //    In 2027: Apr 4 03:00 AEDT = Apr 3 16:00 UTC
    // ----------------------------------------------------------------
    Serial.println(F("5. Southern hemisphere DST (Australia/Sydney):"));
    {
        static const TimezoneDefinition TZ_AEST = { 10, 1, 4, 1, 0, 2, 3, 600, 660 };

        // June (deep winter in southern = AEST, UTC+10) → offset 600
        uint64_t winterAust = TimezoneTranslator::dateToMs(2026, 6, 15, 12, 0, 0);
        TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(winterAust, TZ_AEST));
        check(F("Jun 15 12:00 UTC → 22:00 AEST (+10)"), ts.hour == 22 && ts.minute == 0);

        // December (summer in southern = AEDT, UTC+11) → offset 660
        uint64_t summerAust = TimezoneTranslator::dateToMs(2026, 12, 15, 12, 0, 0);
        TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(summerAust, TZ_AEST));
        check(F("Dec 15 12:00 UTC → 23:00 AEDT (+11)"), ts.hour == 23 && ts.minute == 0);

        // Spring-forward: Oct 3 16:00:00 UTC  (1 ms before → AEST, at → AEDT)
        uint64_t austStart = TimezoneTranslator::dateToMs(2026, 10, 3, 16, 0, 0);
        TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(austStart - 1, TZ_AEST));
        check(F("1 ms before AEDT start → 01:59 AEST (+10)"),
              ts.hour == 1 && ts.minute == 59);
        TimezoneTranslator::toTimeStruct(&ts, tz.utcToLocal(austStart, TZ_AEST));
        check(F("At AEDT start → 03:00 AEDT (+11)"), ts.hour == 3 && ts.minute == 0);
    }
    Serial.println();

    // ----------------------------------------------------------------
    // 6. 32-bit rollover heuristic — UNIX_OFFSET_2020 = 1577836800
    // ----------------------------------------------------------------
    Serial.println(F("6. 32-bit rollover heuristic:"));
    {
        // Exactly at cutoff: treated as 2020-01-01 00:00:00 UTC
        uint64_t atCutoff = TimezoneTranslator::dateToMs(2020, 1, 1, 0, 0, 0);
        uint64_t r1 = tz.utcToLocal((uint32_t)UNIX_OFFSET_2020, TZ_UTC);
        check(F("At 2020-01-01 → no rollover"), r1 == atCutoff);

        // One second above cutoff: 2020-01-01 00:00:01 UTC
        uint64_t r2 = tz.utcToLocal((uint32_t)(UNIX_OFFSET_2020 + 1), TZ_UTC);
        check(F("UNIX_OFFSET_2020+1 → 2020-01-01 00:00:01"), r2 == atCutoff + 1000ULL);

        // One second below cutoff: treated as rolled over (add 2^32 seconds)
        uint64_t r3 = tz.utcToLocal((uint32_t)(UNIX_OFFSET_2020 - 1), TZ_UTC);
        uint64_t expected_rolled = ((uint64_t)(UNIX_OFFSET_2020 - 1) + 4294967296ULL) * 1000ULL;
        check(F("UNIX_OFFSET_2020-1 → rolled over"), r3 == expected_rolled);

        // Far below cutoff (100000000): rollover applied
        uint64_t r4 = tz.utcToLocal((uint32_t)100000000UL, TZ_UTC);
        uint64_t expected_far = ((uint64_t)100000000UL + 4294967296ULL) * 1000ULL;
        check(F("100000000 → rollover applied"), r4 == expected_far);
    }
    Serial.println();

    // ----------------------------------------------------------------
    // 7. Leap year edge cases
    // ----------------------------------------------------------------
    Serial.println(F("7. Leap year edge cases:"));
    {
        // 2024 is a leap year — Feb 29 exists
        uint64_t feb29 = TimezoneTranslator::dateToMs(2024, 2, 29, 0, 0, 0);
        TimezoneTranslator::toTimeStruct(&ts, feb29);
        check(F("2024-02-29 exists (month=2 day=29)"), ts.year == 2024 && ts.month == 2 && ts.day == 29);

        // Day after Feb 29 in 2024 must be Mar 1
        TimezoneTranslator::toTimeStruct(&ts, feb29 + 86400000ULL);
        check(F("2024-02-29 + 1 day = 2024-03-01"), ts.year == 2024 && ts.month == 3 && ts.day == 1);

        // 2100 is NOT a leap year (divisible by 100 but not 400)
        // Feb 28, 2100 + 1 day should be Mar 1 (not Feb 29)
        uint64_t feb28_2100 = TimezoneTranslator::dateToMs(2100, 2, 28, 0, 0, 0);
        TimezoneTranslator::toTimeStruct(&ts, feb28_2100 + 86400000ULL);
        check(F("2100-02-28 + 1 day = 2100-03-01 (not leap)"),
              ts.year == 2100 && ts.month == 3 && ts.day == 1);

        // 2000 IS a leap year (divisible by 400)
        uint64_t feb29_2000 = TimezoneTranslator::dateToMs(2000, 2, 29, 0, 0, 0);
        TimezoneTranslator::toTimeStruct(&ts, feb29_2000);
        check(F("2000-02-29 exists (leap, div 400)"),
              ts.year == 2000 && ts.month == 2 && ts.day == 29);
    }
    Serial.println();

    // ----------------------------------------------------------------
    // 8. dateToMs / toTimeStruct round-trip
    // ----------------------------------------------------------------
    Serial.println(F("8. dateToMs <-> toTimeStruct round-trip:"));
    {
        struct { uint16_t y; uint8_t mo,d,h,mi,s; } cases[] = {
            { 1970,  1,  1,  0,  0,  0 },   // epoch
            { 2024,  2, 29, 13, 45, 22 },   // leap day
            { 2026,  3,  8,  7,  0,  0 },   // spring-forward moment (US Eastern)
            { 2026, 11,  1,  6,  0,  0 },   // fall-back moment (US Eastern)
            { 2100,  3,  1,  0,  0,  0 },   // post-2100 non-leap boundary
            { 2400,  7,  4, 23, 59, 59 },   // far future
        };
        for (uint8_t i = 0; i < 6; i++) {
            uint64_t timestampMs = TimezoneTranslator::dateToMs(cases[i].y, cases[i].mo, cases[i].d,
                                                       cases[i].h, cases[i].mi, cases[i].s);
            TimezoneTranslator::toTimeStruct(&ts, timestampMs);
            bool ok = (ts.year   == cases[i].y  && ts.month  == cases[i].mo &&
                       ts.day    == cases[i].d   && ts.hour   == cases[i].h  &&
                       ts.minute == cases[i].mi  && ts.second == cases[i].s  &&
                       ts.ms     == 0);
            check(F("dateToMs->toTimeStruct round-trips"), ok);
        }
    }
    Serial.println();

    // ----------------------------------------------------------------
    // Summary
    // ----------------------------------------------------------------
    Serial.print(F("=== Results: "));
    Serial.print(g_pass); Serial.print(F(" passed, "));
    Serial.print(g_fail); Serial.println(F(" failed ==="));
    Serial.println();
}




void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println(F("=== TimezoneTranslator - Benchmark Example ==="));
    Serial.println();

    runUsageExamples();
    runBenchmark();
    runUnitTests();
}

void loop() {
}
