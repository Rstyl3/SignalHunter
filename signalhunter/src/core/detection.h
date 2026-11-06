#pragma once
// One tracked radio source (Wi-Fi AP, Wi-Fi station or BLE advertiser).
#include <cstdint>
#include <cstring>

#include "rssi_ema.h"

namespace sh {

enum class Radio : uint8_t { WifiAp = 0, Ble = 1, WifiSta = 2 };

// How strongly the MAC vendor suggests a camera.
enum class VendorTier : uint8_t {
    None   = 0,  // no vendor match
    Iot    = 1,  // generic IoT / chipset vendor (Espressif, Tuya, TP-Link, ...)
    Camera = 2,  // camera-focused vendor (Hikvision, Reolink, Wyze, ...)
};

inline bool macEqual(const uint8_t* a, const uint8_t* b) { return std::memcmp(a, b, 6) == 0; }
inline bool macIsLaa(const uint8_t* mac)   { return (mac[0] & 0x02) != 0; }  // locally administered (privacy MAC)
inline bool macIsGroup(const uint8_t* mac) { return (mac[0] & 0x01) != 0; }  // multicast / broadcast

struct Detection {
    uint8_t     mac[6];
    char        name[33];     // SSID or BLE local name (may be empty)
    int8_t      rssi;         // last raw reading
    RssiEma     ema;
    uint32_t    lastSeenMs;
    Radio       radio;
    uint8_t     channel;      // Wi-Fi only (0 for BLE)
    const char* vendor;       // vendor display name, or nullptr
    VendorTier  tier;
    const char* nameHit;      // matched name pattern, or nullptr
    bool        used;
    bool        announced;    // false until the "[NEW]" log line has been emitted

    int  rssiSmoothed() const { return emaValue(ema); }
    bool flagged() const { return tier != VendorTier::None || nameHit != nullptr; }
    // Sort / auto-pick priority, higher first.
    int priority() const {
        if (tier == VendorTier::Camera) return 3;
        if (nameHit != nullptr)         return 2;
        if (tier == VendorTier::Iot)    return 1;
        return 0;
    }
};

}  // namespace sh
