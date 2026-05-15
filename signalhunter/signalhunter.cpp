// SignalHunter — hidden-camera RF tracker for Waveshare ESP32-C6-LCD-1.47.
//
// See signalhunter.ino for the boot sequence. This file owns everything from
// Signalhunter_Init() down: scanner task, button task, LVGL screens, LED
// driver, and the shared detection table.

#include "signalhunter.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include <lvgl.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <esp_wifi.h>

#if __has_include(<NimBLEDevice.h>)
  #include <NimBLEDevice.h>
  #define SH_USE_NIMBLE 1
#else
  #include <BLEDevice.h>
  #include <BLEScan.h>
  #define SH_USE_NIMBLE 0
#endif

#include "CameraOUI.h"

namespace {

// ============================================================================
// Tunable constants
// ============================================================================

constexpr int     kRgbPin           = 8;
constexpr int     kRgbCount         = 1;
constexpr uint16_t kNeoPixelType    = NEO_RGB + NEO_KHZ800;
constexpr int     kBootBtnPin       = 9;   // BOOT button (active low)

// UI tick (Hz controls how snappy the bar and LED feel)
constexpr uint32_t kUiIntervalMs    = 40;

// Detection table
constexpr int      kMaxDetections   = 64;
constexpr uint32_t kStaleMs         = 4000;   // drop entries not seen in this long
constexpr uint32_t kLockedStaleMs   = 30000;  // locked target gets a longer grace period

// RSSI band thresholds (dBm). Tightened so green-steady only triggers at
// genuine close range (~1-3 m indoors). RSSI vs distance is NOT linear and
// varies with transmitter power, so these may need adjustment per target —
// a cheap low-power camera at 1 m may read what a loud phone reads at 5 m.
constexpr int kBandFarMin       = -82;  // weaker than this -> OUT OF RANGE
constexpr int kBandNearMin      = -70;
constexpr int kBandCloseMin     = -58;
constexpr int kBandVeryCloseMin = -45;  // lime steady starts here (~1.5-3 m)
constexpr int kBandHotMin       = -40;  // emerald steady (~1 m or closer)

// Hysteresis: don't change bands unless RSSI crosses boundary by this much,
// in the direction it's already heading. Prevents flicker at band edges.
constexpr int kBandHysteresisDb = 2;

// Sticky lock: in auto-strongest mode, only switch if a new device is this
// many dB stronger than the current lock.
constexpr int kStickyMarginDb   = 10;

// EMA smoothing for RSSI (alpha = numerator / denominator)
constexpr int kRssiEmaNum = 1;
constexpr int kRssiEmaDen = 3;  // alpha = 0.33

// Scan timing
constexpr uint32_t kWifiScanPeriodMs = 3500;  // active AP scan every N ms
constexpr uint32_t kWifiScanTimeMs   = 1500;  // scan duration (covers all 2.4G chans)
constexpr uint32_t kBleScanWindowMs  = 250;   // chunk size for BLE scan loop

// Button timing
constexpr uint32_t kShortPressMaxMs    = 600;
constexpr uint32_t kLongPressMinMs     = 600;
constexpr uint32_t kLongPressMaxMs     = 2000;
constexpr uint32_t kExtraLongPressMs   = 2000;

// LED — 5-color rainbow gradient with rate-of-blink ramping into a steady
// "found it" state. Richer RGB tones than pure primaries so the LED doesn't
// look like a flat traffic light. Blink rate speeds up as you close in; the
// last two bands go steady so the moment blinking stops you know you're on
// top of the target.
struct RgbColor { uint8_t r, g, b; };
constexpr RgbColor LED_OFF      = {0,   0,   0};
constexpr RgbColor LED_CRIMSON  = {230, 30,  50};   // FAR
constexpr RgbColor LED_CORAL    = {255, 90,  30};   // NEAR
constexpr RgbColor LED_AMBER    = {255, 165, 0};    // CLOSE
constexpr RgbColor LED_LIME     = {130, 220, 30};   // VERY CLOSE
constexpr RgbColor LED_EMERALD  = {30,  215, 90};   // HOT

// Per-band brightness percentages
constexpr uint8_t kBrightOutOfRange = 0;
constexpr uint8_t kBrightFar        = 50;
constexpr uint8_t kBrightNear       = 65;
constexpr uint8_t kBrightClose      = 80;
constexpr uint8_t kBrightVeryClose  = 90;
constexpr uint8_t kBrightHot        = 100;

// Blink half-periods (ms). Smaller = faster blink. Top two bands are steady.
constexpr uint32_t kBlinkFarMs   = 500;   // 1 Hz
constexpr uint32_t kBlinkNearMs  = 250;   // 2 Hz
constexpr uint32_t kBlinkCloseMs = 143;   // ~3.5 Hz

// ============================================================================
// Types
// ============================================================================

enum Radio : uint8_t {
    RADIO_WIFI_AP = 0,
    RADIO_BLE     = 1,
};

enum Band : uint8_t {
    BAND_OUT_OF_RANGE = 0,
    BAND_FAR,
    BAND_NEAR,
    BAND_CLOSE,
    BAND_VERY_CLOSE,
    BAND_HOT,
};

enum UiMode : uint8_t {
    UI_HUNT = 0,
    UI_LIST = 1,
};

struct Detection {
    uint8_t  mac[6];
    char     name[33];      // SSID or BLE local name (may be empty)
    int8_t   rssi;
    int8_t   rssi_smoothed;
    uint32_t last_seen_ms;
    uint8_t  radio;         // Radio enum
    uint8_t  channel;       // WiFi only (0 for BLE)
    const char* vendor;     // non-null if MAC OUI matched a known camera vendor
    const char* name_hit;   // non-null if SSID/name matched a camera pattern
    bool     used;
};

// ============================================================================
// Globals
// ============================================================================

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
Detection    g_detections[kMaxDetections];

// Lock state
bool         g_have_lock = false;
uint8_t      g_lock_mac[6] = {0};
uint8_t      g_lock_radio = RADIO_BLE;

// Band state (for hysteresis)
Band         g_current_band = BAND_OUT_OF_RANGE;

// UI state
volatile UiMode g_ui_mode = UI_HUNT;
// Highlight is a numeric row index — stays where the user put it, regardless
// of how the underlying detection list reshuffles. On long-press we lock onto
// whichever device occupies that row at the moment of the button event.
int g_list_highlight = 0;

// LED
Adafruit_NeoPixel g_rgb(kRgbCount, kRgbPin, kNeoPixelType);

// LVGL objects (two parallel screens)
lv_obj_t* g_hunt_screen = nullptr;
lv_obj_t* g_hunt_target_label = nullptr;
lv_obj_t* g_hunt_vendor_label = nullptr;
lv_obj_t* g_hunt_rssi_label = nullptr;
lv_obj_t* g_hunt_band_label = nullptr;
lv_obj_t* g_hunt_bar = nullptr;
lv_obj_t* g_hunt_bar_label = nullptr;  // "XX%" centered on the bar
lv_obj_t* g_hunt_meta_label = nullptr;

lv_obj_t* g_list_screen = nullptr;
lv_obj_t* g_list_title = nullptr;
constexpr int kListVisibleRows = 8;
lv_obj_t* g_list_rows[kListVisibleRows] = {nullptr};
lv_obj_t* g_list_row_labels[kListVisibleRows] = {nullptr};
lv_obj_t* g_list_row_bars[kListVisibleRows] = {nullptr};

// ============================================================================
// Helpers
// ============================================================================

inline void formatMac(const uint8_t mac[6], char* out, size_t outLen) {
    snprintf(out, outLen, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

inline void formatMacTail(const uint8_t mac[6], char* out, size_t outLen) {
    snprintf(out, outLen, "%02X:%02X:%02X", mac[3], mac[4], mac[5]);
}

inline void setLedColor(const RgbColor& c, uint8_t brightnessPercent) {
    const uint16_t scale = static_cast<uint16_t>(brightnessPercent) * 255 / 100;
    const uint8_t r = static_cast<uint8_t>((static_cast<uint16_t>(c.r) * scale) / 255);
    const uint8_t g = static_cast<uint8_t>((static_cast<uint16_t>(c.g) * scale) / 255);
    const uint8_t b = static_cast<uint8_t>((static_cast<uint16_t>(c.b) * scale) / 255);
    g_rgb.setPixelColor(0, g_rgb.Color(r, g, b));
    g_rgb.show();
}

// ============================================================================
// Detection table (called from scan callbacks — keep light)
// ============================================================================

int findOrInsert(const uint8_t mac[6]) {
    int free_idx = -1;
    int oldest_idx = -1;
    uint32_t oldest_ms = UINT32_MAX;
    for (int i = 0; i < kMaxDetections; i++) {
        if (!g_detections[i].used) {
            if (free_idx < 0) free_idx = i;
            continue;
        }
        if (memcmp(g_detections[i].mac, mac, 6) == 0) return i;
        // Never evict the locked target — once the user picks something to
        // hunt, the entry is sticky regardless of staleness.
        if (g_have_lock &&
            memcmp(g_detections[i].mac, g_lock_mac, 6) == 0) {
            continue;
        }
        if (g_detections[i].last_seen_ms < oldest_ms) {
            oldest_ms = g_detections[i].last_seen_ms;
            oldest_idx = i;
        }
    }
    if (free_idx >= 0) return free_idx;
    return oldest_idx;  // evict oldest non-locked
}

void noteDetection(const uint8_t mac[6], int rssi, const char* name,
                   uint8_t radio, uint8_t channel) {
    const uint32_t now = millis();

    portENTER_CRITICAL(&g_mux);
    int idx = findOrInsert(mac);
    if (idx < 0) { portEXIT_CRITICAL(&g_mux); return; }

    Detection& d = g_detections[idx];
    const bool fresh = !d.used || memcmp(d.mac, mac, 6) != 0;
    if (fresh) {
        memcpy(d.mac, mac, 6);
        d.rssi_smoothed = static_cast<int8_t>(rssi);
        d.vendor = cameraOuiVendor(mac);
        d.name[0] = '\0';
        d.name_hit = nullptr;
    }
    d.rssi = static_cast<int8_t>(rssi);
    // EMA: smoothed = smoothed + alpha*(rssi - smoothed)
    const int delta = rssi - d.rssi_smoothed;
    d.rssi_smoothed = static_cast<int8_t>(
        d.rssi_smoothed + (delta * kRssiEmaNum) / kRssiEmaDen);
    d.last_seen_ms = now;
    d.radio = radio;
    d.channel = channel;
    d.used = true;
    if (name && name[0]) {
        // Update name on every refresh (BLE devices may finally emit a name).
        strncpy(d.name, name, sizeof(d.name) - 1);
        d.name[sizeof(d.name) - 1] = '\0';
        d.name_hit = cameraNameMatch(d.name);
    }
    portEXIT_CRITICAL(&g_mux);
}

// Copy out a snapshot of fresh detections, sorted by (flagged desc, rssi desc).
// Returns the count copied. `out` must hold kMaxDetections entries.
int snapshotFresh(Detection* out) {
    const uint32_t now = millis();
    int n = 0;
    portENTER_CRITICAL(&g_mux);
    for (int i = 0; i < kMaxDetections; i++) {
        if (!g_detections[i].used) continue;
        // Locked target gets a longer grace period so brief gaps in
        // advertisement (common on weak BLE devices) don't drop it from view.
        const bool is_locked = g_have_lock &&
            memcmp(g_detections[i].mac, g_lock_mac, 6) == 0;
        const uint32_t stale_limit = is_locked ? kLockedStaleMs : kStaleMs;
        if ((now - g_detections[i].last_seen_ms) > stale_limit) continue;
        out[n++] = g_detections[i];
    }
    portEXIT_CRITICAL(&g_mux);
    // Sort: flagged (vendor or name_hit) first, then by rssi_smoothed desc.
    // Simple insertion sort — at most 64 entries.
    auto flagged = [](const Detection& d) {
        return (d.vendor != nullptr) || (d.name_hit != nullptr);
    };
    for (int i = 1; i < n; i++) {
        Detection key = out[i];
        int j = i - 1;
        while (j >= 0) {
            const bool kf = flagged(key);
            const bool jf = flagged(out[j]);
            if (kf != jf) {
                if (kf && !jf) { out[j + 1] = out[j]; j--; } else break;
            } else if (key.rssi_smoothed > out[j].rssi_smoothed) {
                out[j + 1] = out[j]; j--;
            } else break;
        }
        out[j + 1] = key;
    }
    return n;
}

// Look up the locked detection in a snapshot, or -1 if not present/fresh.
int findInSnapshot(const Detection* snap, int n, const uint8_t mac[6]) {
    for (int i = 0; i < n; i++) {
        if (memcmp(snap[i].mac, mac, 6) == 0) return i;
    }
    return -1;
}

// ============================================================================
// Band classification with hysteresis
// ============================================================================

Band rssiToBand(int rssi, Band prev) {
    // Upper edge of each band, with hysteresis pull toward the current band.
    // When moving up (RSSI getting stronger) require crossing the next-band
    // floor by +hyst; when moving down require crossing the current floor by
    // -hyst.
    const int h = kBandHysteresisDb;

    auto edge = [&](int min_rssi, Band b) -> int {
        if (prev == b) return min_rssi - h;   // pull down (stay in this band longer)
        return min_rssi + h;                  // require extra to leave
    };

    if (rssi >= edge(kBandHotMin, BAND_HOT))               return BAND_HOT;
    if (rssi >= edge(kBandVeryCloseMin, BAND_VERY_CLOSE))  return BAND_VERY_CLOSE;
    if (rssi >= edge(kBandCloseMin, BAND_CLOSE))           return BAND_CLOSE;
    if (rssi >= edge(kBandNearMin, BAND_NEAR))             return BAND_NEAR;
    if (rssi >= edge(kBandFarMin, BAND_FAR))               return BAND_FAR;
    return BAND_OUT_OF_RANGE;
}

void applyBandToLed(Band b, uint32_t now_ms) {
    switch (b) {
        case BAND_OUT_OF_RANGE:
            setLedColor(LED_OFF, kBrightOutOfRange);
            break;
        case BAND_FAR: {
            const bool on = ((now_ms / kBlinkFarMs) % 2) == 0;
            setLedColor(on ? LED_CRIMSON : LED_OFF, kBrightFar);
            break;
        }
        case BAND_NEAR: {
            const bool on = ((now_ms / kBlinkNearMs) % 2) == 0;
            setLedColor(on ? LED_CORAL : LED_OFF, kBrightNear);
            break;
        }
        case BAND_CLOSE: {
            const bool on = ((now_ms / kBlinkCloseMs) % 2) == 0;
            setLedColor(on ? LED_AMBER : LED_OFF, kBrightClose);
            break;
        }
        case BAND_VERY_CLOSE:
            setLedColor(LED_LIME, kBrightVeryClose);
            break;
        case BAND_HOT:
            setLedColor(LED_EMERALD, kBrightHot);
            break;
    }
}

const char* bandLabel(Band b) {
    switch (b) {
        case BAND_OUT_OF_RANGE: return "OUT OF RANGE";
        case BAND_FAR:          return "FAR";
        case BAND_NEAR:         return "NEAR";
        case BAND_CLOSE:        return "CLOSE";
        case BAND_VERY_CLOSE:   return "VERY CLOSE";
        case BAND_HOT:          return "HOT";
    }
    return "?";
}

uint32_t bandLcdColor(Band b) {
    // Mirror the LED palette so the LCD band label matches the LED color.
    switch (b) {
        case BAND_OUT_OF_RANGE: return 0x444444;
        case BAND_FAR:          return 0xE61E32;  // crimson
        case BAND_NEAR:         return 0xFF5A1E;  // coral
        case BAND_CLOSE:        return 0xFFA500;  // amber
        case BAND_VERY_CLOSE:   return 0x82DC1E;  // lime
        case BAND_HOT:          return 0x1ED75A;  // emerald
    }
    return 0xFFFFFF;
}

// Map RSSI [-100, -30] -> [0, 100] for the bar widget.
int rssiBarPercent(int rssi) {
    int v = rssi + 100;        // -100 -> 0, -30 -> 70
    if (v < 0) v = 0;
    if (v > 70) v = 70;
    return (v * 100) / 70;
}

// ============================================================================
// WiFi scanner — periodic active scan for APs (covers cameras hosting their own
// AP, which is the most common spy-cam mode). Promiscuous-mode station capture
// is out of scope for v1 to keep BLE+WiFi coexistence well-behaved.
// ============================================================================

void runWifiScanOnce() {
    // Async scan; harvest results when done.
    const int found = WiFi.scanNetworks(/*async=*/false,
                                        /*show_hidden=*/true,
                                        /*passive=*/false,
                                        /*max_ms_per_chan=*/120);
    if (found <= 0) {
        WiFi.scanDelete();
        return;
    }
    for (int i = 0; i < found; i++) {
        const String ssid  = WiFi.SSID(i);
        const int    rssi  = WiFi.RSSI(i);
        const int    chan  = WiFi.channel(i);
        const uint8_t* bssid_raw = WiFi.BSSID(i);
        if (!bssid_raw) continue;
        uint8_t mac[6];
        memcpy(mac, bssid_raw, 6);
        noteDetection(mac, rssi, ssid.c_str(), RADIO_WIFI_AP,
                      static_cast<uint8_t>(chan));
    }
    WiFi.scanDelete();
}

// ============================================================================
// BLE scanner — continuous passive scan via NimBLE.
// ============================================================================

#if SH_USE_NIMBLE

// NimBLE-Arduino 2.x API. Required on ESP32-C6 (the 1.x release pulls in
// Bluedroid headers that don't exist on C6/H2).
class BleCb : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        if (!dev) return;
        const int rssi = dev->getRSSI();
        // Parse "aa:bb:cc:dd:ee:ff" — works across NimBLE versions without
        // depending on the volatile getNative()/getVal()/getBase() APIs.
        const std::string addr_s = dev->getAddress().toString();
        unsigned int b[6];
        if (sscanf(addr_s.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
                   &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) return;
        uint8_t mac[6];
        for (int i = 0; i < 6; i++) mac[i] = static_cast<uint8_t>(b[i]);

        std::string name = dev->getName();
        const char* name_c = name.empty() ? nullptr : name.c_str();
        noteDetection(mac, rssi, name_c, RADIO_BLE, 0);
    }
};

BleCb g_ble_cb;

void initBle() {
    NimBLEDevice::init("");
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&g_ble_cb, /*wantDuplicates=*/true);
    scan->setActiveScan(true);
    scan->setInterval(80);   // 50 ms (units of 0.625 ms)
    scan->setWindow(40);     // 25 ms
}

void runBleScanChunk(uint32_t ms) {
    NimBLEScan* scan = NimBLEDevice::getScan();
    // NimBLE 2.x: duration is in milliseconds (1.x used seconds).
    scan->start(ms, /*is_continue=*/false);
    scan->clearResults();
}

#else  // legacy BLE

class BleCb : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice dev) override {
        const int rssi = dev.getRSSI();
        const String addr_s = dev.getAddress().toString();
        unsigned int b[6];
        if (sscanf(addr_s.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x",
                   &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) return;
        uint8_t mac[6];
        for (int i = 0; i < 6; i++) mac[i] = static_cast<uint8_t>(b[i]);
        const String name = dev.getName();
        noteDetection(mac, rssi,
                      name.length() ? name.c_str() : nullptr,
                      RADIO_BLE, 0);
    }
};

