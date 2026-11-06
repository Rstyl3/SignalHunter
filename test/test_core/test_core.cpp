// Host unit tests for signalhunter/src/core (PlatformIO + Unity, env:native).
#include <unity.h>

#include <cstring>

#include "core/band.h"
#include "core/button_fsm.h"
#include "core/camera_match.h"
#include "core/config.h"
#include "core/detection_table.h"
#include "core/frame80211.h"
#include "core/list_order.h"
#include "core/rssi_ema.h"

using namespace sh;

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------------------
// band
// ---------------------------------------------------------------------------
static void test_band_thresholds_without_history() {
    TEST_ASSERT_EQUAL(Band::OutOfRange, rssiToBand(-90, Band::OutOfRange));
    TEST_ASSERT_EQUAL(Band::Far,        rssiToBand(-80, Band::OutOfRange));
    TEST_ASSERT_EQUAL(Band::Near,       rssiToBand(-75, Band::OutOfRange));
    TEST_ASSERT_EQUAL(Band::Close,      rssiToBand(-65, Band::OutOfRange));
    TEST_ASSERT_EQUAL(Band::VeryClose,  rssiToBand(-56, Band::OutOfRange));
    TEST_ASSERT_EQUAL(Band::Hot,        rssiToBand(-45, Band::OutOfRange));
}

static void test_band_hysteresis_sticks_at_edges() {
    // From Far, -77 (1 dB over the Near floor) is not enough to move up.
    TEST_ASSERT_EQUAL(Band::Far,  rssiToBand(-77, Band::Far));
    TEST_ASSERT_EQUAL(Band::Near, rssiToBand(-76, Band::Far));
    // From Near, dropping 1 dB under the floor keeps Near.
    TEST_ASSERT_EQUAL(Band::Near, rssiToBand(-79, Band::Near));
    TEST_ASSERT_EQUAL(Band::Far,  rssiToBand(-81, Band::Near));
    // Hot sticks a little below its floor.
    TEST_ASSERT_EQUAL(Band::Hot,       rssiToBand(-53, Band::Hot));
    TEST_ASSERT_EQUAL(Band::VeryClose, rssiToBand(-55, Band::Hot));
}

static void test_bar_percent_clamps() {
    TEST_ASSERT_EQUAL_INT(0,   rssiBarPercent(-100));
    TEST_ASSERT_EQUAL_INT(0,   rssiBarPercent(kBarMinDbm));
    TEST_ASSERT_EQUAL_INT(50,  rssiBarPercent((kBarMinDbm + kBarMaxDbm) / 2));
    TEST_ASSERT_EQUAL_INT(100, rssiBarPercent(kBarMaxDbm));
    TEST_ASSERT_EQUAL_INT(100, rssiBarPercent(-20));
}

static void test_led_blink_and_scale() {
    const LedState far = bandLed(Band::Far);
    TEST_ASSERT_EQUAL_UINT32(kBlinkFarMs, far.blinkHalfMs);
    TEST_ASSERT_TRUE(ledOn(far, 0));
    TEST_ASSERT_FALSE(ledOn(far, kBlinkFarMs));
    TEST_ASSERT_TRUE(ledOn(bandLed(Band::Hot), 12345));   // steady
    const Rgb half = scaleRgb({200, 100, 0}, 50);
    TEST_ASSERT_UINT8_WITHIN(2, 100, half.r);
    TEST_ASSERT_UINT8_WITHIN(2, 50, half.g);
    TEST_ASSERT_EQUAL_UINT8(0, half.b);
    TEST_ASSERT_EQUAL_UINT8(0, bandLed(Band::OutOfRange).brightness);
}

// ---------------------------------------------------------------------------
// rssi_ema
// ---------------------------------------------------------------------------
static void test_ema_moves_on_small_deltas() {
    RssiEma e;
    emaReset(e, -70);
    TEST_ASSERT_EQUAL_INT(-70, emaValue(e));
    for (int i = 0; i < 20; i++) emaUpdate(e, -68);   // delta of 2 dB
    TEST_ASSERT_EQUAL_INT(-68, emaValue(e));          // integer EMA would be stuck at -70
    emaUpdate(e, -40);
    TEST_ASSERT_TRUE(emaValue(e) > -68 && emaValue(e) < -40);   // one step, not a jump
}

