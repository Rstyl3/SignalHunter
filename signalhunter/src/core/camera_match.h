#pragma once
// Camera heuristics: MAC vendor lookup against the generated OUI table and
// case-insensitive name/SSID pattern matching. Pure.
//
// Both are intentionally over-inclusive: a flag means "worth a closer look",
// never "confirmed camera".
#include <cstdint>

#include "detection.h"

namespace sh {

struct VendorMatch {
    const char* name;   // nullptr when nothing matched
    VendorTier  tier;
};

// Longest-prefix match over the 36-, 28- and 24-bit registries.
VendorMatch vendorLookup(const uint8_t mac[6]);

// Returns the matched pattern (to show as the reason) or nullptr.
const char* cameraNameMatch(const char* name);

}  // namespace sh
