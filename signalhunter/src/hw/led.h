#pragma once
// Onboard WS2812 driven by the current RSSI band.
#include <cstdint>

#include "../core/band.h"

namespace sh {

void ledInit();
// Show the band's colour / blink phase for this instant. Only touches the
// LED when the output actually changes.
void ledApply(Band b, uint32_t nowMs);

}  // namespace sh
