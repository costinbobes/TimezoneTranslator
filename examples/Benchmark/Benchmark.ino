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
// ARDUINO ENTRY POINTS
// ================================================================

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println(F("=== TimezoneTranslator - Benchmark Example ==="));
    Serial.println();

    runUsageExamples();
    runBenchmark();
}

void loop() {
}
