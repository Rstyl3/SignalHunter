#pragma once
// Frozen display order for the List view: the sorted snapshot is captured
// every kListRefreshPeriodMs (or on demand) so rows do not shuffle while the
// user is scrolling. Pure.
#include <cstdint>

#include "config.h"
#include "detection.h"

namespace sh {

struct ListOrder {
    uint8_t  macs[kMaxDetections][6];
    int      count         = 0;
    uint32_t lastRefreshMs = 0;
    bool     valid         = false;
};

// Capture the snapshot order if the period has elapsed (or `force`).
// Returns true when the order was refreshed.
bool listOrderRefresh(ListOrder& lo, const Detection* snap, int n, uint32_t now, bool force);

int clampHighlight(int highlight, int total);
// First visible row index so that `highlight` is on screen.
int listScroll(int highlight, int total, int visibleRows);

}  // namespace sh
