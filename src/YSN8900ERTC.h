// SPDX-License-Identifier: MIT
//
// Driver for the YXC YSN8900E real-time clock: an Epson RX8900 register map plus a chip ID at 0x20.
#pragma once

#include <Wire.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

class YSN8900ERTC
{
public:
    static constexpr uint8_t DEFAULT_ADDRESS = 0x32;
    static constexpr uint8_t CHIP_ID = 0xD2;

    // Returns false if the chip does not ACK or its ID register does not read CHIP_ID.
    bool begin(TwoWire &wire, uint8_t addr = DEFAULT_ADDRESS);

    // Reads the calendar into t (tm_wday is filled, tm_yday and tm_isdst are not).
    // Returns false on a bus error, an out-of-range calendar, or when VLF reports lost time.
    bool getTime(struct tm &t);

    // Writes the calendar and clears VLF/VDET. Only 2000-2099 is representable.
    bool setTime(const struct tm &t);

    // True while VLF is latched: the backup supply dropped and the calendar is meaningless.
    bool lostPower();

    // Turns off the periodic timer and the alarm/timer/update interrupts so /INT stays released.
    bool disableInterrupts();

    const char *chipName() const { return "YSN8900E"; }

    // Calendar register block (seconds..year) <-> tm, separate from I2C so a host test can drive them.
    static bool decodeCalendar(const uint8_t buf[7], struct tm &t);
    static bool encodeCalendar(const struct tm &t, uint8_t buf[7]);

private:
    bool readRegs(uint8_t reg, uint8_t *buf, size_t len);
    bool writeRegs(uint8_t reg, const uint8_t *buf, size_t len);

    TwoWire *_wire = nullptr;
    uint8_t _addr = DEFAULT_ADDRESS;
};