// ---------------------------------------------------------------------------
// detection_table
// ---------------------------------------------------------------------------
static const uint8_t kMacHik[6]   = {0xAC, 0xB9, 0x2F, 0x01, 0x02, 0x03};   // Hikvision
static const uint8_t kMacEsp[6]   = {0x24, 0x0A, 0xC4, 0x01, 0x02, 0x03};   // Espressif
static const uint8_t kMacPlain[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};   // unknown, hardware
static const uint8_t kMacLaa[6]   = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};   // unknown, privacy MAC

static Sighting sight(const uint8_t* mac, int rssi, const char* name = nullptr,
                      Radio radio = Radio::Ble, uint8_t ch = 0) {
    return Sighting{mac, rssi, name, radio, ch};
}

static void test_table_insert_update_and_vendor() {
    DetectionTable t;
    const int i = t.note(sight(kMacHik, -60, "DS-2CD"), 1000);
    TEST_ASSERT_TRUE(i >= 0);
    TEST_ASSERT_EQUAL_STRING("Hikvision", t.at(i).vendor);
    TEST_ASSERT_EQUAL(VendorTier::Camera, t.at(i).tier);
    TEST_ASSERT_EQUAL_STRING("DS-", t.at(i).nameHit);
    TEST_ASSERT_EQUAL_INT(-60, t.at(i).rssiSmoothed());
    TEST_ASSERT_FALSE(t.at(i).announced);

    const int j = t.note(sight(kMacHik, -54), 1200);
    TEST_ASSERT_EQUAL_INT(i, j);                          // same slot
    TEST_ASSERT_EQUAL_INT(-58, t.at(i).rssiSmoothed());   // -60 + (6/3)
    TEST_ASSERT_EQUAL_STRING("DS-2CD", t.at(i).name);     // name kept
    TEST_ASSERT_EQUAL_INT(1, t.used());
}

static void test_table_ema_resets_after_gap() {
    DetectionTable t;
    const int i = t.note(sight(kMacPlain, -80), 0);
    t.note(sight(kMacPlain, -50), kEmaResetGapMs + 1);
    TEST_ASSERT_EQUAL_INT(-50, t.at(i).rssiSmoothed());
}

static void test_table_ap_label_is_sticky() {
    DetectionTable t;
    const int i = t.note(sight(kMacPlain, -60, "Home", Radio::WifiAp, 6), 0);
    t.note(sight(kMacPlain, -60, nullptr, Radio::WifiSta, 6), 100);
    TEST_ASSERT_EQUAL(Radio::WifiAp, t.at(i).radio);
    const int k = t.note(sight(kMacLaa, -60, nullptr, Radio::WifiSta, 1), 0);
    t.note(sight(kMacLaa, -60, nullptr, Radio::Ble, 0), 100);
    TEST_ASSERT_EQUAL(Radio::Ble, t.at(k).radio);
}

static void test_table_snapshot_filters_stale_and_sorts() {
    DetectionTable t;
    t.note(sight(kMacLaa, -40), 0);       // strongest but privacy MAC, unflagged
    t.note(sight(kMacPlain, -70), 0);     // hardware MAC, unflagged
    t.note(sight(kMacEsp, -90), 0);       // IoT tier
    t.note(sight(kMacHik, -95), 0);       // camera tier, weakest
    Detection snap[kMaxDetections];
    int n = t.snapshot(snap, kMaxDetections, 100);
    TEST_ASSERT_EQUAL_INT(4, n);
    TEST_ASSERT_EQUAL_MEMORY(kMacHik, snap[0].mac, 6);
    TEST_ASSERT_EQUAL_MEMORY(kMacEsp, snap[1].mac, 6);
    TEST_ASSERT_EQUAL_MEMORY(kMacPlain, snap[2].mac, 6);
    TEST_ASSERT_EQUAL_MEMORY(kMacLaa, snap[3].mac, 6);
    // Everything ages out after kStaleMs, except the locked target.
    t.setLock(kMacPlain);
    n = t.snapshot(snap, kMaxDetections, kStaleMs + 1);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_MEMORY(kMacPlain, snap[0].mac, 6);
    n = t.snapshot(snap, kMaxDetections, kLockedStaleMs + 1);
    TEST_ASSERT_EQUAL_INT(0, n);
}

