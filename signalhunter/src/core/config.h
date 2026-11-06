#pragma once
// SignalHunter tunables. Pure header (no Arduino includes) so it is shared by
// the firmware and the host unit tests. Everything a user might want to
// adjust lives here; the modules only consume these values.
#include <cstdint>

namespace sh {

// ---------------------------------------------------------------------------
// Hardware (Waveshare ESP32-C6-LCD-1.47)
// ---------------------------------------------------------------------------
constexpr int     kRgbPin           = 8;    // WS2812, single pixel
constexpr int     kBootBtnPin       = 9;    // BOOT button, active low
constexpr uint8_t kBacklightPercent = 80;   // raised only after the first frame is painted

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------
constexpr uint32_t kUiIntervalMs        = 40;     // UI tick (bar + LED responsiveness)
constexpr uint32_t kAgeTickerMs         = 250;    // how often the "x s ago" footer refreshes
constexpr uint32_t kListRefreshPeriodMs = 3000;   // List view: order + repaint cadence
constexpr int      kListVisibleRows     = 7;      // rows above the detail footer
constexpr uint32_t kListFadeMs          = 10000;  // rows unseen this long are drawn grey

// ---------------------------------------------------------------------------
// Detection table
// ---------------------------------------------------------------------------
constexpr int      kMaxDetections = 128;
// Stale window: must comfortably exceed the worst-case re-detection latency
// for a station caught only during one channel dwell per sweep.
constexpr uint32_t kStaleMs       = 20000;
constexpr uint32_t kLockedStaleMs = 30000;  // locked target gets a longer grace period
// RSSI smoothing: exponential moving average, alpha = num/den, in Q4 fixed
// point so small steps are not lost to integer truncation. After a silence
// longer than kEmaResetGapMs the filter restarts from the next raw reading.
constexpr int      kRssiEmaNum     = 1;
constexpr int      kRssiEmaDen     = 3;
constexpr uint32_t kEmaResetGapMs  = 5000;

// ---------------------------------------------------------------------------
// RSSI bands (dBm). Calibrated for low-TX-power targets (Nest / Tapo / Wyze
// cameras), which often register only -55..-65 dBm at contact distance.
// ---------------------------------------------------------------------------
constexpr int kBandFarMin       = -85;  // weaker than this -> OUT OF RANGE
constexpr int kBandNearMin      = -78;
constexpr int kBandCloseMin     = -70;
constexpr int kBandVeryCloseMin = -60;
constexpr int kBandHotMin       = -52;
constexpr int kBandHysteresisDb = 2;    // band edges stick by this much
constexpr int kStickyMarginDb   = 10;   // auto mode switches target only if this much stronger
constexpr int kBarMinDbm        = -85;  // bar 0 %
constexpr int kBarMaxDbm        = -55;  // bar 100 %

// ---------------------------------------------------------------------------
// Scanner
// ---------------------------------------------------------------------------
constexpr uint32_t kWifiSweepPeriodMs    = 6000;  // one promiscuous channel sweep per period
constexpr uint8_t  kPromiscFirstChan     = 1;
constexpr uint8_t  kPromiscLastChan      = 13;    // 14 is JP-only
constexpr uint32_t kPromiscDwellMs       = 150;   // per-channel listen time
constexpr uint32_t kPromiscLockedDwellMs = 500;   // dwell on the locked Wi-Fi target's channel
constexpr bool     kBleActiveScan        = true;  // active: asks advertisers for their name
constexpr uint16_t kBleScanIntervalMs    = 80;
constexpr uint16_t kBleScanWindowMs      = 40;
// BLE scans run as back-to-back sessions of this length, not one endless
// scan: with active scanning the BLE library only reports a connectable
// advertiser once its scan response arrives, and devices that never answer
// are reported when a session completes. Shorter sessions = fresher RSSI
// for those devices; each restart costs a few ms of listening.
constexpr uint32_t kBleSessionSec        = 1;

// ---------------------------------------------------------------------------
// Button gestures
// ---------------------------------------------------------------------------
constexpr uint32_t kShortPressMaxMs     = 600;   // press + release shorter than this = short
constexpr uint32_t kLongPressMaxMs      = 3500;  // 0.6 .. 3.5 s = long
constexpr uint32_t kExtraLongPressMs    = 3500;  // held this long fires extra-long (no release needed)
constexpr uint32_t kDoubleClickMaxGapMs = 400;   // release-of-1 to press-of-2

// ---------------------------------------------------------------------------
// LED per band: brightness (%) and blink half-period (ms, 0 = steady)
// ---------------------------------------------------------------------------
constexpr uint8_t  kBrightFar       = 50;
constexpr uint8_t  kBrightNear      = 65;
constexpr uint8_t  kBrightClose     = 80;
constexpr uint8_t  kBrightVeryClose = 90;
constexpr uint8_t  kBrightHot       = 100;
constexpr uint32_t kBlinkFarMs      = 500;   // 1 Hz
constexpr uint32_t kBlinkNearMs     = 250;   // 2 Hz
constexpr uint32_t kBlinkCloseMs    = 143;   // ~3.5 Hz

}  // namespace sh
