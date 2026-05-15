// SignalHunter — hidden-camera RF tracker for Waveshare ESP32-C6-LCD-1.47.
//
// Boot path:
//   setup()
//     -> LCD_Init()        (Display_ST7789.cpp — SPI + ST7789 init + backlight)
//     -> Lvgl_Init()       (LVGL_Driver.cpp  — LVGL display/input + tick timer;
//                                              calls Signalhunter_Init() at the end)
//        -> Signalhunter_Init()  (signalhunter.cpp — NeoPixel, button, scanner task,
//                                                    UI screens, LVGL tick callback)
//   loop()
//     -> Timer_Loop()      (LVGL_Driver.cpp — lv_timer_handler())
//
// All scanning runs in a separate FreeRTOS task pinned to core 0; the LVGL
// loop runs in the Arduino loop() on core 1.

#include "Display_ST7789.h"
#include "LVGL_Driver.h"

void setup() {
    Serial.begin(115200);
    LCD_Init();
    Lvgl_Init();
}

void loop() {
    Timer_Loop();
    delay(5);
}
