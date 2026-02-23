/*
  DS3231_RTC_Example.ino
  TimezoneTranslator library — DS3231 RTC usage example.

  Demonstrates:
    1. Define a local timezone (America/New_York — US Eastern).
    2. Build a local date/time (year-month-day-hour-minute-second).
    3. Convert local time to UTC milliseconds using localToUtc().
    4. Truncate to whole seconds and write to the DS3231 RTC.
    5. Read seconds back from the DS3231 RTC.
    6. Convert the read-back UTC seconds to local milliseconds using utcToLocal().
    7. Decompose to Y-M-D H:M:S (ms part is truncated / zero since the RTC has no
       sub-second precision).

  Hardware:
    DS3231 connected via I2C (SDA/SCL).

  Dependencies:
    - TimezoneTranslator (this library)
    - RTClib by Adafruit  (install via Library Manager)

  The DS3231 stores time as whole seconds (no millisecond precision).
  When we write UTC to the RTC we truncate the ms part.
  When we read back we get whole seconds, so the local conversion
  will also have ms == 0.

  Upload, open Serial Monitor at 115200 baud.
*/

#include <Wire.h>
#include <RTClib.h>           // Adafruit RTClib
#include <TimezoneTranslator.h>

TimezoneTranslator tzConv;
RTC_DS3231 rtc;

// Print uint64_t to Serial (AVR has no 64-bit Serial.print overload)
static void printU64(uint64_t v) {
    if (v == 0) { Serial.print('0'); return; }
    char buf[21];
    int8_t i = 0;
    while (v > 0) { buf[i++] = '0' + (char)(v % 10); v /= 10; }
    while (i > 0) { Serial.print(buf[--i]); }
}

// Print a TimeStruct as "YYYY-MM-DD HH:MM:SS"
static void printTimeStruct(const TimeStruct& ts) {
    Serial.print(ts.year);   Serial.print('-');
    if (ts.month  < 10) Serial.print('0'); Serial.print(ts.month);  Serial.print('-');
    if (ts.day    < 10) Serial.print('0'); Serial.print(ts.day);    Serial.print(' ');
    if (ts.hour   < 10) Serial.print('0'); Serial.print(ts.hour);   Serial.print(':');
    if (ts.minute < 10) Serial.print('0'); Serial.print(ts.minute); Serial.print(':');
    if (ts.second < 10) Serial.print('0'); Serial.print(ts.second);
}

