#pragma once
// Shared UI helpers: palette, widget factories, change-only setters and
// text formatting used by both screens.
#include <lvgl.h>

#include <cstddef>
#include <cstdint>

#include "../core/band.h"
#include "../core/detection.h"

namespace sh {

constexpr uint32_t kColBg      = 0x06101C;
constexpr uint32_t kColPanel   = 0x0E2440;
constexpr uint32_t kColPanelHi = 0x244878;
constexpr uint32_t kColTitle   = 0xFFD000;
constexpr uint32_t kColCyan    = 0x8BE9FD;
constexpr uint32_t kColWhite   = 0xFFFFFF;
constexpr uint32_t kColGrey    = 0x8A8A8A;
constexpr uint32_t kColDim     = 0x666666;
constexpr uint32_t kColIot     = 0xC8C8C8;

lv_obj_t* makeScreen();
lv_obj_t* makePanel(lv_obj_t* parent, int32_t w, int32_t h, uint32_t color, int radius, int pad);
lv_obj_t* makeLabel(lv_obj_t* parent, const char* txt, uint32_t color, const lv_font_t* font);

// Pin a label to a single line `w` wide: text that does not fit is cut with
// "..." instead of wrapping. LVGL only adds the dots when the label height is
// fixed; with the default content-sized height it just grows a second line.
void setOneLine(lv_obj_t* lbl, int32_t w);

// LVGL repaints on every set_text / set_style call, so only call when the
// value really changed.
void setTextIfChanged(lv_obj_t* lbl, const char* txt);
// For setOneLine() labels: LVGL writes the dots into the label's own text, so
// lv_label_get_text() stops matching what we set and the overload above would
// reset (and repaint) the label on every call. Compare against our own copy
// instead; `cache` must be large enough for any text passed in.
void setTextIfChanged(lv_obj_t* lbl, const char* txt, char* cache, size_t cacheLen);
void setTextColorIfChanged(lv_obj_t* lbl, uint32_t rgb, uint32_t& cache);
void setBgColorIfChanged(lv_obj_t* obj, uint32_t rgb, lv_style_selector_t sel, uint32_t& cache);

void formatMacTail(const uint8_t mac[6], char* out, size_t len);   // "2F:3A:1C"
void formatAge(uint32_t ageMs, char* out, size_t len);             // "850 ms ago" / "2.4 s ago"
const char* radioIcon(Radio r);

// Vendor / flag chip for a detection, e.g. "[CAM? Hikvision]", "[IoT Espressif]",
// "[CAM? name:TAPO]" or "[no vendor match]". Returns the colour to draw it in.
uint32_t vendorChip(const Detection& d, char* out, size_t len);

}  // namespace sh
