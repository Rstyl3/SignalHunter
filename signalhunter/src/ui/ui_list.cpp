#include "ui_list.h"

#include <cstdio>

#include "../core/band.h"
#include "../core/config.h"
#include "../core/detection_table.h"
#include "ui_common.h"

namespace sh {

namespace {

constexpr int kRowH    = 28;
constexpr int kRowGap  = 2;
constexpr int kHeaderH = 40;
constexpr int kFooterH = 56;
constexpr size_t kTextMax = 96;   // scratch buffer + label text caches

struct Row {
    lv_obj_t* panel = nullptr;
    lv_obj_t* name  = nullptr;
    lv_obj_t* rssi  = nullptr;
    lv_obj_t* bar   = nullptr;
    uint32_t  panelColor = 0;
    uint32_t  textColor  = 0;
    uint32_t  barColor   = 0;
    bool      scrolling  = false;
    char      nameText[kTextMax] = "";   // last text set on `name`, see setRowName
};

lv_obj_t* g_screen   = nullptr;
lv_obj_t* g_counts   = nullptr;
lv_obj_t* g_position = nullptr;
lv_obj_t* g_foot1    = nullptr;
lv_obj_t* g_foot2    = nullptr;
lv_obj_t* g_foot3    = nullptr;
uint32_t  g_foot1Color = 0;
char      g_foot1Text[kTextMax];
Row       g_rows[kListVisibleRows];

void setRowTextColor(Row& r, uint32_t rgb) {
    if (r.textColor == rgb) return;
    r.textColor = rgb;
    lv_obj_set_style_text_color(r.name, lv_color_hex(rgb), 0);
    lv_obj_set_style_text_color(r.rssi, lv_color_hex(rgb), 0);
}

void setRowScrolling(Row& r, bool scrolling) {
    if (r.scrolling == scrolling) return;
    r.scrolling = scrolling;
    lv_label_set_long_mode(r.name, scrolling ? LV_LABEL_LONG_MODE_SCROLL_CIRCULAR
                                             : LV_LABEL_LONG_MODE_DOTS);
}

void setRowName(Row& r, const char* txt) {
    setTextIfChanged(r.name, txt, r.nameText, sizeof(r.nameText));
}

}  // namespace

void listBuild() {
    g_screen = makeScreen();
    lv_obj_set_style_pad_all(g_screen, 4, 0);

    lv_obj_t* header = makePanel(g_screen, LV_PCT(100), kHeaderH, kColPanel, 6, 4);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    g_counts = makeLabel(header, LV_SYMBOL_BLUETOOTH " 0  " LV_SYMBOL_WIFI " 0  " LV_SYMBOL_EYE_OPEN " 0",
                         kColTitle, &lv_font_montserrat_14);
    lv_obj_align(g_counts, LV_ALIGN_TOP_LEFT, 2, 0);
    g_position = makeLabel(header, "0/0", kColCyan, &lv_font_montserrat_14);
    lv_obj_align(g_position, LV_ALIGN_BOTTOM_RIGHT, -2, 0);

    for (int i = 0; i < kListVisibleRows; i++) {
        Row& r = g_rows[i];
        r.panel = makePanel(g_screen, LV_PCT(100), kRowH, kColPanel, 4, 3);
        lv_obj_align(r.panel, LV_ALIGN_TOP_LEFT, 0, kHeaderH + 4 + i * (kRowH + kRowGap));
        r.panelColor = kColPanel;

        r.name = makeLabel(r.panel, "", kColWhite, &lv_font_montserrat_14);
        setOneLine(r.name, 122);
        lv_obj_align(r.name, LV_ALIGN_LEFT_MID, 0, -2);

        r.rssi = makeLabel(r.panel, "", kColWhite, &lv_font_montserrat_14);
        lv_obj_align(r.rssi, LV_ALIGN_RIGHT_MID, 0, -2);
        r.textColor = kColWhite;

        r.bar = lv_bar_create(r.panel);
        lv_bar_set_range(r.bar, 0, 100);
        lv_obj_set_size(r.bar, LV_PCT(100), 3);
        lv_obj_align(r.bar, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(r.bar, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(r.bar, LV_OPA_40, 0);
        lv_obj_set_style_bg_color(r.bar, lv_color_hex(kColDim), LV_PART_INDICATOR);
        r.barColor = kColDim;
    }

    lv_obj_t* footer = makePanel(g_screen, LV_PCT(100), kFooterH, kColPanel, 6, 4);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
    g_foot1 = makeLabel(footer, "", kColCyan, &lv_font_montserrat_14);
    setOneLine(g_foot1, LV_PCT(100));
    lv_obj_align(g_foot1, LV_ALIGN_TOP_LEFT, 0, 0);
    g_foot1Color = kColCyan;
    g_foot2 = makeLabel(footer, "", kColWhite, &lv_font_montserrat_14);
    lv_obj_align(g_foot2, LV_ALIGN_TOP_LEFT, 0, 16);
    g_foot3 = makeLabel(footer, "", kColGrey, &lv_font_montserrat_14);
    lv_obj_align(g_foot3, LV_ALIGN_TOP_LEFT, 0, 32);
}

void listShow() {
    lv_screen_load(g_screen);
}

void listRender(const Detection* snap, int n, const ListOrder& order,
                int highlight, uint32_t nowMs) {
    char buf[kTextMax];
    const int total = order.count;

    // Header: per-radio counts, flagged count, position in the list.
    int bt = 0, wifi = 0, cam = 0;
    for (int i = 0; i < total; i++) {
        const int j = findInSnapshot(snap, n, order.macs[i]);
        if (j < 0) continue;
        if (snap[j].radio == Radio::Ble) bt++; else wifi++;
        if (snap[j].flagged()) cam++;
    }
    snprintf(buf, sizeof(buf), LV_SYMBOL_BLUETOOTH " %d  " LV_SYMBOL_WIFI " %d  " LV_SYMBOL_EYE_OPEN " %d",
             bt, wifi, cam);
    setTextIfChanged(g_counts, buf);

    const int hl = clampHighlight(highlight, total);
    snprintf(buf, sizeof(buf), "%d/%d", total ? hl + 1 : 0, total);
    setTextIfChanged(g_position, buf);

    const int scroll = listScroll(hl, total, kListVisibleRows);
    const Detection* highlighted = nullptr;

    for (int row = 0; row < kListVisibleRows; row++) {
        Row& r = g_rows[row];
        const int idx = row + scroll;
        if (idx >= total) {
            setRowName(r, "");
            setTextIfChanged(r.rssi, "");
            lv_bar_set_value(r.bar, 0, LV_ANIM_OFF);
            setBgColorIfChanged(r.panel, kColPanel, 0, r.panelColor);
            setRowScrolling(r, false);
            continue;
        }
        const bool isHl = (idx == hl);
        setBgColorIfChanged(r.panel, isHl ? kColPanelHi : kColPanel, 0, r.panelColor);
        setRowScrolling(r, isHl);

        char tail[16];
        formatMacTail(order.macs[idx], tail, sizeof(tail));
        const int j = findInSnapshot(snap, n, order.macs[idx]);
        if (j < 0) {
            // Aged out since the order was frozen: keep the row, grey it.
            snprintf(buf, sizeof(buf), "  %s", tail);
            setRowName(r, buf);
            setTextIfChanged(r.rssi, "--");
            setRowTextColor(r, kColGrey);
            lv_bar_set_value(r.bar, 0, LV_ANIM_OFF);
            setBgColorIfChanged(r.bar, kColDim, LV_PART_INDICATOR, r.barColor);
            continue;
        }
        const Detection& d = snap[j];
        if (isHl) highlighted = &d;

        const char* flag = d.flagged() ? LV_SYMBOL_EYE_OPEN : " ";
        if (d.name[0]) {
            snprintf(buf, sizeof(buf), "%s%s %s", flag, radioIcon(d.radio), d.name);
        } else if (d.vendor) {
            snprintf(buf, sizeof(buf), "%s%s %s %s", flag, radioIcon(d.radio), d.vendor, tail);
        } else {
            snprintf(buf, sizeof(buf), "%s%s %s", flag, radioIcon(d.radio), tail);
        }
        setRowName(r, buf);
        const int rssi = d.rssiSmoothed();
        snprintf(buf, sizeof(buf), "%d", rssi);
        setTextIfChanged(r.rssi, buf);
        const bool fading = (nowMs - d.lastSeenMs) > kListFadeMs;
        setRowTextColor(r, fading ? kColGrey : kColWhite);

        const int pct = rssiBarPercent(rssi);
        if (lv_bar_get_value(r.bar) != pct) lv_bar_set_value(r.bar, pct, LV_ANIM_OFF);
        setBgColorIfChanged(r.bar, bandColor(rssiToBand(rssi, Band::OutOfRange)),
                            LV_PART_INDICATOR, r.barColor);
    }

    // Footer: details of the highlighted device.
    if (!highlighted) {
        setTextIfChanged(g_foot1, total ? "(device gone)" : "no devices in range",
                         g_foot1Text, sizeof(g_foot1Text));
        setTextColorIfChanged(g_foot1, kColGrey, g_foot1Color);
        setTextIfChanged(g_foot2, "");
        setTextIfChanged(g_foot3, "");
        return;
    }
    const Detection& d = *highlighted;
    const uint32_t chipColor = vendorChip(d, buf, sizeof(buf));
    setTextIfChanged(g_foot1, buf, g_foot1Text, sizeof(g_foot1Text));
    setTextColorIfChanged(g_foot1, chipColor, g_foot1Color);

    if (d.radio == Radio::Ble) {
        snprintf(buf, sizeof(buf), "%s BLE   %d dBm", radioIcon(d.radio), d.rssiSmoothed());
    } else {
        snprintf(buf, sizeof(buf), "%s %s ch%u   %d dBm", radioIcon(d.radio),
                 d.radio == Radio::WifiAp ? "AP" : "STA", d.channel, d.rssiSmoothed());
    }
    setTextIfChanged(g_foot2, buf);

    char age[24], tail[16];
    formatAge(nowMs - d.lastSeenMs, age, sizeof(age));
    formatMacTail(d.mac, tail, sizeof(tail));
    snprintf(buf, sizeof(buf), "%s   %s", age, tail);
    setTextIfChanged(g_foot3, buf);
}

}  // namespace sh
