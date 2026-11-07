#pragma once
// BLE side of the scanner (arduino-esp32 `BLE` library on the ESP-IDF NimBLE
// host). Runs as short back-to-back scan sessions rather than one endless
// scan: with active scanning the library only reports a connectable
// advertiser once its scan response arrives, and devices that never answer
// are reported when a session completes. A cancelled scan never completes,
// so with an endless scan those devices silently vanish. Paused around
// Wi-Fi sweeps.
#include <cstdint>

namespace sh {

void bleScanInit();
// Keep a session running: starts the next one once the previous has
// completed, failed to start or overran. Call often from the scanner task.
void bleScanService(uint32_t nowMs);
// Cancel the running session (before a Wi-Fi sweep); bleScanService()
// starts a fresh one afterwards.
void bleScanStop();

}  // namespace sh