BleCb g_ble_cb;

void initBle() {
    BLEDevice::init("");
    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&g_ble_cb);
    scan->setActiveScan(true);
    scan->setInterval(80);
    scan->setWindow(40);
}

void runBleScanChunk(uint32_t ms) {
    BLEScan* scan = BLEDevice::getScan();
    scan->start(static_cast<uint32_t>(ms / 1000) + 1, /*is_continue=*/false);
    scan->clearResults();
}

#endif  // SH_USE_NIMBLE

// ============================================================================
// Scanner task — interleaves WiFi and BLE on core 0.
// ============================================================================

void scannerTask(void* /*arg*/) {
    // WiFi: bring up STA mode but don't connect.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    delay(100);

    initBle();

    uint32_t last_wifi_scan_ms = 0;

    for (;;) {
        const uint32_t now = millis();

        // Periodic WiFi scan — blocks briefly, then we resume BLE.
        if (now - last_wifi_scan_ms >= kWifiScanPeriodMs) {
            last_wifi_scan_ms = now;
            runWifiScanOnce();
        }

        // BLE scan chunk — non-blocking-ish (returns after ~chunk).
        runBleScanChunk(kBleScanWindowMs);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ============================================================================
// Button task (BOOT, GPIO 9, active low)
// ============================================================================

enum ButtonEvent : uint8_t {
    BTN_NONE = 0,
    BTN_SHORT,        // cycle DOWN
    BTN_DOUBLE,       // cycle UP (double-click)
    BTN_LONG,         // lock / clear lock
    BTN_EXTRA_LONG,   // swap UI mode
};

// Max gap from release-of-click-1 to press-of-click-2 for a double-click to
// register. Measured on press-2 (not release-2) so the user's second-click
// hold time doesn't eat into the window.
constexpr uint32_t kDoubleClickMaxGapMs = 400;

volatile ButtonEvent g_btn_event = BTN_NONE;

void buttonTask(void* /*arg*/) {
    pinMode(kBootBtnPin, INPUT_PULLUP);
    bool was_pressed = false;
    uint32_t press_start_ms = 0;
    bool extra_long_fired = false;
    // Timestamp (millis) of the release of the last short press waiting to see
    // if it becomes a double-click. 0 = nothing pending.
    uint32_t pending_short_at = 0;
    // True when the *current* (currently held) press is the second of a
    // double-click — detected on press, committed on release.
    bool this_press_is_double = false;

    for (;;) {
        const bool pressed = (digitalRead(kBootBtnPin) == LOW);
        const uint32_t now = millis();

        if (pressed && !was_pressed) {
            press_start_ms = now;
            extra_long_fired = false;
            // If this press happened within the double-click window after a
            // pending short release, mark it as the second of a double.
            if (pending_short_at &&
                (now - pending_short_at) <= kDoubleClickMaxGapMs) {
                this_press_is_double = true;
                pending_short_at = 0;
            } else {
                this_press_is_double = false;
            }
        } else if (pressed && was_pressed) {
            // Fire EXTRA_LONG as soon as threshold passes (don't wait for release).
            if (!extra_long_fired && (now - press_start_ms) >= kExtraLongPressMs) {
                g_btn_event = BTN_EXTRA_LONG;
                extra_long_fired = true;
                pending_short_at = 0;
                this_press_is_double = false;
            }
        } else if (!pressed && was_pressed) {
            const uint32_t held = now - press_start_ms;
            if (!extra_long_fired) {
                if (held < kShortPressMaxMs) {
                    if (this_press_is_double) {
                        // Commit the double-click on release.
                        g_btn_event = BTN_DOUBLE;
                        this_press_is_double = false;
                    } else {
                        // First click — wait to see if a second is coming.
                        pending_short_at = now;
                    }
                } else if (held < kLongPressMaxMs) {
                    g_btn_event = BTN_LONG;
                    pending_short_at = 0;
                    this_press_is_double = false;
                }
            }
        }

        // Promote a pending single-click to BTN_SHORT once the window expires
        // with no follow-up press.
        if (pending_short_at &&
            (now - pending_short_at) > kDoubleClickMaxGapMs) {
            g_btn_event = BTN_SHORT;
            pending_short_at = 0;
        }

        was_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ============================================================================
// LVGL UI — Hunt screen (Mode A)
// ============================================================================

lv_obj_t* makeLabel(lv_obj_t* parent, const char* txt, lv_color_t color,
                    const lv_font_t* font = nullptr) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, txt);
    lv_obj_set_style_text_color(lbl, color, 0);
    if (font) lv_obj_set_style_text_font(lbl, font, 0);
    return lbl;
}

void buildHuntScreen() {
    g_hunt_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_hunt_screen, lv_color_hex(0x06101C), 0);
    lv_obj_set_style_pad_all(g_hunt_screen, 6, 0);

    // Header
    lv_obj_t* header = lv_obj_create(g_hunt_screen);
    lv_obj_set_size(header, LV_PCT(100), 34);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x0E2440), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 6, 0);
    lv_obj_set_style_pad_all(header, 6, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t* title = makeLabel(header, "SignalHunter", lv_color_hex(0xFFD000),
                                &lv_font_montserrat_14);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

    // Target panel
    lv_obj_t* panel = lv_obj_create(g_hunt_screen);
    lv_obj_set_size(panel, LV_PCT(100), 96);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x0E2440), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 42);

    makeLabel(panel, "TARGET", lv_color_hex(0x8BE9FD), &lv_font_montserrat_14);
    g_hunt_target_label = makeLabel(panel, "— none —", lv_color_hex(0xFFFFFF),
                                    &lv_font_montserrat_14);
    lv_obj_align(g_hunt_target_label, LV_ALIGN_LEFT_MID, 0, 0);
    g_hunt_vendor_label = makeLabel(panel, "", lv_color_hex(0xFFD000),
                                    &lv_font_montserrat_14);
    lv_obj_align(g_hunt_vendor_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Big band label
    g_hunt_band_label = makeLabel(g_hunt_screen, "OUT OF RANGE",
                                  lv_color_hex(0xFFFFFF),
                                  &lv_font_montserrat_20);
    lv_obj_align(g_hunt_band_label, LV_ALIGN_TOP_MID, 0, 148);
    lv_obj_set_style_text_align(g_hunt_band_label, LV_TEXT_ALIGN_CENTER, 0);

    // RSSI numeric
    g_hunt_rssi_label = makeLabel(g_hunt_screen, "-- dBm",
                                  lv_color_hex(0x8BE9FD),
                                  &lv_font_montserrat_20);
    lv_obj_align(g_hunt_rssi_label, LV_ALIGN_TOP_MID, 0, 184);
    lv_obj_set_style_text_align(g_hunt_rssi_label, LV_TEXT_ALIGN_CENTER, 0);

    // Bar
    g_hunt_bar = lv_bar_create(g_hunt_screen);
    lv_bar_set_range(g_hunt_bar, 0, 100);
    lv_obj_set_size(g_hunt_bar, 150, 18);
    lv_obj_align(g_hunt_bar, LV_ALIGN_TOP_MID, 0, 222);
    lv_obj_set_style_bg_color(g_hunt_bar, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_hunt_bar, LV_OPA_40, 0);
    lv_obj_set_style_radius(g_hunt_bar, 5, 0);

    // Percentage label centered over the bar.
    g_hunt_bar_label = lv_label_create(g_hunt_bar);
    lv_label_set_text(g_hunt_bar_label, "--");
    lv_obj_set_style_text_color(g_hunt_bar_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(g_hunt_bar_label, &lv_font_montserrat_14, 0);
    lv_obj_center(g_hunt_bar_label);

    // Meta (radio/channel/seen)
    g_hunt_meta_label = makeLabel(g_hunt_screen, "",
                                  lv_color_hex(0x8BE9FD),
                                  &lv_font_montserrat_14);
    lv_obj_align(g_hunt_meta_label, LV_ALIGN_TOP_MID, 0, 254);
    lv_obj_set_style_text_align(g_hunt_meta_label, LV_TEXT_ALIGN_CENTER, 0);
}

// ============================================================================
// LVGL UI — List screen (Mode B)
// ============================================================================

void buildListScreen() {
    g_list_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(g_list_screen, lv_color_hex(0x06101C), 0);
    lv_obj_set_style_pad_all(g_list_screen, 4, 0);

    lv_obj_t* header = lv_obj_create(g_list_screen);
    lv_obj_set_size(header, LV_PCT(100), 28);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x0E2440), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 6, 0);
    lv_obj_set_style_pad_all(header, 4, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    g_list_title = makeLabel(header, "Detections (0)", lv_color_hex(0xFFD000),
                             &lv_font_montserrat_14);
    lv_obj_align(g_list_title, LV_ALIGN_LEFT_MID, 4, 0);

    const int row_h = 32;
    for (int i = 0; i < kListVisibleRows; i++) {
        lv_obj_t* row = lv_obj_create(g_list_screen);
        lv_obj_set_size(row, LV_PCT(100), row_h);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x0E2440), 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_pad_all(row, 4, 0);
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 34 + i * (row_h + 2));
        g_list_rows[i] = row;

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, "");
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, -6);
        g_list_row_labels[i] = lbl;

        lv_obj_t* bar = lv_bar_create(row);
        lv_bar_set_range(bar, 0, 100);
        lv_obj_set_size(bar, LV_PCT(100), 4);
        lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_40, 0);
        g_list_row_bars[i] = bar;
    }
}

