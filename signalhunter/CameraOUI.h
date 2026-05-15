#pragma once
//
// CameraOUI.h — heuristic table of MAC OUI prefixes and SSID/name substrings
// commonly used by wireless IP cameras and "smart" cams.
//
// The list is intentionally over-inclusive: false positives (a non-camera
// device matching a vendor OUI) are acceptable, false negatives (missing a
// hidden cam) are not. Treat a flag as "worth a closer look", never as
// "confirmed camera". Many cheap cams use generic ESP32 / Realtek / Espressif
// OUIs and will not match — the hunter still surfaces them by RSSI.
//
// OUI references: IEEE MA-L public registry + reverse-lookup of vendor MACs
// reported by Hikvision / Dahua / Wyze / etc. user communities. Adjust as
// needed if you have a known camera that isn't flagging.
//
#include <Arduino.h>
#include <string.h>

struct CameraOuiEntry {
    uint8_t  oui[3];
    const char* vendor;
};

constexpr CameraOuiEntry kCameraOuis[] = {
    // Hikvision (also rebranded under many private labels)
    {{0x00, 0x0C, 0x43}, "Hikvision"},
    {{0x00, 0x40, 0x48}, "Hikvision"},
    {{0x28, 0x57, 0xBE}, "Hikvision"},
    {{0x44, 0x19, 0xB6}, "Hikvision"},
    {{0x4C, 0xBD, 0x8F}, "Hikvision"},
    {{0xC0, 0x51, 0x7E}, "Hikvision"},
    {{0xBC, 0xAD, 0x28}, "Hikvision"},
    {{0xF0, 0x4B, 0x3A}, "Hikvision"},
    {{0x18, 0x68, 0xCB}, "Hikvision"},
    {{0x24, 0x0F, 0x9B}, "Hikvision"},
    {{0xAC, 0xB9, 0x2F}, "Hikvision"},
    {{0xB4, 0xA3, 0x82}, "Hikvision"},
    {{0xD4, 0xE8, 0x53}, "Hikvision"},
    {{0xF8, 0x4D, 0xFC}, "Hikvision"},
    {{0x44, 0x47, 0xCC}, "Hikvision"},

    // Dahua
    {{0x00, 0x25, 0xD5}, "Dahua"},
    {{0x14, 0xA7, 0x8B}, "Dahua"},
    {{0x3C, 0xEF, 0x8C}, "Dahua"},
    {{0x4C, 0x11, 0xBF}, "Dahua"},
    {{0x90, 0x02, 0xA9}, "Dahua"},
    {{0xA0, 0xBD, 0x1D}, "Dahua"},
    {{0xBC, 0x32, 0x5F}, "Dahua"},
    {{0xE0, 0x50, 0x8B}, "Dahua"},

    // Hangzhou Xiongmai (white-labels half the cheap cams on AliExpress)
    {{0x00, 0x12, 0x31}, "Xiongmai"},
    {{0x00, 0x24, 0x5F}, "Xiongmai"},
    {{0x1C, 0x5F, 0x2B}, "Xiongmai"},

    // Axis Communications
    {{0x00, 0x40, 0x8C}, "Axis"},
    {{0xAC, 0xCC, 0x8E}, "Axis"},
    {{0xB8, 0xA4, 0x4F}, "Axis"},
    {{0x00, 0x0E, 0xF8}, "Axis"},

    // Bosch Security
    {{0x00, 0x1C, 0x44}, "Bosch"},
    {{0x00, 0xE0, 0x7C}, "Bosch"},

    // Vivotek
    {{0x00, 0x02, 0xD1}, "Vivotek"},

    // Foscam
    {{0x00, 0x1A, 0x11}, "Foscam"},
    {{0x00, 0x60, 0x33}, "Foscam"},

    // Mobotix
    {{0x00, 0x03, 0xC5}, "Mobotix"},

    // Hanwha / Samsung Techwin (Wisenet)
    {{0x00, 0x09, 0x18}, "Hanwha"},
    {{0x00, 0x1B, 0xE1}, "Hanwha"},
    {{0x00, 0x23, 0x09}, "Hanwha"},

    // Wyze Labs
    {{0x2C, 0xAA, 0x8E}, "Wyze"},
    {{0x7C, 0x78, 0xB2}, "Wyze"},
    {{0xA4, 0xDA, 0x32}, "Wyze"},
    {{0x5C, 0xE7, 0x0D}, "Wyze"},

    // GoPro
    {{0x78, 0xDB, 0x2F}, "GoPro"},
    {{0xBC, 0x8C, 0xCD}, "GoPro"},
    {{0xF4, 0xDD, 0x9E}, "GoPro"},

    // Reolink (subset — Reolink also uses generic OUIs)
    {{0xEC, 0x71, 0xDB}, "Reolink"},

    // Amcrest
    {{0x00, 0x1B, 0x4B}, "Amcrest"},
    {{0x9C, 0x8E, 0xCD}, "Amcrest"},

    // Tuya Smart (Tuya-based cams are everywhere under random brand names)
    {{0xDC, 0x4F, 0x22}, "Tuya"},
    {{0x68, 0x57, 0x2D}, "Tuya"},
    {{0xCC, 0x8C, 0xE3}, "Tuya"},
    {{0xD8, 0x1F, 0x12}, "Tuya"},

    // eufy / Anker (security cams)
    {{0x8C, 0x85, 0x90}, "eufy"},

    // Arlo (Netgear cam brand)
    {{0xB0, 0x7F, 0xB9}, "Arlo"},
    {{0x9C, 0xD3, 0x6D}, "Arlo"},

    // Ring (Amazon)
    {{0x44, 0x65, 0x0D}, "Ring"},
    {{0xB0, 0x09, 0xDA}, "Ring"},

    // Nest cams (Google)
    {{0x18, 0xB4, 0x30}, "Nest"},
    {{0x64, 0x16, 0x66}, "Nest"},
};
constexpr size_t kCameraOuiCount = sizeof(kCameraOuis) / sizeof(kCameraOuis[0]);

