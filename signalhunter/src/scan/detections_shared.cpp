#include "detections_shared.h"

#include <Arduino.h>

#include <cstring>

namespace sh {

namespace {

portMUX_TYPE   g_mux = portMUX_INITIALIZER_UNLOCKED;
DetectionTable g_table;

const char* radioName(Radio r) {
    switch (r) {
        case Radio::Ble:     return "BLE";
        case Radio::WifiSta: return "STA";
        case Radio::WifiAp:  return "AP";
    }
    return "?";
}

}  // namespace

void detectionsNote(const Sighting& s) {
    const uint32_t now = millis();
    portENTER_CRITICAL(&g_mux);
    g_table.note(s, now);
    portEXIT_CRITICAL(&g_mux);
}

int detectionsSnapshot(Detection* out, int maxOut) {
    const uint32_t now = millis();
    portENTER_CRITICAL(&g_mux);
    const int n = g_table.copyFresh(out, maxOut, now);
    portEXIT_CRITICAL(&g_mux);
    sortSnapshot(out, n);
    return n;
}

void detectionsSetLock(const uint8_t mac[6]) {
    portENTER_CRITICAL(&g_mux);
    g_table.setLock(mac);
    portEXIT_CRITICAL(&g_mux);
}

void detectionsClearLock() {
    portENTER_CRITICAL(&g_mux);
    g_table.clearLock();
    portEXIT_CRITICAL(&g_mux);
}

bool detectionsLockMac(uint8_t out[6]) {
    portENTER_CRITICAL(&g_mux);
    const bool have = g_table.hasLock();
    if (have) std::memcpy(out, g_table.lockMac(), 6);
    portEXIT_CRITICAL(&g_mux);
    return have;
}

uint8_t detectionsLockedWifiChannel() {
    uint8_t ch = 0;
    portENTER_CRITICAL(&g_mux);
    if (g_table.hasLock()) {
        for (int i = 0; i < kMaxDetections; i++) {
            const Detection& d = g_table.at(i);
            if (!d.used || !g_table.isLocked(d)) continue;
            if (d.radio != Radio::Ble) ch = d.channel;
            break;
        }
    }
    portEXIT_CRITICAL(&g_mux);
    return ch;
}

void detectionsAnnounceNew() {
    static Detection buf[8];   // scanner task only
    portENTER_CRITICAL(&g_mux);
    const int n = g_table.takeUnannounced(buf, 8);
    portEXIT_CRITICAL(&g_mux);
    for (int i = 0; i < n; i++) {
        const Detection& d = buf[i];
        Serial.printf("[NEW] %s %02X:%02X:%02X:%02X:%02X:%02X rssi=%d ch=%u vendor=%s%s%s%s\n",
                      radioName(d.radio),
                      d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5],
                      d.rssi, d.channel,
                      d.vendor ? d.vendor : "(unknown)",
                      d.tier == VendorTier::Camera ? " [camera vendor]" :
                      d.tier == VendorTier::Iot    ? " [iot vendor]" : "",
                      d.name[0] ? " name=" : "",
                      d.name[0] ? d.name : "");
    }
}

}  // namespace sh