// ============================================================================
// Tick — runs from LVGL timer at kUiIntervalMs. Reads detection table,
// updates whichever screen is active + LED.
// ============================================================================

void handleButton(const Detection* snap, int n) {
    const ButtonEvent ev = g_btn_event;
    if (ev == BTN_NONE) return;
    g_btn_event = BTN_NONE;

    if (g_ui_mode == UI_HUNT) {
        if (ev == BTN_SHORT || ev == BTN_DOUBLE) {
            // Cycle target: SHORT = next (down); DOUBLE = previous (up).
            // snap order is flagged-first, rssi-desc.
            if (n == 0) return;
            const int dir = (ev == BTN_DOUBLE) ? -1 : +1;
            if (!g_have_lock) {
                const int start = (dir > 0) ? 0 : (n - 1);
                memcpy(g_lock_mac, snap[start].mac, 6);
                g_lock_radio = snap[start].radio;
                g_have_lock = true;
            } else {
                int idx = findInSnapshot(snap, n, g_lock_mac);
                if (idx < 0) idx = 0;
                idx = (idx + dir + n) % n;
                memcpy(g_lock_mac, snap[idx].mac, 6);
                g_lock_radio = snap[idx].radio;
                g_have_lock = true;
            }
        } else if (ev == BTN_LONG) {
            g_have_lock = false;
        } else if (ev == BTN_EXTRA_LONG) {
            g_ui_mode = UI_LIST;
            g_list_highlight = 0;
            lv_scr_load(g_list_screen);
        }
    } else {  // UI_LIST
        if (ev == BTN_SHORT) {
            if (n == 0) return;
            g_list_highlight = (g_list_highlight + 1) % n;
        } else if (ev == BTN_DOUBLE) {
            if (n == 0) return;
            g_list_highlight = (g_list_highlight - 1 + n) % n;
        } else if (ev == BTN_LONG) {
            if (n == 0) return;
            // Clamp in case the list shrank under us.
            int idx = g_list_highlight;
            if (idx >= n) idx = n - 1;
            if (idx < 0) idx = 0;
            memcpy(g_lock_mac, snap[idx].mac, 6);
            g_lock_radio = snap[idx].radio;
            g_have_lock = true;
            g_ui_mode = UI_HUNT;
            lv_scr_load(g_hunt_screen);
        } else if (ev == BTN_EXTRA_LONG) {
            g_ui_mode = UI_HUNT;
            lv_scr_load(g_hunt_screen);
        }
    }
}

