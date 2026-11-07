#include "ui_hunt.h"

#include <cstdio>

#include "../core/config.h"
#include "ui_common.h"

namespace sh {

namespace {

lv_obj_t* g_screen      = nullptr;
lv_obj_t* g_modeChip    = nullptr;
lv_obj_t* g_targetLabel = nullptr;
lv_obj_t* g_vendorLabel = nullptr;
lv_obj_t* g_bandLabel   = nullptr;
lv_obj_t* g_rssiLabel   = nullptr;
lv_obj_t* g_bar         = nullptr;
lv_obj_t* g_barLabel    = nullptr;
lv_obj_t* g_metaLabel   = nullptr;

uint32_t g_modeColor   = 0;
uint32_t g_vendorColor = 0;
uint32_t g_bandColor   = 0;
uint32_t g_barColor    = 0;
uint32_t g_lastMetaMs  = 0;

}  // namespace

void huntBuild() {
    g_screen = makeScreen();
    lv_obj_set_style_pad_all(g_screen, 6, 0);

    // Header: title + AUTO/LOCK chip
    lv_obj_t* header = makePanel(g_screen, LV_PCT(100), 34, kColPanel, 6, 6);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t* title = makeLabel(header, "SignalHunter", kColTitle, &lv_font_montserrat_14);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);
    g_modeChip = makeLabel(header, "AUTO", kColCyan, &lv_font_montserrat_14);
    lv_obj_align(g_modeChip, LV_ALIGN_RIGHT_MID, -4, 0);
    g_modeColor = kColCyan;

