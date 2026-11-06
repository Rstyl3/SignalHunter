#pragma once
// The one shared DetectionTable, guarded by a spinlock. Sightings arrive from
// the Wi-Fi RX task and the BLE host task; the UI reads snapshots from the
// Arduino loop task. Critical sections stay short and never log.
#include <cstdint>

#include "../core/detection_table.h"

namespace sh {

void detectionsNote(const Sighting& s);
// Fresh entries, sorted (sorting happens outside the lock).
int  detectionsSnapshot(Detection* out, int maxOut);

void detectionsSetLock(const uint8_t mac[6]);
void detectionsClearLock();
bool detectionsLockMac(uint8_t out[6]);     // false when nothing is locked
uint8_t detectionsLockedWifiChannel();      // 0 when none, or the lock is a BLE device

// Print a "[NEW] ..." line for every device not announced yet. Call from a
// task that may block on Serial (the scanner task), never from a callback.
void detectionsAnnounceNew();

}  // namespace sh