// Pick the Detection to drive the LED + Hunt screen. Returns nullptr if none.
const Detection* pickActive(const Detection* snap, int n) {
    if (g_have_lock) {
        // While locked, NEVER fall back to "loudest other thing" — the lock
        // is sticky regardless of whether the target is currently visible in
        // the snapshot. If it disappeared, return nullptr so the UI can show
        // a "waiting" state; the user clears the lock with a long press.
        if (n > 0) {
            const int idx = findInSnapshot(snap, n, g_lock_mac);
            if (idx >= 0) return &snap[idx];
        }
        return nullptr;
    }
    // No lock — auto-pick strongest flagged (snap is sorted flagged-first).
    if (n == 0) return nullptr;
    return &snap[0];
}

void renderHunt(const Detection* active, int total_count) {
    char buf[64];
    if (!active) {
        // Two reasons we have no active detection:
        //   1) No lock and nothing in range — generic "no signal".
        //   2) Locked, but the target hasn't been heard from recently —
        //      "waiting" state. Lock stays; user must long-press to clear.
        if (g_have_lock) {
            char tail[16];
            formatMacTail(g_lock_mac, tail, sizeof(tail));
            snprintf(buf, sizeof(buf), "LOCKED · %s", tail);
            lv_label_set_text(g_hunt_target_label, buf);
            lv_label_set_text(g_hunt_vendor_label, "[waiting for signal]");
            lv_obj_set_style_text_color(g_hunt_vendor_label,
                                        lv_color_hex(0xFFD000), 0);
            lv_label_set_text(g_hunt_band_label, "WAITING");
        } else {
            lv_label_set_text(g_hunt_target_label, "— no signal —");
            lv_label_set_text(g_hunt_vendor_label, "");
            lv_label_set_text(g_hunt_band_label, "OUT OF RANGE");
        }
        lv_obj_set_style_text_color(g_hunt_band_label, lv_color_hex(0x666666), 0);
        lv_label_set_text(g_hunt_rssi_label, "-- dBm");
        lv_bar_set_value(g_hunt_bar, 0, LV_ANIM_OFF);
        lv_label_set_text(g_hunt_bar_label, "--");
        lv_obj_set_style_bg_color(g_hunt_bar, lv_color_hex(0x666666),
                                  LV_PART_INDICATOR);
        snprintf(buf, sizeof(buf), "%d nearby", total_count);
        lv_label_set_text(g_hunt_meta_label, buf);
        return;
    }

    // Target line: prefer SSID/name, fall back to MAC tail.
    if (active->name[0]) {
        snprintf(buf, sizeof(buf), "%s", active->name);
    } else {
        char tail[16];
        formatMacTail(active->mac, tail, sizeof(tail));
        snprintf(buf, sizeof(buf), "%s %s",
                 (active->radio == RADIO_BLE) ? LV_SYMBOL_BLUETOOTH
                                              : LV_SYMBOL_WIFI,
                 tail);
    }
    lv_label_set_text(g_hunt_target_label, buf);

    // Vendor / flag chip
    const char* vendor_tag = active->vendor ? active->vendor :
                             (active->name_hit ? active->name_hit : "");
    if (vendor_tag[0]) {
        snprintf(buf, sizeof(buf), "[CAM? %s]", vendor_tag);
        lv_label_set_text(g_hunt_vendor_label, buf);
        lv_obj_set_style_text_color(g_hunt_vendor_label,
                                    lv_color_hex(0xFFD000), 0);
    } else {
        lv_label_set_text(g_hunt_vendor_label, "[no vendor match]");
        lv_obj_set_style_text_color(g_hunt_vendor_label,
                                    lv_color_hex(0x8BE9FD), 0);
    }

    // Band
    const Band band = rssiToBand(active->rssi_smoothed, g_current_band);
    g_current_band = band;
    lv_label_set_text(g_hunt_band_label, bandLabel(band));
    lv_obj_set_style_text_color(g_hunt_band_label,
                                lv_color_hex(bandLcdColor(band)), 0);

    snprintf(buf, sizeof(buf), "%d dBm", active->rssi_smoothed);
    lv_label_set_text(g_hunt_rssi_label, buf);
    const int pct = rssiBarPercent(active->rssi_smoothed);
    lv_bar_set_value(g_hunt_bar, pct, LV_ANIM_OFF);
    // Percentage centered on the bar.
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(g_hunt_bar_label, buf);
    // Color the bar's fill to match the current band.
    lv_obj_set_style_bg_color(g_hunt_bar, lv_color_hex(bandLcdColor(band)),
                              LV_PART_INDICATOR);

    const uint32_t age = millis() - active->last_seen_ms;
    if (active->radio == RADIO_BLE) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_BLUETOOTH " · %lums ago",
                 static_cast<unsigned long>(age));
    } else {
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " ch%u · %lums ago",
                 active->channel, static_cast<unsigned long>(age));
    }
    lv_label_set_text(g_hunt_meta_label, buf);
}