static void test_table_eviction_prefers_stale_unflagged_privacy_macs() {
    DetectionTable t;
    uint8_t mac[6] = {0x02, 0xAA, 0, 0, 0, 0};   // privacy MACs
    // Fill the table: slot 0 = camera vendor (old), slot 1 = locked, the rest privacy phones.
    t.note(sight(kMacHik, -90), 0);
    t.note(sight(kMacPlain, -90), 0);
    t.setLock(kMacPlain);
    for (int i = 2; i < kMaxDetections; i++) {
        mac[4] = static_cast<uint8_t>(i >> 8);
        mac[5] = static_cast<uint8_t>(i & 0xFF);
        t.note(sight(mac, -60), 10 + i);
    }
    TEST_ASSERT_EQUAL_INT(kMaxDetections, t.used());
    // A newcomer must evict a phone, never the camera (older) nor the lock.
    const int i = t.note(sight(kMacEsp, -50), 5000);
    TEST_ASSERT_TRUE(i >= 2);
    Detection snap[kMaxDetections];
    const int n = t.snapshot(snap, kMaxDetections, 5000);
    TEST_ASSERT_TRUE(findInSnapshot(snap, n, kMacHik) >= 0);
    TEST_ASSERT_TRUE(findInSnapshot(snap, n, kMacPlain) >= 0);
    TEST_ASSERT_TRUE(findInSnapshot(snap, n, kMacEsp) >= 0);
}

static void test_table_take_unannounced_marks_entries() {
    DetectionTable t;
    t.note(sight(kMacHik, -60), 0);
    t.note(sight(kMacEsp, -60), 0);
    Detection out[4];
    TEST_ASSERT_EQUAL_INT(2, t.takeUnannounced(out, 4));
    TEST_ASSERT_EQUAL_INT(0, t.takeUnannounced(out, 4));
    t.note(sight(kMacHik, -61), 10);   // update: not new again
    TEST_ASSERT_EQUAL_INT(0, t.takeUnannounced(out, 4));
}

static void test_pick_auto_is_sticky_within_margin() {
    DetectionTable t;
    t.note(sight(kMacPlain, -60), 0);
    t.note(sight(kMacLaa, -55), 0);   // 5 dB stronger but privacy MAC: sorts second anyway
    Detection snap[kMaxDetections];
    int n = t.snapshot(snap, kMaxDetections, 10);
    TEST_ASSERT_EQUAL_MEMORY(kMacPlain, snap[pickAuto(snap, n, nullptr, kStickyMarginDb)].mac, 6);
    // If the previous auto target was kMacLaa, a 5 dB weaker hardware MAC
    // does not take over: it needs the full margin.
    TEST_ASSERT_EQUAL_MEMORY(kMacLaa, snap[pickAuto(snap, n, kMacLaa, kStickyMarginDb)].mac, 6);
    t.note(sight(kMacPlain, -40), 20);
    t.note(sight(kMacPlain, -40), 30);
    t.note(sight(kMacPlain, -40), 40);
    t.note(sight(kMacPlain, -40), 50);
    n = t.snapshot(snap, kMaxDetections, 60);
    TEST_ASSERT_TRUE(snap[findInSnapshot(snap, n, kMacPlain)].rssiSmoothed() - (-55) >= kStickyMarginDb);
    TEST_ASSERT_EQUAL_MEMORY(kMacPlain, snap[pickAuto(snap, n, kMacLaa, kStickyMarginDb)].mac, 6);
    // A higher priority class always wins.
    t.note(sight(kMacHik, -95), 70);
    n = t.snapshot(snap, kMaxDetections, 80);
    TEST_ASSERT_EQUAL_MEMORY(kMacHik, snap[pickAuto(snap, n, kMacPlain, kStickyMarginDb)].mac, 6);
    TEST_ASSERT_EQUAL_INT(-1, pickAuto(snap, 0, nullptr, kStickyMarginDb));
}

static void test_copy_display_name_sanitises() {
    char buf[8];
    copyDisplayName(buf, sizeof(buf), "a\x01" "b\x7f" "cdefghij");
    TEST_ASSERT_EQUAL_STRING("a?b?cde", buf);
    copyDisplayName(buf, sizeof(buf), nullptr);
    TEST_ASSERT_EQUAL_STRING("", buf);
}

