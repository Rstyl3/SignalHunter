#pragma once
// Exponential moving average of RSSI in Q4 fixed point (value * 16), so that
// steps smaller than the integer alpha quantum still move the estimate.
#include <cstdint>

#include "config.h"

namespace sh {

struct RssiEma {
    int16_t q4   = 0;
    bool    init = false;
};

inline void emaReset(RssiEma& e, int rssi) {
    e.q4   = static_cast<int16_t>(rssi * 16);
    e.init = true;
}

inline void emaUpdate(RssiEma& e, int rssi) {
    if (!e.init) { emaReset(e, rssi); return; }
    const int target = rssi * 16;
    e.q4 = static_cast<int16_t>(e.q4 + ((target - e.q4) * kRssiEmaNum) / kRssiEmaDen);
}

// Rounded to the nearest dBm (half away from zero).
inline int emaValue(const RssiEma& e) {
    const int q = e.q4;
    return (q >= 0) ? (q + 8) / 16 : (q - 8) / 16;
}

}  // namespace sh