void renderList(const Detection* snap, int n) {
    char buf[48];
    snprintf(buf, sizeof(buf), "Detections (%d)", n);
    lv_label_set_text(g_list_title, buf);

    // Highlight is a fixed row index. Clamp for display if the list shrank
    // below the chosen row, but leave the stored value alone so the user's
    // intent reappears once the list grows back.
    int highlight_idx = g_list_highlight;
    if (n == 0)                       highlight_idx = 0;
    else if (highlight_idx >= n)      highlight_idx = n - 1;
    else if (highlight_idx < 0)       highlight_idx = 0;

    // Scroll so the highlighted row is always visible.
    int scroll = 0;
    if (highlight_idx >= kListVisibleRows) {
        scroll = highlight_idx - kListVisibleRows + 1;
    }
    if (scroll > n - kListVisibleRows) scroll = n - kListVisibleRows;
    if (scroll < 0) scroll = 0;

    for (int row = 0; row < kListVisibleRows; row++) {
        const int idx = row + scroll;
        const bool highlighted = (idx == highlight_idx);
        if (idx >= n) {
            lv_label_set_text(g_list_row_labels[row], "");
            lv_bar_set_value(g_list_row_bars[row], 0, LV_ANIM_OFF);
            lv_obj_set_style_bg_color(g_list_rows[row],
                                      lv_color_hex(0x0E2440), 0);
            continue;
        }
        const Detection& d = snap[idx];
        char tail[16];
        formatMacTail(d.mac, tail, sizeof(tail));
        // LVGL built-in glyphs from the Montserrat symbol set — render as the
        // WiFi-arc and Bluetooth-runes icons via the same font as the text.
        const char* type = (d.radio == RADIO_BLE) ? LV_SYMBOL_BLUETOOTH
                                                  : LV_SYMBOL_WIFI;
        const char flag = (d.vendor || d.name_hit) ? '*' : ' ';
        const char* short_name = d.name[0] ? d.name : tail;
        // Trim long names to 14 chars to leave room for RSSI on a 172px LCD.
        char shown[15];
        size_t k = 0;
        for (; short_name[k] && k < sizeof(shown) - 1; k++) shown[k] = short_name[k];
        shown[k] = '\0';
        snprintf(buf, sizeof(buf), "%c%s %s %d", flag, type, shown,
                 d.rssi_smoothed);
        lv_label_set_text(g_list_row_labels[row], buf);
        lv_bar_set_value(g_list_row_bars[row], rssiBarPercent(d.rssi_smoothed),
                         LV_ANIM_OFF);
        // Color the mini-bar fill to match this row's band — no hysteresis
        // for per-row display, so we pass a neutral previous band.
        const Band row_band = rssiToBand(d.rssi_smoothed, BAND_OUT_OF_RANGE);
        lv_obj_set_style_bg_color(g_list_row_bars[row],
                                  lv_color_hex(bandLcdColor(row_band)),
                                  LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(g_list_rows[row],
                                  lv_color_hex(highlighted ? 0x244878
                                                           : 0x0E2440),
                                  0);
    }
}

void uiTick(lv_timer_t* /*t*/) {
    // Snapshot the table under critical section, then render outside.
    static Detection snap[kMaxDetections];
    const int n = snapshotFresh(snap);

    handleButton(snap, n);

    const Detection* active = pickActive(snap, n);

    // LED reflects the active detection (lock or auto-strongest-flagged).
    const uint32_t now = millis();
    if (active) {
        const Band b = rssiToBand(active->rssi_smoothed, g_current_band);
        g_current_band = b;
        applyBandToLed(b, now);
    } else {
        g_current_band = BAND_OUT_OF_RANGE;
        applyBandToLed(BAND_OUT_OF_RANGE, now);
    }

    if (g_ui_mode == UI_HUNT) {
        renderHunt(active, n);
    } else {
        renderList(snap, n);
    }
}

}  // namespace

// ============================================================================
// Public entry point — called from Lvgl_Init() in LVGL_Driver.cpp.
// ============================================================================

void Signalhunter_Init(void) {
    // RGB LED
    g_rgb.begin();
    g_rgb.setBrightness(255);
    g_rgb.clear();
    g_rgb.show();

    // Build both screens; default to Hunt.
    buildHuntScreen();
    buildListScreen();
    lv_scr_load(g_hunt_screen);

    // Background tasks pinned to core 0 (LVGL runs on core 1 via loop()).
    xTaskCreatePinnedToCore(scannerTask, "sh_scan", 8192, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(buttonTask,  "sh_btn",  2048, nullptr, 1, nullptr, 0);

    // UI tick driven by LVGL.
    lv_timer_create(uiTick, kUiIntervalMs, nullptr);
}
