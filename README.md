# SignalHunter

> Inspired by and forked from **[PierreGode/waveshareesp32c6lcd](https://github.com/pierregode/waveshareesp32c6lcd)**. The LCD / LVGL driver files come from his `blewatch` project (see [NOTICE.md](NOTICE.md)); everything else was written for hidden-camera hunting. Many thanks to Pierre.

A handheld **hidden-camera RF tracker** for the **Waveshare ESP32-C6-LCD-1.47**.

Listens on the 2.4 GHz Wi-Fi and BLE bands continuously, flags devices whose MAC vendor or SSID/name looks like a camera, and turns RSSI into a "warmer / colder" signal on the onboard WS2812 RGB LED and the 1.47" ST7789 LCD. It never transmits: Wi-Fi is captured in promiscuous mode, BLE uses a scan.

## What it can — and can't — detect

**Can detect:**
- Wi-Fi cameras hosting their own access point (cheap spy cams in setup mode) — heard through their beacons
- Wi-Fi cameras that joined an existing 2.4 GHz network — caught as **stations** from their data and probe frames across all 13 channels (e.g. Tapo C210, Wyze, Ring, Reolink)
- BLE-enabled cameras (modern smart cams advertise on BLE for pairing / companion-app discovery)

**Cannot detect:**
- Cameras that are wired-only, fully offline, or record to SD card without RF
- 5 GHz Wi-Fi cameras (the ESP32-C6 is 2.4 GHz only)
- Anything that doesn't transmit RF

RSSI is **not** direction-finding. SignalHunter is a "warmer / colder" walk — sweep the room slowly and watch the LED change.

## LED colour scheme (WS2812 on GPIO 8)

Five-colour gradient with blink-rate ramping into a steady "found it" state. The moment the blinking stops you are on top of the target.

| Band         | RSSI (dBm)   | Colour   | Brightness | Blink    |
|--------------|--------------|----------|------------|----------|
| OUT OF RANGE | < −85        | off      |   0 %      | —        |
| FAR          | −85 … −78    | crimson  |  50 %      | 1 Hz     |
| NEAR         | −78 … −70    | coral    |  65 %      | 2 Hz     |
| CLOSE        | −70 … −60    | amber    |  80 %      | ~3.5 Hz  |
| VERY CLOSE   | −60 … −52    | lime     |  90 %      | steady   |
| HOT          | ≥ −52        | emerald  | 100 %      | steady   |

Band thresholds are tuned for **low-TX-power IP cameras** (Nest / Tapo / Wyze etc.), which often only register −55 to −65 dBm even at contact distance. A phone at point-blank will pin HOT from further away — expected for a hunting tool (false positives are easier to dismiss than false negatives). ±2 dBm hysteresis at each band edge prevents flicker, and the hysteresis resets whenever the tracked device changes.

The big band label on the LCD is colour-matched to the LED so screen and light always agree.

## UI modes

Two screens. The **BOOT** button (GPIO 9) is the only input, with four gestures:

| Gesture    | Definition                                        |
|------------|---------------------------------------------------|
| Short      | press + release in < 0.6 s                        |
| Double     | two short presses within 0.4 s gap                |
| Long       | hold 0.6 – 3.5 s, fires on release                |
| Extra-long | hold ≥ 3.5 s, fires while still held               |

### Mode A — Hunt (default on boot)
- Header chip shows **AUTO** (following the best candidate) or **LOCK** (following one device)
- Target name (SSID / BLE name, or radio icon + MAC tail)
- Vendor chip: `[CAM? Hikvision]` for a camera-vendor MAC, `[CAM? name:TAPO]` for a name match, `[IoT Espressif]` for a generic chipset/IoT vendor, `[no vendor match]` otherwise
- Big colour-coded band label, RSSI in dBm, bar with percentage
- Footer: radio icon, channel, and how long ago the device was last heard

In **AUTO** the device follows the strongest candidate, camera vendors first, then name matches, then IoT vendors, then everything else (hardware MACs before randomised ones). It only switches to another device of the same class when that one is at least 10 dB stronger, so the LED does not hop between two similar devices.

**The lock is sticky.** Once locked, short / double presses do nothing — the device won't swap targets while you move around. If the target goes quiet the screen shows `WAITING` and the lock is held.

| Gesture        | Action                                   |
|----------------|------------------------------------------|
| Short / Double | no-op (protects the lock)                |
| Long           | go to **List** to pick a target          |
| Extra-long     | clear the lock, back to **AUTO**         |

### Mode B — List
- Everything in range, sorted: camera vendors → name matches → IoT vendors → unflagged, hardware MACs before randomised ones, then strongest first
- Each row: `[eye if flagged] [radio icon] [name | vendor + MAC tail]  [rssi]` with a mini-bar coloured by band; rows not heard for 10 s turn grey
- The highlighted row's name scrolls if it is too long, other rows are cut short with `...`; the header shows `position/total`
- A footer shows the highlighted device's vendor chip, radio, channel, RSSI, age and MAC tail
- **The highlighted row drives the LED**, so you can step through devices and feel each one's distance
- The order is frozen for 3 s at a time — no row-shuffling mid-scroll; button presses repaint immediately without reordering

| Gesture    | Action                                            |
|------------|---------------------------------------------------|
| Short      | move highlight DOWN one row (wraps)               |
| Double     | move highlight UP one row (wraps)                 |
| Long       | lock the highlighted row and jump back to **Hunt** |
| Extra-long | back to **Hunt** without changing the lock        |

### Icons

| Icon | Meaning |
|------|---------|
| eye        | MAC vendor or name matched a camera heuristic |
| wifi fan   | Wi-Fi AP — the device is **broadcasting** a network |
| home       | Wi-Fi STA — the device is a **client** on an existing network (most cameras live here) |
| bluetooth  | BLE device |

## Hardware

| Component | Pin / Detail |
|-----------|--------------|
| MCU       | ESP32-C6 (single-core RISC-V, Wi-Fi 6, BLE 5) |
| Display   | 1.47" IPS LCD (172×320), ST7789 over SPI |
| RGB LED   | WS2812 on GPIO 8 (single pixel) |
| Button    | BOOT, GPIO 9, active low |

## Build / Flash

### PlatformIO (recommended)

From the repo root:

```sh
pio run -t upload
pio device monitor
```

The Waveshare board's USB-Serial-JTAG auto-resets into download mode — no BOOT-button dance. [platformio.ini](platformio.ini) pins the pioarduino platform release (arduino-esp32 3.3.8 / ESP-IDF 5.5.4), LVGL 9.5.0 and the NeoPixel library, selects the `huge_app.csv` partition table and routes `Serial` through the C6's native USB CDC.

On Windows, run `pio` from PowerShell or cmd. The platform's toolchain installer refuses to run under Git Bash / MSYS.

### Arduino IDE

1. Install the **ESP32 board package** (Espressif "esp32 by Espressif Systems", 3.x) and select **"ESP32C6 Dev Module"** (or the Waveshare profile if installed).
2. Install the libraries `LVGL` (9.x) and `Adafruit NeoPixel`.
3. BLE comes from the board package's own `BLE` library. Do **not** install `NimBLE-Arduino`: it compiles a second NimBLE host and the link fails with duplicate symbols.
4. Make sure the `lv_conf.h` in the sketch folder is the one LVGL picks up (`LV_CONF_INCLUDE_SIMPLE`, or copy it next to the library).
5. Open `signalhunter/signalhunter.ino` and flash. The IDE compiles the modules under `signalhunter/src/` automatically. Scanning starts at boot.

### Tests and CI

The pure logic (band classification, RSSI smoothing, detection table and eviction, list ordering, button gestures, 802.11 frame parsing, vendor/name matching) lives in `signalhunter/src/core/` without any Arduino dependency and is covered by Unity tests in `test/test_core/`:

```sh
pio test -e native
```

This needs a host C++ compiler on `PATH` (GCC or Clang; on Windows install MSYS2 and its `mingw-w64-ucrt-x86_64-gcc` package, then add `C:\msys64\ucrt64\bin` to `PATH`). The GitHub Actions workflow in `.github/workflows/build.yml` builds the firmware and runs the tests on every push.

## How the scanner works

One FreeRTOS task owns both radios:

- **BLE scans in back-to-back 1 s sessions** (active scan, so advertisers are asked for their names; duplicates reported so RSSI keeps updating). A device that never answers a scan request is only reported when a session completes, which is why the scan is not one endless session: that one would only ever be cancelled, and such devices would silently drop out.
- Every **6 s** BLE is paused for one **promiscuous Wi-Fi sweep**: channels 1 → 13, 150 ms each (500 ms on the locked target's channel, so a locked station re-appears faster). Management and data frames are parsed for the transmitter's MAC and role:
  - beacons / probe responses → **AP** (SSID taken from the frame)
  - probe requests → **STA** (requested SSID shown if present)
  - data frames → **STA** when sent towards the network, **AP** when sent from it; WDS frames are ignored
- No active Wi-Fi scan: beacons already carry everything it would reveal, and the device stays silent.

That gives BLE roughly two thirds of the air time instead of the one fifth the first version had. The detection table holds 128 devices; when it is full, stale unflagged privacy-MAC entries are evicted first and flagged entries last, and the locked target is never evicted. Entries expire 20 s after the last sighting (`kStaleMs`); the locked target gets 30 s (`kLockedStaleMs`).

Serial runs at **115200 baud**. Every new device logs one line, e.g.

```
[NEW] STA 44:19:B6:12:34:56 rssi=-61 ch=6 vendor=Hikvision [camera vendor] name=DS-2CD2043
[NEW] BLE 5C:E7:0D:AA:BB:CC rssi=-70 ch=0 vendor=Wyze [camera vendor]
[NEW] AP  24:0A:C4:00:11:22 rssi=-48 ch=1 vendor=Espressif [iot vendor] name=ESP_001122
[PROMISC] sweep: 812 frames (total 4310)
[BTN] LONG in HUNT
[LOCK] -> 44:19:B6:12:34:56
```

## File layout

```
signalhunter/
├── signalhunter.ino        # setup() / loop()
├── Display_ST7789.cpp/.h   # ST7789 LCD driver            (inherited, see NOTICE.md)
├── LVGL_Driver.cpp/.h      # LVGL display + tick bridge   (inherited)
├── lv_conf.h               # LVGL config                   (inherited)
└── src/
    ├── app.cpp/.h          # Signalhunter_Init(): wires everything
    ├── core/               # pure C++17, no Arduino: unit-tested on the host
    │   ├── config.h        # every tunable
    │   ├── band.*          # RSSI bands, hysteresis, bar %, LED/LCD colours
    │   ├── rssi_ema.h      # fixed-point RSSI smoothing
    │   ├── detection*.{h,cpp}  # Detection struct, table, eviction, sorting, auto-pick
    │   ├── list_order.*    # frozen List order, highlight/scroll maths
    │   ├── button_fsm.*    # gesture recogniser
    │   ├── frame80211.*    # 802.11 header parser (AP vs STA, SSIDs)
    │   ├── camera_match.*  # vendor lookup + name patterns
    │   └── camera_oui_table.h  # GENERATED by tools/gen_camera_oui.py
    ├── hw/                 # led.*, button_task.*
    ├── scan/               # detections_shared.*, wifi_scan.*, ble_scan.*, scanner_task.*
    └── ui/                 # ui_common.*, ui_hunt.*, ui_list.*, ui_tick.*
tools/gen_camera_oui.py     # regenerates the vendor table from the IEEE registries
tools/camera_vendors.py     # curated vendor list (edit this, then regenerate)
test/test_core/             # Unity tests (pio test -e native)
platformio.ini              # PlatformIO envs: firmware + native tests
```

## Tuning

All tunables live in [signalhunter/src/core/config.h](signalhunter/src/core/config.h):

- **Band thresholds** — `kBandFarMin` … `kBandHotMin`, **hysteresis** `kBandHysteresisDb`, **bar range** `kBarMinDbm` / `kBarMaxDbm`
- **Auto-mode stickiness** — `kStickyMarginDb`
- **RSSI smoothing** — `kRssiEmaNum / kRssiEmaDen` (α = 1/3), `kEmaResetGapMs`
- **Scan timing** — `kWifiSweepPeriodMs`, `kPromiscDwellMs`, `kPromiscLockedDwellMs`, `kPromiscFirstChan` / `kPromiscLastChan`, `kBleActiveScan`, `kBleScanIntervalMs` / `kBleScanWindowMs`, `kBleSessionSec`
- **Detection freshness** — `kStaleMs`, `kLockedStaleMs`, table size `kMaxDetections`
- **Button timing** — `kShortPressMaxMs`, `kLongPressMaxMs`, `kExtraLongPressMs`, `kDoubleClickMaxGapMs`
- **Display** — `kBacklightPercent`, `kListRefreshPeriodMs`, `kListFadeMs`

### Camera vendor table

`signalhunter/src/core/camera_oui_table.h` is generated from the IEEE MA-L / MA-M / MA-S registries, filtered by the curated list in `tools/camera_vendors.py`. Each vendor has a tier:

- **CAM** — camera-focused vendors (Hikvision, Dahua, Reolink, Wyze, Amcrest, Axis, Arlo, Ring, …): a match is a strong hint and sorts first.
- **IOT** — generic chipset / IoT vendors (Espressif, Realtek, Tuya, Xiaomi, TP-Link, Amazon, Google, …) whose blocks are also used by countless no-name cameras: a weak hint.

To add or re-tier a vendor, edit `tools/camera_vendors.py` and run:

```sh
python tools/gen_camera_oui.py            # uses tools/.cache if present
python tools/gen_camera_oui.py --refresh  # re-download the registries
python tools/gen_camera_oui.py --check    # is the committed table current?
```

When a device shows up on Serial as `vendor=(unknown)` and you can confirm it is a camera, look its MAC prefix up in the IEEE registry, add the organisation to the vendor list, and regenerate. Name/SSID substrings (`TAPO`, `DS-`, `IPC…`, `CAMERA`, …) are matched case-insensitively from the list in `signalhunter/src/core/camera_match.cpp`; short tokens such as `RING` or `NEST` only match as whole words.

## Future ideas

- **SD-card logging** — the board has a slot; a CSV of detections + RSSI samples would allow after-the-fact analysis.
- **Web UI over Wi-Fi AP** — a `SignalHunter-Setup` AP for richer control without buttons.
- **LVGL's built-in ST7789 driver** (`LV_USE_ST7789`) in place of the inherited display files.
- **2.4 GHz spectrum sweep** — coarse "is there RF activity on channel N?" heatmap independent of frame parsing.

## Honest caveats

- Vendor matching is heuristic. A non-camera device with a flagged vendor will be flagged (a TP-Link router shows `[IoT TP-Link]`); many cheap cameras use generic Espressif / Realtek / Tuya blocks and only get the weak IoT tag. The hunter still surfaces them by RSSI.
- BLE devices that aren't yours and don't match a known vendor are almost always **randomised privacy MACs** (phones, watches, trackers rotating their identifier every ~15 min). The list de-prioritises locally-administered MACs for this reason.
- Indoor RF has heavy multipath. Expect dead zones and false hot-spots from reflections. Sweep slowly and trust the steady state more than transients.
- SignalHunter is for **defensive personal use** in spaces you control or have explicit permission to sweep. Don't use it where local law restricts RF monitoring.

## Third-party code

The inherited display / LVGL driver files are listed in [NOTICE.md](NOTICE.md) together with their origin.
