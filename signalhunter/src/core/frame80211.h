#pragma once
// Minimal 802.11 header parser for promiscuous-mode capture: who transmitted
// this frame, were they an access point or a station, and did the frame
// carry an SSID. Pure, bounds-checked, no allocation.
#include <cstdint>

namespace sh {

enum class FrameRole : uint8_t { Skip = 0, Ap, Sta };

struct ParsedFrame {
    FrameRole role = FrameRole::Skip;
    uint8_t   addr[6] = {0, 0, 0, 0, 0, 0};   // transmitter address (Address 2)
    char      ssid[33] = {0};
    bool      hasSsid = false;
};

// `len` is the frame length as reported by the radio (may include the FCS).
// Returns false when the frame should be ignored (control frames, WDS,
// multicast transmitters, truncated headers).
bool parseFrame(const uint8_t* frame, int len, ParsedFrame& out);

}  // namespace sh
