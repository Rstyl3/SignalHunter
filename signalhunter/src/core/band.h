#pragma once
// RSSI band classification (with hysteresis), bar scaling, and the LED / LCD
// appearance of each band. Pure: no hardware access.
#include <cstdint>

namespace sh {

enum class Band : uint8_t { OutOfRange = 0, Far, Near, Close, VeryClose, Hot };

struct Rgb { uint8_t r, g, b; };

struct LedState {
    Rgb      color;
    uint8_t  brightness;   // percent
    uint32_t blinkHalfMs;  // 0 = steady
};

// Classify with hysteresis: the current band's floor is lowered by the
// hysteresis, every other floor is raised by it, so RSSI jitter at an edge
// does not flicker between bands.
Band rssiToBand(int rssi, Band prev);

// Map RSSI to 0..100 for the bar widget (kBarMinDbm -> 0, kBarMaxDbm -> 100).
int rssiBarPercent(int rssi);

const char* bandLabel(Band b);
uint32_t    bandColor(Band b);   // 0xRRGGBB for the LCD, mirrors the LED palette
LedState    bandLed(Band b);
bool        ledOn(const LedState& s, uint32_t nowMs);   // blink phase for this instant
Rgb         scaleRgb(Rgb c, uint8_t brightnessPercent);

}  // namespace sh
