// LVGL <-> ST7789 bridge. See LVGL_Driver.h.
#include "LVGL_Driver.h"

#include <Arduino.h>

static lv_color_t buf1[LVGL_BUF_LEN];
static lv_color_t buf2[LVGL_BUF_LEN];

// Push one rendered area to the panel. LVGL hands us RGB565 in memory order;
// the ST7789 is initialised with RAMCTRL ENDIAN=1 (little-endian), so the
// bytes can go out as-is.
static void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  LCD_addWindow(area->x1, area->y1, area->x2, area->y2,
                reinterpret_cast<uint16_t*>(px_map));
  lv_display_flush_ready(disp);
}

static uint32_t tickCb(void) {
  return static_cast<uint32_t>(millis());
}

void Lvgl_Init(void) {
  lv_init();
  lv_tick_set_cb(tickCb);

  lv_display_t* disp = lv_display_create(LVGL_WIDTH, LVGL_HEIGHT);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
}

void Timer_Loop(void) {
  lv_timer_handler();
}
