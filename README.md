# SignalHunter

> Inspired by and forked from **[PierreGode/waveshareesp32c6lcd](https://github.com/pierregode/waveshareesp32c6lcd)**. The LCD / LVGL / NeoPixel / proximity-state-machine code is borrowed from his `blewatch` project and extended here for hidden-camera hunting. Many thanks to Pierre.

A handheld **hidden-camera RF tracker** for the **Waveshare ESP32-C6-LCD-1.47**.

Scans the 2.4 GHz Wi-Fi and BLE bands continuously, flags devices whose MAC OUI or SSID/name matches a known camera vendor, and uses RSSI as a "warmer/colder" proximity signal driven onto the onboard WS2812 RGB LED and the 1.47" ST7789 LCD.

## What it can — and can't — detect

**Can detect:**
- Wi-Fi cameras hosting their own access point (common for cheap spy cams in setup mode)
- Wi-Fi cameras that joined an existing 2.4 GHz network — caught as **stations** via promiscuous-mode frame capture across all 13 channels (e.g. Tapo C210, Wyze, Ring, Reolink)
- BLE-enabled cameras (modern smart cams often advertise on BLE for pairing/companion-app discovery)

**Cannot detect:**
- Cameras that are wired-only, fully offline, or store to SD card without RF
- 5 GHz Wi-Fi cameras (ESP32-C6 is 2.4 GHz only)
- Anything that doesn't transmit RF

RSSI is **not** direction-finding. SignalHunter is a "warmer / colder" walk — sweep the room, watch the LED change.

## LED color scheme (WS2812 on GPIO 8)

Five-color rainbow gradient with blink-rate ramping into a steady "found it" state. The moment the blinking stops you know you're on top of the target.

| Band         | RSSI (dBm)   | Color    | Brightness | Blink           |
|--------------|--------------|----------|------------|-----------------|
| OUT OF RANGE | < −85        | off      |   0 %      | —               |
| FAR          | −85 … −78    | crimson  |  50 %      | 1 Hz            |
| NEAR         | −78 … −70    | coral    |  65 %      | 2 Hz            |
| CLOSE        | −70 … −60    | amber    |  80 %      | ~3.5 Hz         |
| VERY CLOSE   | −60 … −52    | lime     |  90 %      | steady          |
| HOT          | ≥ −52        | emerald  | 100 %      | steady          |

Band thresholds are tuned for **low-TX-power IP cameras** (Nest / Tapo / Wyze etc.), which often only register −55 to −65 dBm even at contact distance. A phone at point-blank will pin HOT from further away — that's expected for a hunting tool (false positives are easier to dismiss than false negatives). ±2 dBm hysteresis at each band edge prevents flicker.

The big band label on the LCD is colour-matched to the LED so the screen and the light always agree.

## UI modes

Two screens. The **BOOT** button (GPIO 9) is the only input — distinguishing four gestures:

| Gesture        | Definition                          |
|----------------|-------------------------------------|
| Short          | press + release in < 0.6 s          |
| Double         | two short presses within 0.4 s gap  |
| Long           | hold 0.6 – 3.5 s                    |
| Extra-long     | hold ≥ 3.5 s                        |

### Mode A — Hunt (default on boot)
- Target name (SSID / BLE name, or MAC tail with radio icon)
- Vendor flag chip ("`[CAM? TP-Link]`", "`[CAM? Hikvision]`", etc.) when the OUI matches `CameraOUI.h`
- Big colour-coded band label + RSSI in dBm + bar with %
- Radio icon + channel + last-seen-Xms-ago ticker as footer
- LED reflects the locked target's band (or strongest flagged if no lock)

**The lock is sticky.** Once locked, short / double presses do nothing — the device won't accidentally swap targets while you're moving around. If the target goes quiet, the screen shows `WAITING` and the lock is held; the only way out is a long press.

| Gesture        | Action                                   |
|----------------|------------------------------------------|
| Short / Double | (no-op — protects the lock)              |
| Long / Extra-long | Go to **List** mode to pick a new target |

### Mode B — List
- Top detections, sorted: flagged-first → hardware-MAC before randomized → strongest RSSI
- Each row: `[eye-icon if flagged] [radio icon] [name | mac-tail] [rssi]` + mini-bar coloured to match the band
- Highlighted row drives the LED so you can sweep through devices and feel each one's distance
- List order is frozen for 3 s at a time — no row-shuffling mid-scroll

| Gesture        | Action                                        |
|----------------|-----------------------------------------------|
| Short          | Move highlight DOWN one row (wraps)           |
| Double         | Move highlight UP one row (wraps)             |
| Long / Extra-long | Lock highlighted row + jump back to **Hunt** |

### Icons

| Icon | Meaning |
|------|---------|
| 👁 (eye)        | OUI or SSID matched a known camera vendor |
| 📶 (wifi fan)   | Wi-Fi AP — device is **broadcasting** a network |
| 🏠 (home)       | Wi-Fi STA — device is a **client** on an existing network (← most cameras live here) |
| 🔵 (bluetooth)  | BLE device |

## Hardware

| Component | Pin / Detail |
|-----------|--------------|
| MCU       | ESP32-C6 (RISC-V, Wi-Fi 6, BLE 5) |
| Display   | 1.47" IPS LCD (172×320), ST7789 over SPI |
| RGB LED   | WS2812 on GPIO 8 (single pixel) |
| Button    | BOOT, GPIO 9, active low |

## Build / Flash

Two supported flows.

### PlatformIO (recommended)

From the repo root:

```sh
pio run -t upload
pio device monitor
```

The Waveshare board's USB-Serial-JTAG auto-resets into download mode — no BOOT-button dance. See [platformio.ini](platformio.ini) for the partition table (`huge_app.csv`) and the build flags that route `Serial` through the C6's native USB CDC.

### Arduino IDE

1. Install the **ESP32 board package** (Espressif "esp32 by Espressif Systems") and select board **"ESP32C6 Dev Module"** (or the Waveshare profile if installed).
2. Install the libraries:
   - `LVGL` (v9.x)
   - `Adafruit NeoPixel`
   - `NimBLE-Arduino` is **not** required and **not** recommended — arduino-esp32 v3.x ships NimBLE 2.x as a built-in framework component. Installing it separately causes duplicate-symbol link errors.
3. Make sure the `lv_conf.h` in this folder is picked up (Arduino's LVGL library reads it via `LV_CONF_INCLUDE_SIMPLE`).
4. Open `signalhunter.ino` and flash. Boot the board — scanning starts automatically.

Serial console runs at **115200 baud** for debug. New detections log a `[NEW] BLE/AP/STA <mac> rssi=… vendor=…` line so you can spot any cameras whose OUI isn't yet in `CameraOUI.h`.

## How the scanner works

A single FreeRTOS task on core 0 interleaves three radio passes on a 5-second period:

1. **Active Wi-Fi AP scan** (~1.5 s) — catches access-point-mode cameras and your home APs.
2. **Promiscuous-mode burst** (~2.6 s) — hops channels 1 → 13 dwelling 200 ms each, listens for **management + data** frames, extracts source MACs from probes and data frames. This is how station-mode cameras (Tapo C210, etc.) get found.
3. **BLE continuous passive scan** in 250 ms chunks between Wi-Fi cycles.

Detections persist 20 s after last sighting (`kStaleMs`) so a STA that's only caught during one channel dwell every few cycles still stays on the list. The currently-locked target gets a longer 30 s grace period (`kLockedStaleMs`) so brief gaps in advertisement don't drop it.

## File layout

```
signalhunter/
├── signalhunter.ino        # setup() / loop()
├── signalhunter.cpp        # scanner, detection table, UI, LED, button
├── signalhunter.h          # public entry point declaration
├── CameraOUI.h             # ~220 camera-vendor OUIs + SSID/name patterns
├── Display_ST7789.cpp/.h   # ST7789 LCD driver  (inherited from blewatch)
├── LVGL_Driver.cpp/.h      # LVGL ↔ ST7789 bridge (inherited)
├── lv_conf.h               # LVGL config (inherited)
└── SD_Card.cpp/.h          # SD slot support (inherited; unused in v1)
platformio.ini              # PlatformIO build target (pioarduino fork)
```

## Tuning

All tunables live near the top of [signalhunter/signalhunter.cpp](signalhunter/signalhunter.cpp):

- **Band thresholds** — `kBandFarMin` … `kBandHotMin`
- **Hysteresis** — `kBandHysteresisDb`
- **Sticky-lock margin** — `kStickyMarginDb` (auto-track only)
- **EMA smoothing** — `kRssiEmaNum / kRssiEmaDen` (default α = 0.33)
- **Scan timing** — `kWifiScanPeriodMs`, `kBleScanWindowMs`
- **Promiscuous capture** — `kPromiscFirstChan` / `kPromiscLastChan`, `kPromiscDwellMs` (bump the dwell if a STA camera flickers in/out)
- **Detection freshness** — `kStaleMs`, `kLockedStaleMs`
- **Button timing** — `kShortPressMaxMs`, `kLongPressMaxMs`, `kExtraLongPressMs`, `kDoubleClickMaxGapMs`

### Adding a missing camera vendor

The repo ships with ~220 OUIs covering Hikvision, Dahua, Imou, Xiaomi/Yi, D-Link, TP-Link/Tapo, Wyze, Ring, Nest, eufy, Arlo, Reolink, Foscam, Amcrest, Bosch, Axis, Vivotek, Mobotix, Hanwha, GoPro, Annke, Lorex, Tenda, Ezviz, VStarcam, Tuya, LSC, Xiongmai, plus chipset blocks for Espressif, Realtek and MediaTek that cover most no-name AliExpress / Amazon spy-cams.

To add another: append an entry to `kCameraOuis` in [signalhunter/CameraOUI.h](signalhunter/CameraOUI.h). For cameras whose SSID or BLE name contains a distinctive substring, add it to `kCameraNamePatterns` instead — that's a case-insensitive substring match across the device's broadcast name.

When a new device appears in serial as `vendor=(unknown)` and you can confirm it's a camera (e.g. by checking the MAC in the vendor's app), grab the first three bytes and add them — the next flash will flag it.

## Future ideas

- **SD-card logging** — `SD_Card.*` is already wired in; write a CSV of detections + RSSI samples for after-the-fact analysis.
- **Web UI over Wi-Fi AP** — open a `SignalHunter-Setup` AP for richer control without buttons.
- **Adaptive channel dwell** — once a target is locked, spend more time on its channel during the promiscuous burst so re-detection latency drops.
- **2.4 GHz spectrum sweep** — coarse "is there RF activity on channel N?" heatmap independent of frame parsing. Catches non-Wi-Fi 2.4 GHz noise.

## Honest caveats

- OUI matching is heuristic. A non-camera device with a flagged OUI will be flagged (your TP-Link router will absolutely show the eye-icon); many real cheap cameras use generic ESP32 / Realtek / Tuya OUIs that won't precisely identify the brand — the hunter still surfaces them, just without a vendor-specific tag.
- BLE devices that aren't yours and don't match a known OUI are almost always **randomized privacy MACs** (phones, watches, AirTags rotating their identifier every ~15 min). The list view de-prioritises locally-administered MACs for this reason.
- Indoor RF has heavy multipath. Expect dead zones and false hot-spots from reflections. The technique works, but is not magic — sweep slowly and trust the steady-state more than transients.
- `signalhunter` is for **defensive personal use** in spaces you control or have explicit permission to sweep. Don't use it where local law restricts RF monitoring.
