# Notice

## Inherited files

The files below were taken from the `blewatch` sketch in
[PierreGode/waveshareesp32c6lcd](https://github.com/PierreGode/waveshareesp32c6lcd) and adapted
here. That repository publishes no licence file and its README states no terms. The display
driver there appears to derive from Waveshare's demo code for the
[ESP32-C6-LCD-1.47](https://www.waveshare.com/wiki/ESP32-C6-LCD-1.47).

| File | Origin | Changes here |
|------|--------|--------------|
| `signalhunter/Display_ST7789.cpp`, `.h` | blewatch (Waveshare ST7789 SPI driver) | backlight fix, cleanup |
| `signalhunter/LVGL_Driver.cpp`, `.h` | blewatch (LVGL to ST7789 bridge) | reduced to display + tick |
| `signalhunter/lv_conf.h` | blewatch (LVGL configuration) | reduced |

If you need clear terms for those files, ask the upstream author, or replace them with LVGL's
built-in ST7789 driver (`LV_USE_ST7789`), which ships under LVGL's MIT licence.

## Third-party libraries

LVGL, Adafruit NeoPixel and arduino-esp32 are used under their own licences (MIT, LGPL-3.0 and
LGPL-2.1 respectively) and are not redistributed in this repository.
