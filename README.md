# TimezoneTranslator

Ultra-fast timezone translation library for Arduino.  Converts UTC timestamps
to local time and vice-versa, handling arbitrary DST rules for both northern
and southern hemispheres.

Designed to run on resource-constrained microcontrollers — from 8-bit AVR
(Arduino Uno/Nano) to 32-bit ESP8266 and ESP32.

## Features

- **64-bit millisecond precision** — all outputs are `uint64_t` milliseconds
  since the Unix epoch (1970-01-01 00:00:00 UTC).
- **32-bit second input** with automatic rollover handling — 32-bit Unix
  timestamps are accepted and extended to 64-bit via the Jan-1-2020
  rollover heuristic (see [32-bit Rollover](#32-bit-rollover-and-the-2020-cutoff)).
- **Arbitrary DST rules** — supports nth-weekday-of-month and
  last-weekday-of-month switch rules, covering both northern and southern
  hemisphere timezones.
- **O(1) cached lookups** — each instance caches the UTC boundaries of the
  current offset period.  Repeated conversions within the same DST/standard
  season resolve with just two `uint64_t` comparisons.
- **Pure 32-bit arithmetic** where possible — year-from-days binary search,
  date-to-days, and weekday calculations all use 32-bit math for AVR
  friendliness.
- **Multiple independent instances** — each `TimezoneTranslator` object
  carries its own timezone definition and cache.  Use one per timezone.
- **Broken-down time** — `toTimeStruct()` decomposes a millisecond timestamp
  into year/month/day/hour/minute/second/ms/weekday fields.
- **No dynamic allocation** — zero `malloc`/`new`; everything lives on the
  stack or in the object.

## Installation

### Arduino IDE (Library Manager)

1. Open **Sketch → Include Library → Manage Libraries…**
2. Search for **TimezoneTranslator**.
3. Click **Install**.

### Manual installation

1. Download or clone this repository.
2. Copy the `TimezoneTranslator` folder into your Arduino `libraries` directory
   (e.g. `~/Arduino/libraries/TimezoneTranslator`).
3. Restart the Arduino IDE.

### PlatformIO

Add to `platformio.ini`:
```ini
lib_deps =
    costinbobes/TimezoneTranslator
```

## Quick Start

```cpp
#include <TimezoneTranslator.h>

// US Eastern: UTC-5 standard, UTC-4 DST
// DST: 2nd Sunday of March at 02:00 → 1st Sunday of November at 02:00
TimezoneDefinition tzEST = { 3, 2, 11, 1, 0, 2, 2, -300, -240 };

TimezoneTranslator tz;

void setup() {
    Serial.begin(115200);
    tz.setLocalTimezone(tzEST);

    // Build a UTC timestamp: 2026-07-15 12:00:00 UTC
    uint64_t utcMs = TimezoneTranslator::dateToMs(2026, 7, 15, 12, 0, 0);

    // Convert to local time (EDT in July → UTC-4)
    uint64_t localMs = tz.utcToLocal(utcMs);

    // Decompose into fields
    TimeStruct ts;
    TimezoneTranslator::toTimeStruct(&ts, localMs);

    // Prints: 2026-07-15 08:00:00.000
    Serial.print(ts.year);  Serial.print('-');
    Serial.print(ts.month); Serial.print('-');
    Serial.print(ts.day);   Serial.print(' ');
    Serial.print(ts.hour);  Serial.print(':');
    Serial.print(ts.minute);Serial.print(':');
    Serial.println(ts.second);
}

void loop() {}
```

## API Reference

### Structs

#### `TimezoneDefinition`

Defines a timezone's UTC offset and DST transition rules.

| Field              | Type      | Description                                              |
|--------------------|-----------|----------------------------------------------------------|
| `dst_start_month`  | `uint8_t` | Month DST begins (1-12).  **Set to 0 for no DST.**      |
| `dst_start_week`   | `int8_t`  | >0: nth occurrence of weekday; ≤0: last in month.        |
| `dst_end_month`    | `uint8_t` | Month DST ends (1-12).                                   |
| `dst_end_week`     | `int8_t`  | >0: nth occurrence of weekday; ≤0: last in month.        |
| `dst_weekday`      | `uint8_t` | Day of week for DST switch: 0=Sun, 1=Mon … 6=Sat.       |
| `dst_start_hour`   | `uint8_t` | Local standard-time hour when DST begins (0-23).         |
| `dst_end_hour`     | `uint8_t` | Local DST-time hour when DST ends (0-23).                |
| `offset_min`       | `int16_t` | UTC offset in **minutes** when DST is **not** active.    |
| `offset_dst_min`   | `int16_t` | UTC offset in **minutes** when DST **is** active.        |

#### `TimeStruct`

Broken-down time with millisecond precision (analogous to `struct tm`).

| Field     | Type       | Description                                   |
|-----------|------------|-----------------------------------------------|
| `year`    | `uint16_t` | Calendar year (1970+).                        |
| `month`   | `uint8_t`  | Month, 1-12.                                  |
| `day`     | `uint8_t`  | Day of month, 1-31.                           |
| `hour`    | `uint8_t`  | Hour, 0-23.                                   |
| `minute`  | `uint8_t`  | Minute, 0-59.                                 |
| `second`  | `uint8_t`  | Second, 0-59.                                 |
| `ms`      | `uint16_t` | Millisecond, 0-999.                           |
| `weekday` | `uint8_t`  | Day of week: 0=Sunday … 6=Saturday.           |

#### `DstCache`

Internal cache structure.  Users do not need to interact with this directly.

### Class `TimezoneTranslator`

#### Construction

```cpp
TimezoneTranslator();
```
Creates an instance defaulting to UTC (offset 0, no DST).

#### `setLocalTimezone`

```cpp
bool setLocalTimezone(const TimezoneDefinition& tz);
```
Sets the default timezone.  Returns `false` if the definition is invalid
(e.g. `dst_start_month > 12`, or start month set without end month).
Invalidates the internal cache.

#### `utcToLocal` — 64-bit milliseconds

```cpp
uint64_t utcToLocal(uint64_t utcMs, const TimezoneDefinition& tz);
uint64_t utcToLocal(uint64_t utcMs);  // uses default tz
```
Converts a UTC millisecond timestamp to local milliseconds.

The explicit-tz overload uses a temporary cache (always cold).  The no-tz
overload uses the instance cache, which stays warm across calls in the same
DST/standard period.

#### `localToUtc` — 64-bit milliseconds

```cpp
uint64_t localToUtc(uint64_t localMs, const TimezoneDefinition& tz);
uint64_t localToUtc(uint64_t localMs);  // uses default tz
```
Converts a local millisecond timestamp to UTC milliseconds.

During the DST fall-back overlap (the "ambiguous hour") the standard-time
interpretation is returned (the earlier UTC instant).

#### `utcToLocal` / `localToUtc` — 32-bit seconds

```cpp
uint64_t utcToLocal(uint32_t utcSec, const TimezoneDefinition& tz);
uint64_t utcToLocal(uint32_t utcSec);
uint64_t localToUtc(uint32_t localSec, const TimezoneDefinition& tz);
uint64_t localToUtc(uint32_t localSec);
```
Accept a `uint32_t` seconds timestamp, apply the Jan-1-2020 rollover
heuristic internally, then perform the conversion.  **Output is always
64-bit milliseconds.**  See [32-bit Rollover](#32-bit-rollover-and-the-2020-cutoff).

#### Utility helpers

```cpp
static void    toTimeStruct(TimeStruct* dest, uint64_t utcMs);
static uint64_t dateToMs(uint16_t year, uint8_t month, uint8_t day,
                         uint8_t hour, uint8_t minute, uint8_t second);
```

## 32-bit Rollover and the 2020 Cutoff

### The problem

A `uint32_t` Unix timestamp counts seconds since 1970-01-01.  It overflows
on **2038-01-19 03:14:07 UTC**, after which the counter wraps back to 0.
Many RTC chips (DS1307, DS3231, PCF8563) and NTP libraries still expose time
as a 32-bit unsigned value, so post-2038 values will appear as small numbers
in the range `0 … ~1.58 billion`.

### The heuristic

The rollover heuristic uses **January 1, 2020** (Unix time 1,577,836,800) as a
dividing line:

| 32-bit value                   | Interpretation                                         |
|--------------------------------|--------------------------------------------------------|
| ≥ 1,577,836,800 (2020-01-01)  | Treated as-is — a normal timestamp in 2020 … 2038.     |
| < 1,577,836,800               | Assumed rolled over — **2³² seconds are added** before converting to milliseconds, placing the result in 2038 … 2106. |

This allows 32-bit sources to remain useful until approximately **2106**
without any firmware change.

### Why January 1, 2020?

- It is far enough in the past (relative to 2026) that no legitimate
  "current" timestamp will fall below it.
- It leaves the full 2020 … 2038 range addressable without rollover
  adjustment.
- The cutoff date is a round, memorable constant.

### Why you should prefer 64-bit

The 32-bit overloads exist for convenience when interfacing with hardware or
libraries that only provide seconds.  **All output from this library is 64-bit
milliseconds.**  Whenever you have a choice, work in 64-bit throughout:

- No rollover ambiguity.
- Millisecond precision preserved.
- Valid until approximately the year 586,512.

If your RTC only provides seconds (e.g. DS3231), convert to 64-bit ms as
early as possible using `dateToMs()` and stay in 64-bit from that point on.

## DST Rules Explained

DST transition rules are defined using three fields per transition:

- **month** — which month the switch occurs (1-12).
- **week** — which occurrence of the weekday:
  - `1` = first, `2` = second, `3` = third, `4` = fourth, `5` = fifth.
  - `0` or negative = **last** occurrence in the month.
- **weekday** — which day of the week (0=Sunday … 6=Saturday).

The **hour** field specifies the local wall-clock hour at which the switch
happens.  For DST start this is in standard time; for DST end this is in
DST time.

### Northern hemisphere

DST start month < DST end month.  The period between the two transitions is
the DST (summer) period.

### Southern hemisphere

DST start month > DST end month (e.g. October → April).  The DST period
wraps across the year boundary.  The library handles this automatically.

## Common Timezone Definitions

```cpp
// UTC — no DST
TimezoneDefinition TZ_UTC  = { 0, 0, 0, 0, 0, 0, 0,    0,    0 };

// US Eastern (America/New_York) — UTC-5 / UTC-4
TimezoneDefinition TZ_EST  = { 3, 2, 11, 1, 0, 2, 2, -300, -240 };

// US Central (America/Chicago) — UTC-6 / UTC-5
TimezoneDefinition TZ_CST  = { 3, 2, 11, 1, 0, 2, 2, -360, -300 };

// US Mountain (America/Denver) — UTC-7 / UTC-6
TimezoneDefinition TZ_MST  = { 3, 2, 11, 1, 0, 2, 2, -420, -360 };

// US Pacific (America/Los_Angeles) — UTC-8 / UTC-7
TimezoneDefinition TZ_PST  = { 3, 2, 11, 1, 0, 2, 2, -480, -420 };

// Central European (Europe/Berlin) — UTC+1 / UTC+2
TimezoneDefinition TZ_CET  = { 3,-1, 10,-1, 0, 2, 3,   60,  120 };

// Eastern European (Europe/Bucharest) — UTC+2 / UTC+3
TimezoneDefinition TZ_EET  = { 3,-1, 10,-1, 0, 3, 4,  120,  180 };

// India Standard Time (Asia/Kolkata) — UTC+5:30, no DST
TimezoneDefinition TZ_IST  = { 0, 0, 0, 0, 0, 0, 0,  330,  330 };

// Japan Standard Time (Asia/Tokyo) — UTC+9, no DST
TimezoneDefinition TZ_JST  = { 0, 0, 0, 0, 0, 0, 0,  540,  540 };

// Australia Eastern (Australia/Sydney) — UTC+10 / UTC+11
TimezoneDefinition TZ_AEST = { 10, 1, 4, 1, 0, 2, 3,  600,  660 };

// New Zealand (Pacific/Auckland) — UTC+12 / UTC+13
TimezoneDefinition TZ_NZST = { 9,-1, 4, 1, 0, 2, 3,  720,  780 };
```

## Examples

### Benchmark

`examples/Benchmark/Benchmark.ino` — Demonstrates the full API with readable
output, followed by raw performance measurements:

- No-DST fast path, cold vs warm cache, far-future years, batch throughput,
  32-bit vs 64-bit input overhead, `toTimeStruct` and `dateToMs` speed.

### DS3231 RTC

`examples/DS3231_RTC_Example/DS3231_RTC_Example.ino` — Real-world usage with
an I²C DS3231 RTC:

1. Define local timezone (America/New_York).
2. Set a local date/time, convert to UTC, write to the RTC.
3. Read UTC seconds back from the RTC.
4. Convert to local time, display as `YYYY-MM-DD HH:MM:SS`.

Requires the **RTClib** library by Adafruit (install via Library Manager).

## Performance

Measured on an ESP8266 (Generic ESP8266 Module, 80 MHz):

| Operation                        | Time      |
|----------------------------------|-----------|
| `utcToLocal` — no DST            | ~1 µs     |
| `utcToLocal` — cache hit         | ~2 µs     |
| `utcToLocal` — cache miss (cold) | ~100 µs   |
| `localToUtc` — cache hit         | ~3 µs     |
| `toTimeStruct`                   | ~30 µs    |
| `dateToMs`                       | ~15 µs    |

Run the Benchmark example on your target hardware for exact numbers.

## Memory Usage

- **Object size**: ~24 bytes per `TimezoneTranslator` instance
  (9-byte `TimezoneDefinition` + 18-byte `DstCache` + padding).
- **Code size**: ~2-3 KB Flash (platform-dependent).
- **Stack**: Conversions use a small fixed amount of stack; no heap allocation.

## Thread Safety

Each `TimezoneTranslator` instance is independent.  If you share one instance
across threads (e.g. ESP32 dual-core), protect it with a mutex.  Alternatively,
create one instance per core — each will maintain its own cache.

All `static` utility methods (`dateToMs`, `toTimeStruct`, etc.)
are stateless and thread-safe.

## License

Copyright (C) 2010-2026 Costin Bobes

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the
[GNU General Public License](https://www.gnu.org/licenses/gpl-3.0.html)
for more details.
