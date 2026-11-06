#include "frame80211.h"

#include <cstring>

#include "detection.h"

namespace sh {

namespace {

constexpr int kHdrLen = 24;

// Read the first information element at `off`; if it is an SSID element with
// a non-hidden value, copy it out.
void readSsidIe(const uint8_t* f, int len, int off, ParsedFrame& out) {
    if (off + 2 > len) return;
    const uint8_t id = f[off];
    const int     l  = f[off + 1];
    if (id != 0 || l <= 0 || l > 32 || off + 2 + l > len) return;
    bool allZero = true;
    for (int i = 0; i < l; i++) {
        const uint8_t c = f[off + 2 + i];
        if (c != 0) allZero = false;
        out.ssid[i] = (c < 0x20 || c == 0x7F) ? '?' : static_cast<char>(c);
    }
    out.ssid[l] = '\0';
    if (allZero) { out.ssid[0] = '\0'; return; }   // hidden SSID (zero-filled)
    out.hasSsid = true;
}

}  // namespace

bool parseFrame(const uint8_t* f, int len, ParsedFrame& out) {
    out = ParsedFrame{};
    if (!f || len < kHdrLen) return false;

    const uint8_t fc0 = f[0];
    const uint8_t fc1 = f[1];
    if ((fc0 & 0x03) != 0) return false;             // protocol version must be 0
    const uint8_t type    = (fc0 >> 2) & 0x3;
    const uint8_t subtype = (fc0 >> 4) & 0xF;
    const bool toDs   = (fc1 & 0x01) != 0;
    const bool fromDs = (fc1 & 0x02) != 0;

    const uint8_t* a2 = f + 10;   // transmitter
    const uint8_t* a3 = f + 16;   // BSSID for management frames
    if (macIsGroup(a2)) return false;                 // group address cannot transmit

    if (type == 0) {   // management: AP when the transmitter is the BSSID itself
        out.role = macEqual(a2, a3) ? FrameRole::Ap : FrameRole::Sta;
        std::memcpy(out.addr, a2, 6);
        if (subtype == 8 || subtype == 5) {
            // beacon / probe response: timestamp(8) + interval(2) + capability(2), then IEs
            readSsidIe(f, len, kHdrLen + 12, out);
        } else if (subtype == 4) {
            // probe request: IEs start right after the header
            readSsidIe(f, len, kHdrLen, out);
        }
        return true;
    }

    if (type == 2) {   // data
        if (toDs && fromDs) return false;             // WDS / mesh: transmitter role ambiguous
        std::memcpy(out.addr, a2, 6);
        if (fromDs)      out.role = FrameRole::Ap;    // Address 2 is the BSSID
        else if (toDs)   out.role = FrameRole::Sta;   // Address 2 is the station
        else             out.role = macEqual(a2, a3) ? FrameRole::Ap : FrameRole::Sta;
        return true;
    }

    return false;   // control / reserved
}

}  // namespace sh