static const char* dayName(uint8_t wd) {
    static const char days[7][4] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
    return (wd < 7) ? days[wd] : "???";
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println(F("=== DS3231 RTC + TimezoneTranslator Example ==="));
    Serial.println();

    // ---- Initialize I2C and RTC ----
    Wire.begin();
    if (!rtc.begin()) {
        Serial.println(F("ERROR: DS3231 not found. Check wiring."));
        while (true) { delay(1000); }
    }
    Serial.println(F("DS3231 found."));

    // ---- Define timezone: America/New_York (US Eastern) ----
    // Standard: UTC-5 (EST), DST: UTC-4 (EDT)
    // DST starts: 2nd Sunday of March at 02:00 local
    // DST ends:   1st Sunday of November at 02:00 local
    TimezoneDefinition tdEST = {
        /* dst_start_month */ 3,
        /* dst_start_week  */ 2,      // 2nd occurrence
        /* dst_end_month   */ 11,
        /* dst_end_week    */ 1,      // 1st occurrence
        /* dst_weekday     */ 0,      // Sunday
        /* dst_start_hour  */ 2,      // 02:00 local
        /* dst_end_hour    */ 2,      // 02:00 local
        /* offset_min      */ -300,   // UTC-5 hours = -300 minutes
        /* offset_dst_min  */ -240    // UTC-4 hours = -240 minutes
    };

    tzConv.setLocalTimezone(tdEST);

    // ================================================================
    // Step 1: Build a local date/time to write into the RTC
    // Example: July 15, 2026 at 14:30:45 Eastern (this is during EDT)
    // ================================================================
    uint16_t localYear   = 2026;
    uint8_t  localMonth  = 7;
    uint8_t  localDay    = 15;
    uint8_t  localHour   = 14;
    uint8_t  localMinute = 30;
    uint8_t  localSecond = 45;

    Serial.print(F("Local time to set: "));
    Serial.print(localYear);  Serial.print('-');
    if (localMonth  < 10) Serial.print('0'); Serial.print(localMonth);  Serial.print('-');
    if (localDay    < 10) Serial.print('0'); Serial.print(localDay);    Serial.print(' ');
    if (localHour   < 10) Serial.print('0'); Serial.print(localHour);   Serial.print(':');
    if (localMinute < 10) Serial.print('0'); Serial.print(localMinute); Serial.print(':');
    if (localSecond < 10) Serial.print('0'); Serial.print(localSecond);
    Serial.println(F(" (America/New_York)"));

    // ================================================================
    // Step 2: Convert local -> UTC milliseconds
    // ================================================================
    uint64_t localMs = TimezoneTranslator::dateToMs(localYear, localMonth, localDay,
                                                     localHour, localMinute, localSecond);
    uint64_t utcMs = tzConv.localToUtc(localMs);

    Serial.print(F("UTC ms:  ")); printU64(utcMs); Serial.println();

    // Show the UTC broken-down time
    TimeStruct utcTs;
    TimezoneTranslator::toTimeStruct(&utcTs, utcMs);
    Serial.print(F("UTC:     ")); printTimeStruct(utcTs);
    Serial.print(F(" (")); Serial.print(dayName(utcTs.weekday)); Serial.println(')');

    // ================================================================
    // Step 3: Truncate to whole seconds and write to the DS3231
    // The DS3231 has no sub-second storage — only Y/M/D H:M:S.
    // ================================================================
    rtc.adjust(DateTime(utcTs.year, utcTs.month, utcTs.day,
                        utcTs.hour, utcTs.minute, utcTs.second));
    Serial.println(F("Written to DS3231 (UTC)."));
    Serial.println();

    // Small delay so we can read back a slightly different second
    delay(2000);

    // ================================================================
    // Step 4: Read back from the DS3231
    // ================================================================
    DateTime now = rtc.now();
    Serial.print(F("RTC now: "));
    Serial.print(now.year());  Serial.print('-');
    if (now.month()  < 10) Serial.print('0'); Serial.print(now.month());  Serial.print('-');
    if (now.day()    < 10) Serial.print('0'); Serial.print(now.day());    Serial.print(' ');
    if (now.hour()   < 10) Serial.print('0'); Serial.print(now.hour());   Serial.print(':');
    if (now.minute() < 10) Serial.print('0'); Serial.print(now.minute()); Serial.print(':');
    if (now.second() < 10) Serial.print('0'); Serial.print(now.second());
    Serial.println(F(" UTC"));

    // ================================================================
    // Step 5: Convert RTC read-back (UTC) to local time
    // Build UTC ms from the RTC fields.  ms part is 0 — the RTC has
    // only whole-second resolution, so the local result will also
    // have ms == 0.
    // ================================================================
    uint64_t rtcUtcMs = TimezoneTranslator::dateToMs(now.year(), now.month(), now.day(),
                                                      now.hour(), now.minute(), now.second());
    uint64_t rtcLocalMs = tzConv.utcToLocal(rtcUtcMs);

    TimeStruct localTs;
    TimezoneTranslator::toTimeStruct(&localTs, rtcLocalMs);

    Serial.print(F("Local:   ")); printTimeStruct(localTs);
    Serial.print(F(" (")); Serial.print(dayName(localTs.weekday)); Serial.println(')');
    Serial.print(F("(ms part is always 0 since DS3231 stores whole seconds only)"));
    Serial.println();

    Serial.println();
    Serial.println(F("=== Done ==="));
}

void loop() {
    // Nothing to do here.
    // In a real application you would read the RTC periodically and
    // convert to local time for display.
}