// ---------------------------------------------------------------------------
// list_order
// ---------------------------------------------------------------------------
static void test_list_order_freezes_between_refreshes() {
    DetectionTable t;
    t.note(sight(kMacPlain, -60), 0);
    Detection snap[kMaxDetections];
    int n = t.snapshot(snap, kMaxDetections, 0);
    ListOrder lo;
    TEST_ASSERT_TRUE(listOrderRefresh(lo, snap, n, 0, false));
    TEST_ASSERT_EQUAL_INT(1, lo.count);
    t.note(sight(kMacHik, -50), 10);
    n = t.snapshot(snap, kMaxDetections, 10);
    TEST_ASSERT_FALSE(listOrderRefresh(lo, snap, n, 10, false));   // frozen
    TEST_ASSERT_EQUAL_INT(1, lo.count);
    TEST_ASSERT_TRUE(listOrderRefresh(lo, snap, n, 20, true));     // forced
    TEST_ASSERT_EQUAL_INT(2, lo.count);
    TEST_ASSERT_TRUE(listOrderRefresh(lo, snap, n, 20 + kListRefreshPeriodMs, false));
}

static void test_list_clamp_and_scroll() {
    TEST_ASSERT_EQUAL_INT(0, clampHighlight(5, 0));
    TEST_ASSERT_EQUAL_INT(2, clampHighlight(5, 3));
    TEST_ASSERT_EQUAL_INT(0, clampHighlight(-1, 3));
    TEST_ASSERT_EQUAL_INT(0, listScroll(3, 20, 7));
    TEST_ASSERT_EQUAL_INT(4, listScroll(10, 20, 7));
    TEST_ASSERT_EQUAL_INT(0, listScroll(6, 5, 7));
}

// ---------------------------------------------------------------------------
// button_fsm
// ---------------------------------------------------------------------------
static ButtonEvent drive(ButtonFsm& fsm, bool pressed, uint32_t from, uint32_t to) {
    ButtonEvent got = ButtonEvent::None;
    for (uint32_t t = from; t <= to; t += 20) {
        const ButtonEvent e = fsm.feed(pressed, t);
        if (e != ButtonEvent::None) {
            TEST_ASSERT_EQUAL(ButtonEvent::None, got);   // never two events in one window
            got = e;
        }
    }
    return got;
}

static void test_button_short_press() {
    ButtonFsm fsm;
    TEST_ASSERT_EQUAL(ButtonEvent::None, drive(fsm, true, 0, 100));
    TEST_ASSERT_EQUAL(ButtonEvent::None, drive(fsm, false, 120, 400));   // waiting for a double
    TEST_ASSERT_EQUAL(ButtonEvent::Short, drive(fsm, false, 420, 700));
}

static void test_button_double_press() {
    ButtonFsm fsm;
    drive(fsm, true, 0, 100);
    drive(fsm, false, 120, 300);
    TEST_ASSERT_EQUAL(ButtonEvent::None, drive(fsm, true, 320, 400));
    TEST_ASSERT_EQUAL(ButtonEvent::Double, drive(fsm, false, 420, 500));
    TEST_ASSERT_EQUAL(ButtonEvent::None, drive(fsm, false, 520, 1500));
}

static void test_button_long_and_extra_long() {
    ButtonFsm fsm;
    TEST_ASSERT_EQUAL(ButtonEvent::None, drive(fsm, true, 0, 1500));
    TEST_ASSERT_EQUAL(ButtonEvent::Long, drive(fsm, false, 1520, 1600));
    ButtonFsm fsm2;
    TEST_ASSERT_EQUAL(ButtonEvent::ExtraLong, drive(fsm2, true, 0, kExtraLongPressMs + 40));
    TEST_ASSERT_EQUAL(ButtonEvent::None, drive(fsm2, true, kExtraLongPressMs + 60, 6000));
    TEST_ASSERT_EQUAL(ButtonEvent::None, drive(fsm2, false, 6020, 7000));   // release is silent
}

// ---------------------------------------------------------------------------
// frame80211
// ---------------------------------------------------------------------------
static const uint8_t kBssid[6] = {0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x01};
static const uint8_t kSta[6]   = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
static const uint8_t kBcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static int header(uint8_t* f, uint8_t fc0, uint8_t fc1, const uint8_t* a1,
                  const uint8_t* a2, const uint8_t* a3) {
    std::memset(f, 0, 24);
    f[0] = fc0;
    f[1] = fc1;
    std::memcpy(f + 4, a1, 6);
    std::memcpy(f + 10, a2, 6);
    std::memcpy(f + 16, a3, 6);
    return 24;
}

