#pragma once
// Mode A, Hunt: one target, big band label, RSSI bar, freshness footer.
#include <cstdint>

#include "../core/band.h"
#include "../core/detection.h"

namespace sh {

void huntBuild();
void huntShow();

// `active` is the locked target or the auto-picked one (nullptr if none is in
// range). `band` is the hysteresis-filtered band already applied to the LED.
void huntRender(const Detection* active, int totalCount, bool haveLock,
                const uint8_t* lockMac, Band band, uint32_t nowMs);

}  // namespace sh
