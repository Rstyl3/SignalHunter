# SignalHunter

> Inspired by and forked from **[PierreGode/waveshareesp32c6lcd](https://github.com/pierregode/waveshareesp32c6lcd)**. The LCD / LVGL / NeoPixel / proximity-state-machine code is borrowed from his `blewatch` project and extended here for hidden-camera hunting. Many thanks to Pierre.

A handheld **hidden-camera RF tracker** for the **Waveshare ESP32-C6-LCD-1.47**.

Scans WiFi APs and BLE advertisements continuously, flags devices whose MAC OUI or SSID/name matches a known camera vendor, and uses RSSI as a "hotter/colder" proximity signal driven onto the onboard WS2812 RGB LED and the 1.47" ST7789 LCD.

## What it can — and can't — detect

**Can detect:**
- WiFi cameras hosting their own access point (very common for cheap spy cams)
- WiFi cameras advertising on a 2.4 GHz network (visible via beacon frames)
- BLE-enabled cameras (modern smart cams often pair over BLE)

**Cannot detect:**
- Cameras that are wired-only, fully offline, or store to SD card without RF
- 5 GHz WiFi cameras (ESP32-C6 is 2.4 GHz only)
- Cameras that connect as a station without broadcasting (probe-request capture is out of scope for v1 — see "Future ideas" below)
- Anything that doesn't transmit RF

RSSI is **not** direction-finding. SignalHunter is a "warmer / colder" walk — sweep the room, watch the LED change.

## LED color scheme (WS2812 on GPIO 8)

| Band         | RSSI range (dBm) | Color  | Brightness |
|--------------|------------------|--------|------------|
| OUT OF RANGE | < −85            | off    | 0 %        |
| FAR          | −85 to −75       | blue   | 30 %       |
| NEAR         | −75 to −65       | cyan   | 50 %       |
| CLOSE        | −65 to −55       | green  | 70 %       |
| VERY CLOSE   | −55 to −45       | yellow | 90 %       |
| HOT          | ≥ −45            | red blink | 100 %   |

There is ±2 dBm hysteresis at each band edge to prevent color flicker.

## UI modes

Two screens, swap with a 2 s hold on the **BOOT** button (GPIO 9).

### Mode A — Hunt (default on boot)
- Big band label + RSSI in dBm + bar
- Target name (SSID / BLE name) and vendor flag chip ("CAM? Hikvision", etc.)
- Last-seen-Xms-ago ticker
- LED reflects the locked target's band (or strongest flagged if no lock)

| Button         | Action                                                |
|----------------|-------------------------------------------------------|
| Short press    | Cycle the lock to the next detection in priority order |
| Long press (≥0.6 s) | Clear the lock — auto-track strongest flagged    |
| Hold ≥2 s      | Switch to **List** mode                               |

### Mode B — List / Heatmap
- Top 8 detections, sorted flagged-first then by RSSI
- Each row: flag (`*`) · radio (W/B) · short name · RSSI · mini-bar
- LED tracks the highlighted row's band

| Button         | Action                                                |
|----------------|-------------------------------------------------------|
| Short press    | Move highlight down one row (wraps)                   |
| Long press     | Lock onto highlighted row and jump back to **Hunt**   |
| Hold ≥2 s      | Return to **Hunt** without changing the lock          |

## Hardware

| Component | Pin / Detail |
|-----------|-------------|
| MCU       | ESP32-C6 (RISC-V, WiFi 6, BLE 5) |
| Display   | 1.47" IPS LCD (172×320), ST7789 over SPI |
| RGB LED   | WS2812 on GPIO 8 (single pixel) |
| Button    | BOOT, GPIO 9, active low |

## Build / Flash (Arduino IDE)

1. Install the **ESP32 board package** (Espressif "esp32 by Espressif Systems") and select board **"ESP32C6 Dev Module"** (or the Waveshare profile if installed).
2. Install the libraries:
   - `LVGL` (v9.x)
   - `Adafruit NeoPixel`
   - `NimBLE-Arduino` (recommended; falls back to the built-in `BLEDevice` if absent)
3. Make sure the `lv_conf.h` in this folder is picked up (Arduino's LVGL library reads it via `LV_CONF_INCLUDE_SIMPLE`).
4. Open `signalhunter.ino` and flash. Boot the board — scanning starts automatically.

Serial console runs at **115200 baud** for debug.

## File layout

```
signalhunter/
├── signalhunter.ino        # setup() / loop()
├── signalhunter.cpp        # scanner, detection table, UI, LED, button
├── signalhunter.h          # public entry point declaration
├── CameraOUI.h             # camera-vendor OUIs + SSID/name patterns
├── Display_ST7789.cpp/.h   # ST7789 LCD driver  (inherited from blewatch)
├── LVGL_Driver.cpp/.h      # LVGL <-> ST7789 bridge (inherited)
├── lv_conf.h               # LVGL config (inherited)
└── SD_Card.cpp/.h          # SD slot support (inherited; unused in v1)
```

## Tuning

All tunables live near the top of `signalhunter.cpp`:

- **Band thresholds** (`kBandFarMin` … `kBandHotMin`)
- **Hysteresis** (`kBandHysteresisDb`)
- **Sticky lock margin** (`kStickyMarginDb`)
- **EMA smoothing** (`kRssiEmaNum / kRssiEmaDen`)
- **Scan periods** (`kWifiScanPeriodMs`, `kBleScanWindowMs`)
- **Button timing** (`kShortPressMaxMs`, `kLongPressMaxMs`, `kExtraLongPressMs`)

To add a missing camera vendor: append an entry to `kCameraOuis` in `CameraOUI.h`, or a substring to `kCameraNamePatterns` if the camera ships with a distinctive default SSID prefix.

## Future ideas

- **Promiscuous-mode station capture** — interleave brief WiFi-promiscuous windows on the locked channel to harvest probe requests and data frames. Pauses BLE during the window. Would catch cameras that connect as a station but don't host an AP. Time-slicing logic needs careful testing for radio stability.
- **SD-card logging** — `SD_Card.*` is already included; write a CSV of detections + RSSI samples for after-the-fact analysis.
- **Web UI over WiFi AP** — open `SignalHunter-Setup` AP for richer control without a touchscreen.
- **2.4 GHz spectrum sweep** — use ESP-IDF's `esp_wifi_set_channel` + `wifi_sta_get_ap_info()` channel-busy metric to build a coarse "is there RF activity on channel N?" heatmap independent of frame parsing. Catches non-WiFi 2.4 GHz noise.

## Honest caveats

- OUI matching is heuristic. A non-camera device with a flagged OUI will be flagged; many real cheap cameras use generic ESP32 / Realtek / Tuya OUIs that won't match — the hunter still shows them, just unflagged.
- Indoor RF has heavy multipath. Expect dead zones and false hot-spots from reflections. The technique works, but is not magic.
- `signalhunter` is for **defensive personal use** in spaces you control or have explicit permission to sweep. Don't use it where local law restricts RF monitoring.