static void test_frame_beacon_is_ap_with_ssid() {
    uint8_t f[64];
    int n = header(f, 0x80, 0x00, kBcast, kBssid, kBssid);   // beacon
    n += 12;                                                 // fixed fields
    f[n++] = 0; f[n++] = 4; std::memcpy(f + n, "Home", 4); n += 4;
    ParsedFrame p;
    TEST_ASSERT_TRUE(parseFrame(f, n + 4 /* FCS */, p));
    TEST_ASSERT_EQUAL(FrameRole::Ap, p.role);
    TEST_ASSERT_EQUAL_MEMORY(kBssid, p.addr, 6);
    TEST_ASSERT_TRUE(p.hasSsid);
    TEST_ASSERT_EQUAL_STRING("Home", p.ssid);
}

static void test_frame_probe_request_is_sta_with_ssid() {
    uint8_t f[64];
    int n = header(f, 0x40, 0x00, kBcast, kSta, kBcast);     // probe request
    f[n++] = 0; f[n++] = 3; std::memcpy(f + n, "Cam", 3); n += 3;
    ParsedFrame p;
    TEST_ASSERT_TRUE(parseFrame(f, n, p));
    TEST_ASSERT_EQUAL(FrameRole::Sta, p.role);
    TEST_ASSERT_EQUAL_MEMORY(kSta, p.addr, 6);
    TEST_ASSERT_EQUAL_STRING("Cam", p.ssid);
}

static void test_frame_hidden_ssid_and_probe_response() {
    uint8_t f[64];
    int n = header(f, 0x50, 0x00, kSta, kBssid, kBssid);     // probe response
    n += 12;
    f[n++] = 0; f[n++] = 0;                                  // zero-length SSID
    ParsedFrame p;
    TEST_ASSERT_TRUE(parseFrame(f, n, p));
    TEST_ASSERT_EQUAL(FrameRole::Ap, p.role);
    TEST_ASSERT_FALSE(p.hasSsid);
}

static void test_frame_data_ds_bits() {
    uint8_t f[64];
    ParsedFrame p;
    header(f, 0x08, 0x01, kBssid, kSta, kBssid);             // data, ToDS: station transmits
    TEST_ASSERT_TRUE(parseFrame(f, 28, p));
    TEST_ASSERT_EQUAL(FrameRole::Sta, p.role);
    TEST_ASSERT_EQUAL_MEMORY(kSta, p.addr, 6);
    header(f, 0x88, 0x02, kSta, kBssid, kSta);               // QoS data, FromDS: AP transmits
    TEST_ASSERT_TRUE(parseFrame(f, 30, p));
    TEST_ASSERT_EQUAL(FrameRole::Ap, p.role);
    TEST_ASSERT_EQUAL_MEMORY(kBssid, p.addr, 6);
    header(f, 0x08, 0x03, kSta, kBssid, kSta);               // WDS: skipped
    TEST_ASSERT_FALSE(parseFrame(f, 30, p));
    header(f, 0x48, 0x01, kBssid, kSta, kBssid);             // null data from a station
    TEST_ASSERT_TRUE(parseFrame(f, 24, p));
    TEST_ASSERT_EQUAL(FrameRole::Sta, p.role);
}

static void test_frame_rejects_junk() {
    uint8_t f[64];
    ParsedFrame p;
    header(f, 0x40, 0x00, kBcast, kBcast, kBcast);           // multicast transmitter
    TEST_ASSERT_FALSE(parseFrame(f, 24, p));
    header(f, 0xB4, 0x00, kSta, kBssid, kBssid);             // control frame (RTS)
    TEST_ASSERT_FALSE(parseFrame(f, 24, p));
    header(f, 0x80, 0x00, kBcast, kBssid, kBssid);
    TEST_ASSERT_FALSE(parseFrame(f, 10, p));                 // truncated
    TEST_ASSERT_FALSE(parseFrame(nullptr, 100, p));
}

