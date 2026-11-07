// SignalHunter — hidden-camera RF tracker for Waveshare ESP32-C6-LCD-1.47.
//
// Boot path:
//   setup()
//     -> LCD_Init()           (Display_ST7789.cpp — SPI + ST7789 init, backlight off)
//     -> Lvgl_Init()          (LVGL_Driver.cpp   — LVGL display, draw buffers, tick)
//     -> Signalhunter_Init()  (src/app.cpp       — LED, UI screens, scanner + button tasks)
//     -> first frame painted, then backlight on
//   loop()
//     -> Timer_Loop()         (LVGL_Driver.cpp   — lv_timer_handler())
//
// The ESP32-C6 is single-core. The scanner and button tasks are ordinary
// FreeRTOS tasks that share the one core with the Arduino loop task, which
// runs LVGL. Everything application-specific lives under src/.

#include "Display_ST7789.h"
#include "LVGL_Driver.h"
#include "src/app.h"
#include "src/core/config.h"

void setup() {
    Serial.begin(115200);
    LCD_Init();
    Lvgl_Init();
    Signalhunter_Init();
    lv_refr_now(nullptr);                  // paint the first frame before lighting the panel
    Set_Backlight(sh::kBacklightPercent);
}

void loop() {
    Timer_Loop();
    delay(5);
}
