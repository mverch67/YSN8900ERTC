# YSN8900ERTC

Arduino / PlatformIO driver for the YXC **YSN8900E** I²C real-time clock. It reads and writes the calendar as a `struct tm` and reports the power-loss flag, so a caller can tell a valid clock from one that lost its supply.

- Framework: Arduino (any platform with `TwoWire`)
- Dependencies: none beyond `Wire`
- I²C address: `0x32`
- Calendar range: 2000-01-01 to 2099-12-31, 24-hour

## Installation

PlatformIO (`platformio.ini`):

```ini
lib_deps =
  https://github.com/mverch67/YSN8900ERTC.git
```

Arduino IDE: copy this folder into your `libraries/` directory.

## Usage

```cpp
#include <Wire.h>
#include <YSN8900ERTC.h>

YSN8900ERTC rtc;

void setup()
{
    Wire.begin();
    if (!rtc.begin(Wire))
        return; // no YSN8900E at 0x32

    rtc.disableInterrupts();

    struct tm t;
    if (!rtc.getTime(t)) {
        // power was lost: set the clock from a trusted source
        t = {};
        t.tm_year = 2026 - 1900;
        t.tm_mon = 0;
        t.tm_mday = 1;
        t.tm_wday = 4;
        rtc.setTime(t);
    }
}
```

## API

| Method                                                           | Description                                                                                                                                                   |
| ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `bool begin(TwoWire &wire, uint8_t addr = 0x32)`                 | Binds the bus. Returns `false` if the chip does not ACK or its ID register does not read `0xD2`.                                                              |
| `bool getTime(struct tm &t)`                                     | Reads seconds to year. `tm_wday` is filled; `tm_yday` and `tm_isdst` are not. Returns `false` on a bus error, an out-of-range calendar, or when `VLF` is set. |
| `bool setTime(const struct tm &t)`                               | Writes the calendar and clears `VLF`/`VDET`. Returns `false` outside 2000–2099 or on a bus error.                                                             |
| `bool lostPower()`                                               | `true` while `VLF` is latched, or when the chip cannot be read.                                                                                               |
| `bool disableInterrupts()`                                       | Clears the timer enable (`TE`) and the update, timer and alarm interrupt enables, so `/INT` stays released.                                                   |
| `const char *chipName()`                                         | `"YSN8900E"`.                                                                                                                                                 |
| `static bool decodeCalendar(const uint8_t buf[7], struct tm &t)` | Register block `0x00`–`0x06` to `tm`, without I²C.                                                                                                            |
| `static bool encodeCalendar(const struct tm &t, uint8_t buf[7])` | `tm` to register block `0x00`–`0x06`, without I²C.                                                                                                            |

## Differences from the Epson RX8900

The YSN8900E follows the RX8900 register map for `0x00`–`0x1F`. Observed on hardware:

|                  | YSN8900E                                     | RX8900      |
| ---------------- | -------------------------------------------- | ----------- |
| Chip ID register | `0x20` reads `0xD2`                          | not present |
| Register `0x21`  | present (reads `0x80`, purpose undocumented) | not present |

Everything else used by this driver matches the RX8900:

- Weekday register `0x03` is one-hot (`0x01` = Sunday … `0x40` = Saturday), not BCD.
- Power loss is reported by `VLF` (flag register `0x0E`, bit 1). Seconds bit 7 is not a validity flag.
- Registers `0x10`–`0x16` mirror the calendar, `0x17` is the temperature sensor, `0x18` is the backup-function register (left untouched by this driver).
- After power-up the calendar reads 2000-01-01 (Saturday) with `VLF` and `VDET` set.

No public datasheet was available; the differences above come from register dumps of a production part.

## License

GPL-3.0 license
