#pragma once
// LVGL <-> ST7789 bridge for the Waveshare ESP32-C6-LCD-1.47 (inherited from
// blewatch, reduced). Owns the LVGL display object, its draw buffers and the
// tick source. Nothing application-specific lives here.

#ifndef LV_CONF_INCLUDE_SIMPLE
#define LV_CONF_INCLUDE_SIMPLE 1   // Arduino IDE path; PlatformIO sets it in platformio.ini
#endif

#include <lvgl.h>
#include "Display_ST7789.h"

#define LVGL_WIDTH    LCD_WIDTH
#define LVGL_HEIGHT   LCD_HEIGHT
#define LVGL_BUF_LEN  (LVGL_WIDTH * LVGL_HEIGHT / 20)   // 1/20 screen per partial buffer

void Lvgl_Init(void);    // lv_init + display + buffers + tick; call after LCD_Init()
void Timer_Loop(void);   // call from loop(): runs lv_timer_handler()
