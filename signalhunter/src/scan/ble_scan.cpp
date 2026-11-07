#include "ble_scan.h"

#include <Arduino.h>
#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>

#include <cstdio>

#include "../core/config.h"
#include "detections_shared.h"

namespace sh {

namespace {

class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    // Runs in the BLE host task.
    void onResult(BLEAdvertisedDevice dev) override {
        const String addr = dev.getAddress().toString();
        unsigned int b[6];
        if (sscanf(addr.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
                   &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) {
            return;
        }
        uint8_t mac[6];
        for (int i = 0; i < 6; i++) mac[i] = static_cast<uint8_t>(b[i]);
        const String name = dev.getName();
        const Sighting s{mac, dev.getRSSI(), name.length() ? name.c_str() : nullptr,
                         Radio::Ble, 0};
        detectionsNote(s);
    }
};

ScanCallbacks g_callbacks;
BLEScan*      g_scan = nullptr;

// Session bookkeeping, owned by the scanner task. `g_running` is also
// cleared from the BLE host task when a session completes.
volatile bool g_running        = false;
bool          g_startFailed    = false;
uint32_t      g_sessionStartMs = 0;
uint32_t      g_nextStartMs    = 0;   // earliest start of the next session (back-off after a failure)

constexpr uint32_t kSessionGraceMs = 2000;   // overrun after which a session counts as stuck
constexpr uint32_t kStartRetryMs   = 1000;

// Called by the library when the session's duration has elapsed (BLE host
// task) or from stop() (scanner task). By then it has reported every device
// seen without a scan response; the scanner task starts the next session.
void onScanComplete(BLEScanResults /*results*/) { g_running = false; }

}  // namespace

void bleScanInit() {
    BLEDevice::init("");
    g_scan = BLEDevice::getScan();
    // Report duplicates: without both of these the controller and the library
    // would each report a device once per session and RSSI would freeze.
    g_scan->setDuplicateFilter(false);
    g_scan->setAdvertisedDeviceCallbacks(&g_callbacks, /*wantDuplicates=*/true,
                                         /*shouldParse=*/true);
    g_scan->setActiveScan(kBleActiveScan);
    g_scan->setInterval(kBleScanIntervalMs);   // milliseconds
    g_scan->setWindow(kBleScanWindowMs);
}

void bleScanService(uint32_t nowMs) {
    if (!g_scan) return;
    if (g_running) {
        // A session ends by itself after kBleSessionSec. If the completion
        // never arrives (host reset mid-scan) cancel it and start over.
        if (nowMs - g_sessionStartMs < kBleSessionSec * 1000 + kSessionGraceMs) return;
        Serial.printf("[BLE] scan session overran, restarting\n");
        bleScanStop();
    }
    if (static_cast<int32_t>(nowMs - g_nextStartMs) < 0) return;   // backing off

    g_running        = true;   // before start(): the completion may run at once
    g_sessionStartMs = nowMs;
    g_nextStartMs    = nowMs;
    if (!g_scan->start(kBleSessionSec, onScanComplete, /*is_continue=*/false)) {
        g_running     = false;
        g_nextStartMs = nowMs + kStartRetryMs;
        if (!g_startFailed) {
            Serial.printf("[BLE] scan start failed, retrying every %lu ms\n",
                          static_cast<unsigned long>(kStartRetryMs));
        }
        g_startFailed = true;
    } else if (g_startFailed) {
        g_startFailed = false;
        Serial.printf("[BLE] scan running again\n");
    }
}

void bleScanStop() {
    if (!g_scan) return;
    g_scan->stop();   // runs onScanComplete itself when a session was active
    g_running = false;
    g_scan->clearResults();
}

}  // namespace sh
