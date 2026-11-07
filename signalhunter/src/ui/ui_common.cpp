#include "ui_common.h"

#include <cstdio>
#include <cstring>

namespace sh {

lv_obj_t* makeScreen() {
    lv_obj_t* s = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s, lv_color_hex(kColBg), 0);
    lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    return s;
}

lv_obj_t* makePanel(lv_obj_t* parent, int32_t w, int32_t h, uint32_t color, int radius, int pad) {
    lv_obj_t* p = lv_obj_create(parent);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_color(p, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_radius(p, radius, 0);
    lv_obj_set_style_pad_all(p, pad, 0);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* txt, uint32_t color, const lv_font_t* font) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, txt);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
    if (font) lv_obj_set_style_text_font(lbl, font, 0);
    return lbl;
}

void setOneLine(lv_obj_t* lbl, int32_t w) {
    const lv_font_t* font = lv_obj_get_style_text_font(lbl, LV_PART_MAIN);
    lv_obj_set_size(lbl, w, lv_font_get_line_height(font));
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_MODE_DOTS);
}

void setTextIfChanged(lv_obj_t* lbl, const char* txt) {
    const char* cur = lv_label_get_text(lbl);
    if (cur && std::strcmp(cur, txt) == 0) return;
    lv_label_set_text(lbl, txt);
}

void setTextIfChanged(lv_obj_t* lbl, const char* txt, char* cache, size_t cacheLen) {
    if (std::strcmp(cache, txt) == 0) return;
    snprintf(cache, cacheLen, "%s", txt);
    lv_label_set_text(lbl, txt);
}

void setTextColorIfChanged(lv_obj_t* lbl, uint32_t rgb, uint32_t& cache) {
    if (cache == rgb) return;
    cache = rgb;
    lv_obj_set_style_text_color(lbl, lv_color_hex(rgb), 0);
}

void setBgColorIfChanged(lv_obj_t* obj, uint32_t rgb, lv_style_selector_t sel, uint32_t& cache) {
    if (cache == rgb) return;
    cache = rgb;
    lv_obj_set_style_bg_color(obj, lv_color_hex(rgb), sel);
}

void formatMacTail(const uint8_t mac[6], char* out, size_t len) {
    snprintf(out, len, "%02X:%02X:%02X", mac[3], mac[4], mac[5]);
}

void formatAge(uint32_t ageMs, char* out, size_t len) {
    if (ageMs < 1000) {
        snprintf(out, len, "%lu ms ago", static_cast<unsigned long>(ageMs));
    } else {
        snprintf(out, len, "%lu.%lu s ago",
                 static_cast<unsigned long>(ageMs / 1000),
                 static_cast<unsigned long>((ageMs % 1000) / 100));
    }
}

const char* radioIcon(Radio r) {
    switch (r) {
        case Radio::Ble:     return LV_SYMBOL_BLUETOOTH;
        case Radio::WifiSta: return LV_SYMBOL_HOME;
        case Radio::WifiAp:  return LV_SYMBOL_WIFI;
    }
    return "?";
}

uint32_t vendorChip(const Detection& d, char* out, size_t len) {
    if (d.tier == VendorTier::Camera) {
        snprintf(out, len, LV_SYMBOL_EYE_OPEN " [CAM? %s]", d.vendor);
        return kColTitle;
    }
    if (d.nameHit) {
        snprintf(out, len, LV_SYMBOL_EYE_OPEN " [CAM? name:%s]", d.nameHit);
        return kColTitle;
    }
    if (d.tier == VendorTier::Iot) {
        snprintf(out, len, LV_SYMBOL_EYE_OPEN " [IoT %s]", d.vendor);
        return kColIot;
    }
    snprintf(out, len, "[no vendor match]");
    return kColCyan;
}

}  // namespace sh
