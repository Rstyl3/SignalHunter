#include "ui_tick.h"

#include <Arduino.h>
#include <lvgl.h>

#include <cstring>

#include "../core/band.h"
#include "../core/button_fsm.h"
#include "../core/config.h"
#include "../core/detection_table.h"
#include "../core/list_order.h"
#include "../hw/button_task.h"
#include "../hw/led.h"
#include "../scan/detections_shared.h"
#include "ui_hunt.h"
#include "ui_list.h"

namespace sh {

namespace {

enum class UiMode : uint8_t { Hunt, List };

UiMode    g_mode = UiMode::Hunt;
ListOrder g_order;
int       g_highlight   = 0;
bool      g_listReorder = false;   // capture a fresh order on the next tick
bool      g_listRepaint = false;   // repaint on the next tick (button feedback)

// Auto mode keeps following the same device until something clearly better shows up.
bool    g_haveAuto = false;
uint8_t g_autoMac[6] = {0, 0, 0, 0, 0, 0};

// Band hysteresis state belongs to one device; reset when the device changes.
Band    g_band = Band::OutOfRange;
bool    g_haveBandMac = false;
uint8_t g_bandMac[6] = {0, 0, 0, 0, 0, 0};

Band bandFor(const Detection* d) {
    if (!d) {
        g_haveBandMac = false;
        g_band = Band::OutOfRange;
        return g_band;
    }
    if (!g_haveBandMac || !macEqual(g_bandMac, d->mac)) {
        std::memcpy(g_bandMac, d->mac, 6);
        g_haveBandMac = true;
        g_band = Band::OutOfRange;
    }
    g_band = rssiToBand(d->rssiSmoothed(), g_band);
    return g_band;
}

void logLock(const uint8_t* mac) {
    if (mac) {
        Serial.printf("[LOCK] -> %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        Serial.printf("[LOCK] cleared\n");
    }
}

void enterList() {
    g_mode = UiMode::List;
    g_highlight = 0;
    g_listReorder = true;
    g_listRepaint = true;
    listShow();
}

void enterHunt() {
    g_mode = UiMode::Hunt;
    huntShow();
}

void handleButton(ButtonEvent ev) {
    Serial.printf("[BTN] %s in %s\n", buttonEventName(ev),
                  g_mode == UiMode::Hunt ? "HUNT" : "LIST");

    if (g_mode == UiMode::Hunt) {
        // Short/double are ignored on purpose: the lock must survive fumbling
        // with the device. LONG opens the list, EXTRA_LONG returns to AUTO.
        if (ev == ButtonEvent::Long) {
            enterList();
        } else if (ev == ButtonEvent::ExtraLong) {
            detectionsClearLock();
            g_haveAuto = false;
            logLock(nullptr);
        }
        return;
    }

    const int total = g_order.count;
    switch (ev) {
        case ButtonEvent::Short:
            if (total > 0) g_highlight = (clampHighlight(g_highlight, total) + 1) % total;
            g_listRepaint = true;
            break;
        case ButtonEvent::Double:
            if (total > 0) g_highlight = (clampHighlight(g_highlight, total) - 1 + total) % total;
            g_listRepaint = true;
            break;
        case ButtonEvent::Long:
            if (total > 0) {
                const uint8_t* mac = g_order.macs[clampHighlight(g_highlight, total)];
                detectionsSetLock(mac);
                logLock(mac);
            }
            enterHunt();
            break;
        case ButtonEvent::ExtraLong:   // cancel: back to Hunt, lock untouched
            enterHunt();
            break;
        case ButtonEvent::None:
            break;
    }
}

void uiTick(lv_timer_t* /*timer*/) {
    static Detection snap[kMaxDetections];
    const uint32_t now = millis();
    const int n = detectionsSnapshot(snap, kMaxDetections);

    uint8_t lockMac[6];
    const bool haveLock = detectionsLockMac(lockMac);

    // Which device the Hunt screen and (in Hunt mode) the LED follow.
    const Detection* active = nullptr;
    if (haveLock) {
        const int i = findInSnapshot(snap, n, lockMac);
        if (i >= 0) active = &snap[i];
    } else {
        const int i = pickAuto(snap, n, g_haveAuto ? g_autoMac : nullptr, kStickyMarginDb);
        if (i >= 0) {
            active = &snap[i];
            std::memcpy(g_autoMac, snap[i].mac, 6);
            g_haveAuto = true;
        } else {
            g_haveAuto = false;
        }
    }

    // Log the locked target going silent and coming back, so a Hunt screen
    // stuck on WAITING can be told apart from a target that is still heard.
    static bool s_waiting = false;
    const bool waiting = haveLock && !active;
    if (waiting != s_waiting) {
        s_waiting = waiting;
        if (waiting) {
            Serial.printf("[LOCK] target not heard for %lu s -> WAITING\n",
                          static_cast<unsigned long>(kLockedStaleMs / 1000));
        } else if (haveLock) {
            Serial.printf("[LOCK] target back, rssi=%d\n", active->rssiSmoothed());
        }
    }

    ButtonEvent ev = ButtonEvent::None;
    while (buttonPoll(ev)) handleButton(ev);

    if (g_mode == UiMode::Hunt) {
        const Band band = bandFor(active);
        ledApply(band, now);
        huntRender(active, n, haveLock, lockMac, band, now);
        return;
    }

    // List mode: the LED follows the highlighted row so each device can be
    // "felt" while scrolling; the order only changes every kListRefreshPeriodMs.
    const bool reordered = listOrderRefresh(g_order, snap, n, now, g_listReorder);
    g_listReorder = false;
    const Detection* hlDet = nullptr;
    if (g_order.count > 0) {
        const int j = findInSnapshot(snap, n, g_order.macs[clampHighlight(g_highlight, g_order.count)]);
        if (j >= 0) hlDet = &snap[j];
    }
    ledApply(bandFor(hlDet), now);
    if (reordered || g_listRepaint) {
        g_listRepaint = false;
        listRender(snap, n, g_order, g_highlight, now);
    }
}

}  // namespace

void uiInit() {
    huntBuild();
    listBuild();
    huntShow();
    lv_timer_create(uiTick, kUiIntervalMs, nullptr);
}

}  // namespace sh
