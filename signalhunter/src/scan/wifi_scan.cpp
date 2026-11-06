#include "wifi_scan.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "../core/config.h"
#include "../core/frame80211.h"
#include "detections_shared.h"

namespace sh {

namespace {

volatile uint32_t g_framesSweep = 0;
volatile uint32_t g_framesTotal = 0;

// Runs in the Wi-Fi task for every captured frame: parse, then one short
// critical section to record the sighting. No logging here.
void promiscCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!buf) return;
    if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;
    const wifi_promiscuous_pkt_t* pkt = static_cast<const wifi_promiscuous_pkt_t*>(buf);

    ParsedFrame pf;
    if (!parseFrame(pkt->payload, static_cast<int>(pkt->rx_ctrl.sig_len), pf)) return;

    g_framesSweep = g_framesSweep + 1;
    g_framesTotal = g_framesTotal + 1;

    const Sighting s{pf.addr,
                     pkt->rx_ctrl.rssi,
                     pf.hasSsid ? pf.ssid : nullptr,
                     pf.role == FrameRole::Ap ? Radio::WifiAp : Radio::WifiSta,
                     static_cast<uint8_t>(pkt->rx_ctrl.channel)};
    detectionsNote(s);
}

}  // namespace

void wifiScanInit() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);   // modem sleep would blind the receiver between beacons
    const esp_err_t err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
        Serial.printf("[WIFI] start err=%d\n", static_cast<int>(err));
    }

    // The IDF default regulatory domain only allows channels 1..11, so
    // tuning to 12/13 would silently fail. We only ever listen, so open the
    // full 2.4 GHz range that the sweep covers.
    wifi_country_t country = {};
    country.cc[0] = '0';
    country.cc[1] = '1';
    country.cc[2] = '\0';
    country.schan = kPromiscFirstChan;
    country.nchan = static_cast<uint8_t>(kPromiscLastChan - kPromiscFirstChan + 1);
    country.policy = WIFI_COUNTRY_POLICY_MANUAL;
    esp_wifi_set_country(&country);

    static const wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA,
    };
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&promiscCb);
}

void wifiSweepOnce() {
    g_framesSweep = 0;
    const esp_err_t err = esp_wifi_set_promiscuous(true);
    if (err != ESP_OK) {
        Serial.printf("[PROMISC] enable failed err=%d\n", static_cast<int>(err));
        return;
    }

    const uint8_t lockedCh = detectionsLockedWifiChannel();
    int chanErrors = 0;
    for (uint8_t ch = kPromiscFirstChan; ch <= kPromiscLastChan; ch++) {
        if (esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE) != ESP_OK) chanErrors++;
        const uint32_t dwell = (ch == lockedCh) ? kPromiscLockedDwellMs : kPromiscDwellMs;
        vTaskDelay(pdMS_TO_TICKS(dwell));
    }

    esp_wifi_set_promiscuous(false);
    Serial.printf("[PROMISC] sweep: %lu frames (total %lu)\n",
                  static_cast<unsigned long>(g_framesSweep),
                  static_cast<unsigned long>(g_framesTotal));
    if (chanErrors) Serial.printf("[PROMISC] %d channel switches failed\n", chanErrors);
}

}  // namespace sh
