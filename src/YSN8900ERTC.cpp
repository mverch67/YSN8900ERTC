// SPDX-License-Identifier: MIT
//
// See YSN8900ERTC.h. Register layout follows the Epson RX8900 application manual; the chip ID and
// the one-hot weekday were confirmed by register dumps on a SenseCAP MeshPager X2.
#include "YSN8900ERTC.h"

namespace
{
    constexpr uint8_t REG_SECONDS = 0x00;
    constexpr uint8_t REG_EXTENSION = 0x0D;
    constexpr uint8_t REG_FLAG = 0x0E;
    constexpr uint8_t REG_CONTROL = 0x0F;
    constexpr uint8_t REG_CHIP_ID = 0x20;

    constexpr uint8_t EXT_TE = 0x10;
    constexpr uint8_t FLAG_VLF = 0x02;
    constexpr uint8_t FLAG_VDET = 0x01;
    constexpr uint8_t CTRL_UIE = 0x20;
    constexpr uint8_t CTRL_TIE = 0x10;
    constexpr uint8_t CTRL_AIE = 0x08;

    uint8_t bcdToDec(uint8_t v)
    {
        return (v >> 4) * 10 + (v & 0x0F);
    }

    uint8_t decToBcd(uint8_t v)
    {
        return ((v / 10) << 4) | (v % 10);
    }
} // namespace

bool YSN8900ERTC::decodeCalendar(const uint8_t buf[7], struct tm &t)
{
    // Weekday is one-hot (bit 0 = Sunday), not BCD.
    uint8_t wdayBits = buf[3] & 0x7F;
    if (wdayBits == 0)
        return false;
    int wday = 0;
    while (!(wdayBits & 1))
    {
        wdayBits >>= 1;
        wday++;
    }

    t = {};
    t.tm_sec = bcdToDec(buf[0] & 0x7F);
    t.tm_min = bcdToDec(buf[1] & 0x7F);
    t.tm_hour = bcdToDec(buf[2] & 0x3F);
    t.tm_wday = wday;
    t.tm_mday = bcdToDec(buf[4] & 0x3F);
    t.tm_mon = bcdToDec(buf[5] & 0x1F) - 1;
    t.tm_year = 2000 + bcdToDec(buf[6]) - 1900;

    // Callers index month tables with these, so reject garbage rather than pass it on.
    return t.tm_sec <= 59 && t.tm_min <= 59 && t.tm_hour <= 23 && t.tm_mday >= 1 && t.tm_mday <= 31 && t.tm_mon >= 0 &&
           t.tm_mon <= 11;
}

bool YSN8900ERTC::encodeCalendar(const struct tm &t, uint8_t buf[7])
{
    const int year = t.tm_year + 1900;
    if (year < 2000 || year > 2099 || t.tm_wday < 0 || t.tm_wday > 6)
        return false;

    buf[0] = decToBcd(t.tm_sec);
    buf[1] = decToBcd(t.tm_min);
    buf[2] = decToBcd(t.tm_hour);
    buf[3] = 1U << t.tm_wday;
    buf[4] = decToBcd(t.tm_mday);
    buf[5] = decToBcd(t.tm_mon + 1);
    buf[6] = decToBcd(year % 100);
    return true;
}

bool YSN8900ERTC::begin(TwoWire &wire, uint8_t addr)
{
    _wire = &wire;
    _addr = addr;
    uint8_t id = 0;
    if (!readRegs(REG_CHIP_ID, &id, 1) || id != CHIP_ID)
    {
        _wire = nullptr;
        return false;
    }
    return true;
}

bool YSN8900ERTC::readRegs(uint8_t reg, uint8_t *buf, size_t len)
{
    if (!_wire)
        return false;
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0 || _wire->requestFrom(_addr, (uint8_t)len) != len)
        return false;
    for (size_t i = 0; i < len; i++)
        buf[i] = _wire->read();
    return true;
}

bool YSN8900ERTC::writeRegs(uint8_t reg, const uint8_t *buf, size_t len)
{
    if (!_wire)
        return false;
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    _wire->write(buf, len);
    return _wire->endTransmission() == 0;
}

bool YSN8900ERTC::lostPower()
{
    uint8_t flag = 0;
    if (!readRegs(REG_FLAG, &flag, 1))
        return true; // cannot talk to the chip, so its time is not trustworthy either
    return flag & FLAG_VLF;
}

bool YSN8900ERTC::getTime(struct tm &t)
{
    uint8_t buf[7] = {};
    return !lostPower() && readRegs(REG_SECONDS, buf, sizeof(buf)) && decodeCalendar(buf, t);
}

bool YSN8900ERTC::setTime(const struct tm &t)
{
    uint8_t buf[7];
    uint8_t flag = 0;
    if (!encodeCalendar(t, buf) || !writeRegs(REG_SECONDS, buf, sizeof(buf)) || !readRegs(REG_FLAG, &flag, 1))
        return false;
    // Flags clear on a written 0 and ignore a written 1, so only VLF/VDET are touched here.
    flag = (uint8_t)(flag & ~(FLAG_VLF | FLAG_VDET));
    return writeRegs(REG_FLAG, &flag, 1);
}

bool YSN8900ERTC::disableInterrupts()
{
    uint8_t ext = 0, ctrl = 0;
    if (!readRegs(REG_EXTENSION, &ext, 1) || !readRegs(REG_CONTROL, &ctrl, 1))
        return false;
    ext = (uint8_t)(ext & ~EXT_TE);
    ctrl = (uint8_t)(ctrl & ~(CTRL_UIE | CTRL_TIE | CTRL_AIE));
    return writeRegs(REG_EXTENSION, &ext, 1) && writeRegs(REG_CONTROL, &ctrl, 1);
}
