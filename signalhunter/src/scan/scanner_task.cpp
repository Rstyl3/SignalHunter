#include "scanner_task.h"

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../core/config.h"
#include "ble_scan.h"
#include "detections_shared.h"
#include "wifi_scan.h"

namespace sh {

namespace {

void scannerTask(void* /*arg*/) {
    wifiScanInit();
    bleScanInit();

    uint32_t lastSweepMs = millis() - kWifiSweepPeriodMs;   // sweep right away
    for (;;) {
        const uint32_t now = millis();
        if (now - lastSweepMs >= kWifiSweepPeriodMs) {
            lastSweepMs = now;
            bleScanStop();
            wifiSweepOnce();
        }
        bleScanService(millis());   // (re)start a BLE session whenever none is running
        detectionsAnnounceNew();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

}  // namespace

void scannerStart() {
    xTaskCreate(scannerTask, "sh_scan", 8192, nullptr, 1, nullptr);
}

}  // namespace sh
