#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Entry point called from Lvgl_Init() in LVGL_Driver.cpp after the LVGL
// display/input devices are registered. Owns: button task, scanner task,
// LVGL UI screens, WS2812 LED driver.
void Signalhunter_Init(void);

#ifdef __cplusplus
}
#endif