// ---------------------------------------------------------------------------
// camera_match
// ---------------------------------------------------------------------------
static void test_vendor_lookup_known_blocks() {
    const uint8_t hik[6] = {0xAC, 0xB9, 0x2F, 0, 0, 0};
    const uint8_t reo[6] = {0xEC, 0x71, 0xDB, 0, 0, 0};
    const uint8_t wyz[6] = {0x2C, 0xAA, 0x8E, 0, 0, 0};
    const uint8_t apl[6] = {0x8C, 0x85, 0x90, 0, 0, 0};   // Apple: must not be a camera vendor
    const uint8_t mtk[6] = {0x00, 0x0C, 0x43, 0, 0, 0};   // MediaTek/Ralink, not Hikvision
    TEST_ASSERT_EQUAL_STRING("Hikvision", vendorLookup(hik).name);
    TEST_ASSERT_EQUAL(VendorTier::Camera, vendorLookup(hik).tier);
    TEST_ASSERT_EQUAL_STRING("Reolink", vendorLookup(reo).name);
    TEST_ASSERT_EQUAL_STRING("Wyze", vendorLookup(wyz).name);
    TEST_ASSERT_NULL(vendorLookup(apl).name);
    TEST_ASSERT_EQUAL(VendorTier::None, vendorLookup(apl).tier);
    TEST_ASSERT_EQUAL_STRING("MediaTek", vendorLookup(mtk).name);
    TEST_ASSERT_EQUAL(VendorTier::Iot, vendorLookup(mtk).tier);
}

static void test_name_patterns() {
    TEST_ASSERT_EQUAL_STRING("TAPO", cameraNameMatch("Tapo_C210_1234"));
    TEST_ASSERT_EQUAL_STRING("DS-", cameraNameMatch("ds-2cd2043"));
    TEST_ASSERT_EQUAL_STRING("IPC", cameraNameMatch("IPC-123"));
    TEST_ASSERT_NULL(cameraNameMatch("TIPCO-Office"));          // IPC only as a prefix
    TEST_ASSERT_EQUAL_STRING("RING", cameraNameMatch("Ring Chime"));
    TEST_ASSERT_NULL(cameraNameMatch("Herring-WiFi"));          // RING only as a word
    TEST_ASSERT_NULL(cameraNameMatch("Ernest"));                // NEST only as a word
    TEST_ASSERT_EQUAL_STRING("NEST", cameraNameMatch("Nest Cam"));
    TEST_ASSERT_NULL(cameraNameMatch("TLSC-guest"));            // LSC only as a prefix
    TEST_ASSERT_EQUAL_STRING("LSC", cameraNameMatch("LSC-Smart-1a2b"));
    TEST_ASSERT_NULL(cameraNameMatch("Baby shark"));            // BABY alone is not enough
    TEST_ASSERT_NOT_NULL(cameraNameMatch("babycam-01"));
    TEST_ASSERT_NULL(cameraNameMatch(""));
    TEST_ASSERT_NULL(cameraNameMatch(nullptr));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_band_thresholds_without_history);
    RUN_TEST(test_band_hysteresis_sticks_at_edges);
    RUN_TEST(test_bar_percent_clamps);
    RUN_TEST(test_led_blink_and_scale);
    RUN_TEST(test_ema_moves_on_small_deltas);
    RUN_TEST(test_table_insert_update_and_vendor);
    RUN_TEST(test_table_ema_resets_after_gap);
    RUN_TEST(test_table_ap_label_is_sticky);
    RUN_TEST(test_table_snapshot_filters_stale_and_sorts);
    RUN_TEST(test_table_eviction_prefers_stale_unflagged_privacy_macs);
    RUN_TEST(test_table_take_unannounced_marks_entries);
    RUN_TEST(test_pick_auto_is_sticky_within_margin);
    RUN_TEST(test_copy_display_name_sanitises);
    RUN_TEST(test_list_order_freezes_between_refreshes);
    RUN_TEST(test_list_clamp_and_scroll);
    RUN_TEST(test_button_short_press);
    RUN_TEST(test_button_double_press);
    RUN_TEST(test_button_long_and_extra_long);
    RUN_TEST(test_frame_beacon_is_ap_with_ssid);
    RUN_TEST(test_frame_probe_request_is_sta_with_ssid);
    RUN_TEST(test_frame_hidden_ssid_and_probe_response);
    RUN_TEST(test_frame_data_ds_bits);
    RUN_TEST(test_frame_rejects_junk);
    RUN_TEST(test_vendor_lookup_known_blocks);
    RUN_TEST(test_name_patterns);
    return UNITY_END();
}
