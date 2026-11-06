#include "band.h"

#include "config.h"

namespace sh {

Band rssiToBand(int rssi, Band prev) {
    const int h = kBandHysteresisDb;
    auto edge = [&](int floorDbm, Band b) -> int {
        return (prev == b) ? floorDbm - h : floorDbm + h;
    };
    if (rssi >= edge(kBandHotMin, Band::Hot))             return Band::Hot;
    if (rssi >= edge(kBandVeryCloseMin, Band::VeryClose)) return Band::VeryClose;
    if (rssi >= edge(kBandCloseMin, Band::Close))         return Band::Close;
    if (rssi >= edge(kBandNearMin, Band::Near))           return Band::Near;
    if (rssi >= edge(kBandFarMin, Band::Far))             return Band::Far;
    return Band::OutOfRange;
}

int rssiBarPercent(int rssi) {
    const int span = kBarMaxDbm - kBarMinDbm;
    int v = rssi - kBarMinDbm;
    if (v < 0) v = 0;
    if (v > span) v = span;
    return (v * 100) / span;
}

const char* bandLabel(Band b) {
    switch (b) {
        case Band::OutOfRange: return "OUT OF RANGE";
        case Band::Far:        return "FAR";
        case Band::Near:       return "NEAR";
        case Band::Close:      return "CLOSE";
        case Band::VeryClose:  return "VERY CLOSE";
        case Band::Hot:        return "HOT";
    }
    return "?";
}

uint32_t bandColor(Band b) {
    switch (b) {
        case Band::OutOfRange: return 0x444444;
        case Band::Far:        return 0xE61E32;  // crimson
        case Band::Near:       return 0xFF5A1E;  // coral
        case Band::Close:      return 0xFFA500;  // amber
        case Band::VeryClose:  return 0x82DC1E;  // lime
        case Band::Hot:        return 0x1ED75A;  // emerald
    }
    return 0xFFFFFF;
}

LedState bandLed(Band b) {
    switch (b) {
        case Band::Far:        return {{230, 30, 50},  kBrightFar,       kBlinkFarMs};
        case Band::Near:       return {{255, 90, 30},  kBrightNear,      kBlinkNearMs};
        case Band::Close:      return {{255, 165, 0},  kBrightClose,     kBlinkCloseMs};
        case Band::VeryClose:  return {{130, 220, 30}, kBrightVeryClose, 0};
        case Band::Hot:        return {{30, 215, 90},  kBrightHot,       0};
        case Band::OutOfRange: break;
    }
    return {{0, 0, 0}, 0, 0};
}

bool ledOn(const LedState& s, uint32_t nowMs) {
    if (s.blinkHalfMs == 0) return true;
    return ((nowMs / s.blinkHalfMs) % 2) == 0;
}

Rgb scaleRgb(Rgb c, uint8_t brightnessPercent) {
    const uint16_t scale = static_cast<uint16_t>(brightnessPercent) * 255 / 100;
    return {static_cast<uint8_t>((static_cast<uint16_t>(c.r) * scale) / 255),
            static_cast<uint8_t>((static_cast<uint16_t>(c.g) * scale) / 255),
            static_cast<uint8_t>((static_cast<uint16_t>(c.b) * scale) / 255)};
}

}  // namespace sh