    // Target panel
    lv_obj_t* panel = makePanel(g_screen, LV_PCT(100), 96, kColPanel, 8, 8);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 42);
    makeLabel(panel, "TARGET", kColCyan, &lv_font_montserrat_14);
    g_targetLabel = makeLabel(panel, "no target", kColWhite, &lv_font_montserrat_14);
    lv_obj_set_width(g_targetLabel, LV_PCT(100));
    lv_label_set_long_mode(g_targetLabel, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(g_targetLabel, LV_ALIGN_LEFT_MID, 0, 0);
    g_vendorLabel = makeLabel(panel, "", kColTitle, &lv_font_montserrat_14);
    lv_obj_set_width(g_vendorLabel, LV_PCT(100));
    lv_label_set_long_mode(g_vendorLabel, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(g_vendorLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    g_vendorColor = kColTitle;

    // Big band label
    g_bandLabel = makeLabel(g_screen, "OUT OF RANGE", kColDim, &lv_font_montserrat_20);
    lv_obj_align(g_bandLabel, LV_ALIGN_TOP_MID, 0, 148);
    lv_obj_set_style_text_align(g_bandLabel, LV_TEXT_ALIGN_CENTER, 0);
    g_bandColor = kColDim;

    // RSSI numeric
    g_rssiLabel = makeLabel(g_screen, "-- dBm", kColCyan, &lv_font_montserrat_20);
    lv_obj_align(g_rssiLabel, LV_ALIGN_TOP_MID, 0, 184);
    lv_obj_set_style_text_align(g_rssiLabel, LV_TEXT_ALIGN_CENTER, 0);

    // Bar with the percentage centred on it
    g_bar = lv_bar_create(g_screen);
    lv_bar_set_range(g_bar, 0, 100);
    lv_obj_set_size(g_bar, 150, 18);
    lv_obj_align(g_bar, LV_ALIGN_TOP_MID, 0, 222);
    lv_obj_set_style_bg_color(g_bar, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_bar, LV_OPA_40, 0);
    lv_obj_set_style_radius(g_bar, 5, 0);
    lv_obj_set_style_bg_color(g_bar, lv_color_hex(kColDim), LV_PART_INDICATOR);
    g_barColor = kColDim;
    g_barLabel = makeLabel(g_bar, "--", kColWhite, &lv_font_montserrat_14);
    lv_obj_center(g_barLabel);

    // Footer: radio / channel / freshness
    g_metaLabel = makeLabel(g_screen, "", kColCyan, &lv_font_montserrat_14);
    lv_obj_align(g_metaLabel, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_text_align(g_metaLabel, LV_TEXT_ALIGN_CENTER, 0);
}

void huntShow() {
    lv_screen_load(g_screen);
}

void huntRender(const Detection* active, int totalCount, bool haveLock,
                const uint8_t* lockMac, Band band, uint32_t nowMs) {
    char buf[96];

    setTextIfChanged(g_modeChip, haveLock ? "LOCK" : "AUTO");
    setTextColorIfChanged(g_modeChip, haveLock ? kColTitle : kColCyan, g_modeColor);

    if (!active) {
        // Locked but silent -> WAITING (lock is kept). Nothing locked and
        // nothing in range -> plain no-signal state.
        if (haveLock) {
            char tail[16];
            formatMacTail(lockMac, tail, sizeof(tail));
            snprintf(buf, sizeof(buf), "LOCKED " LV_SYMBOL_BULLET " %s", tail);
            setTextIfChanged(g_targetLabel, buf);
            setTextIfChanged(g_vendorLabel, "[waiting for signal]");
            setTextColorIfChanged(g_vendorLabel, kColTitle, g_vendorColor);
            setTextIfChanged(g_bandLabel, "WAITING");
        } else {
            setTextIfChanged(g_targetLabel, "no signal");
            setTextIfChanged(g_vendorLabel, "");
            setTextIfChanged(g_bandLabel, "OUT OF RANGE");
        }
        setTextColorIfChanged(g_bandLabel, kColDim, g_bandColor);
        setTextIfChanged(g_rssiLabel, "-- dBm");
        lv_bar_set_value(g_bar, 0, LV_ANIM_OFF);
        setTextIfChanged(g_barLabel, "--");
        setBgColorIfChanged(g_bar, kColDim, LV_PART_INDICATOR, g_barColor);
        snprintf(buf, sizeof(buf), "%d nearby", totalCount);
        setTextIfChanged(g_metaLabel, buf);
        g_lastMetaMs = 0;
        return;
    }

    // Target line: name, else radio icon + MAC tail.
    if (active->name[0]) {
        snprintf(buf, sizeof(buf), "%s", active->name);
    } else {
        char tail[16];
        formatMacTail(active->mac, tail, sizeof(tail));
        snprintf(buf, sizeof(buf), "%s %s", radioIcon(active->radio), tail);
    }
    setTextIfChanged(g_targetLabel, buf);

    const uint32_t chipColor = vendorChip(*active, buf, sizeof(buf));
    setTextIfChanged(g_vendorLabel, buf);
    setTextColorIfChanged(g_vendorLabel, chipColor, g_vendorColor);

    setTextIfChanged(g_bandLabel, bandLabel(band));
    setTextColorIfChanged(g_bandLabel, bandColor(band), g_bandColor);

    const int rssi = active->rssiSmoothed();
    snprintf(buf, sizeof(buf), "%d dBm", rssi);
    setTextIfChanged(g_rssiLabel, buf);

    const int pct = rssiBarPercent(rssi);
    if (lv_bar_get_value(g_bar) != pct) lv_bar_set_value(g_bar, pct, LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%d%%", pct);
    setTextIfChanged(g_barLabel, buf);
    setBgColorIfChanged(g_bar, bandColor(band), LV_PART_INDICATOR, g_barColor);

    // Freshness footer, throttled: it changes every tick otherwise.
    if (g_lastMetaMs == 0 || nowMs - g_lastMetaMs >= kAgeTickerMs) {
        g_lastMetaMs = nowMs;
        char age[24];
        formatAge(nowMs - active->lastSeenMs, age, sizeof(age));
        if (active->radio == Radio::Ble) {
            snprintf(buf, sizeof(buf), "%s\n%s", radioIcon(active->radio), age);
        } else {
            snprintf(buf, sizeof(buf), "%s ch%u\n%s", radioIcon(active->radio),
                     active->channel, age);
        }
        setTextIfChanged(g_metaLabel, buf);
    }
}

}  // namespace sh