// Case-insensitive substrings often seen in WiFi SSIDs / BLE names of cameras.
constexpr const char* kCameraNamePatterns[] = {
    "IPC",          // generic IP-camera prefix used by Hikvision OEMs
    "IPCAM",
    "IP-CAM",
    "IPCAMERA",
    "CAM-",
    "-CAM",
    "CAMERA",
    "HIKVISION",
    "DAHUA",
    "DH-",          // Dahua model prefix
    "DS-",          // Hikvision model prefix
    "WYZE",
    "REOLINK",
    "FOSCAM",
    "V380",         // common Chinese cam app
    "VSTARCAM",
    "WANSVIEW",
    "TAPO",         // TP-Link cam
    "TAPO_C",
    "EZVIZ",
    "EUFY",
    "ARLO",
    "RING",
    "NEST",
    "GOPRO",
    "AXIS-",
    "MOBOTIX",
    "VIVOTEK",
    "AMCREST",
    "XMEYE",        // Xiongmai app brand
    "MIPC",         // Tuya
    "SMARTLIFE-",   // Tuya
    "TUYA",
    "BABY",         // baby monitors are usually IP cams
    "DOORBELL",
};
constexpr size_t kCameraNamePatternCount =
    sizeof(kCameraNamePatterns) / sizeof(kCameraNamePatterns[0]);

// Returns the vendor name for a matching OUI, or nullptr if no match.
inline const char* cameraOuiVendor(const uint8_t mac[6]) {
    for (size_t i = 0; i < kCameraOuiCount; i++) {
        if (mac[0] == kCameraOuis[i].oui[0] &&
            mac[1] == kCameraOuis[i].oui[1] &&
            mac[2] == kCameraOuis[i].oui[2]) {
            return kCameraOuis[i].vendor;
        }
    }
    return nullptr;
}

// Case-insensitive substring match against the camera name patterns. Returns
// the matched pattern (so callers can show it as the reason) or nullptr.
inline const char* cameraNameMatch(const char* name) {
    if (!name || !name[0]) return nullptr;
    // Build uppercase copy, capped at 48 chars (longer names are uncommon and
    // would waste stack).
    char up[49];
    size_t n = 0;
    for (; name[n] && n < sizeof(up) - 1; n++) {
        char c = name[n];
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        up[n] = c;
    }
    up[n] = '\0';
    for (size_t i = 0; i < kCameraNamePatternCount; i++) {
        if (strstr(up, kCameraNamePatterns[i]) != nullptr) {
            return kCameraNamePatterns[i];
        }
    }
    return nullptr;
}
