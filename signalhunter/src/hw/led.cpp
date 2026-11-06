#include "led.h"

#include <Adafruit_NeoPixel.h>

#include "../core/config.h"

namespace sh {

namespace {

Adafruit_NeoPixel g_rgb(1, kRgbPin, NEO_RGB + NEO_KHZ800);
uint32_t          g_last = 0xFFFFFFFFu;   // last packed colour written

}  // namespace

void ledInit() {
    g_rgb.begin();
    g_rgb.setBrightness(255);
    g_rgb.clear();
    g_rgb.show();
    g_last = 0;
}

void ledApply(Band b, uint32_t nowMs) {
    const LedState s = bandLed(b);
    const Rgb c = ledOn(s, nowMs) ? scaleRgb(s.color, s.brightness) : Rgb{0, 0, 0};
    const uint32_t packed = (static_cast<uint32_t>(c.r) << 16) |
                            (static_cast<uint32_t>(c.g) << 8) | c.b;
    if (packed == g_last) return;
    g_last = packed;
    g_rgb.setPixelColor(0, Adafruit_NeoPixel::Color(c.r, c.g, c.b));
    g_rgb.show();
}

}  // namespace sh
