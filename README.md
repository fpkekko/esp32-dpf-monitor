# esp32-dpf-monitor

[![License: GPL v3 + Commons Clause](https://img.shields.io/badge/license-GPL%20v3%20%2B%20Commons%20Clause-red.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Language](https://img.shields.io/badge/language-C%2B%2B-orange.svg)](https://isocpp.org/)
[![Version](https://img.shields.io/badge/version-v1.0-brightgreen.svg)](CHANGELOG)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](CONTRIBUTING)

Real-time **Diesel Particulate Filter (DPF/FAP) monitor** built on the **LILYGO T-Display-S3** (ESP32-S3 + ST7789 170×320 display). Connects to the ECU via a **BLE ELM327 OBD2 adapter** and displays soot load, regeneration status, DPF temperature, differential pressure, and engine/battery parameters on a colour screen.

Configuration is done entirely from a phone browser — no app, no cable, no internet connection required.

---

## Table of Contents

- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Wiring / Connections](#wiring--connections)
- [Software Requirements](#software-requirements)
- [Installation](#installation)
- [Configuration](#configuration)
- [OBD PIDs Reference](#obd-pids-reference)
- [Supported Vehicles](#supported-vehicles)
- [Screens Overview](#screens-overview)
- [Alarm Reference](#alarm-reference)
- [Project Structure](#project-structure)
- [Known Limitations](#known-limitations)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

---

## Features

### DPF / FAP Monitor
- Real-time soot load percentage with colour-coded bar
- Active regeneration detection with flashing alarm
- DPF temperature (°C) and differential pressure (mbar)
- Kilometres driven since last regeneration

### Regen History
- Log of the last 10 regeneration events (km, duration, type: routine / forced)
- Cumulative counters: total regens, average km, average duration

### Engine & Turbo Monitor
- Oil and coolant temperature (°C)
- Turbo / MAP pressure (bar) and engine RPM

### Battery Monitor
- Battery voltage with colour-zone scale (10.5 V — 15.0 V)
- Alternator status detection (charging / fault)
- Estimated remaining range in km

### User Interface
- 4 navigable screens with 2 physical buttons (BTN1 ← / BTN2 →)
- Semantic colours on every value: green / yellow / orange / red
- Flashing alarm bar for critical events
- Built-in WiFi Access Point: open `192.168.4.1` from any phone
- Live JSON endpoint `/data` for custom dashboards

### Configuration & Persistence
- Web UI with 4 tabs: vehicle profile, alarm thresholds, active PIDs, advanced
- All settings saved to NVS flash — survive reboots and firmware updates
- Vehicle profile switching without reflashing

---

## Hardware Requirements

| Component | Details | Notes |
|---|---|---|
| **LILYGO T-Display-S3** | ESP32-S3, ST7789 170×320 display, 16 MB flash | Recommended; other ESP32-S3 boards work with different pin definitions |
| **ELM327 BLE OBD2 adapter** | Bluetooth Low Energy, UUIDs FFF0 / FFF1 / FFF2 | See note below |
| **OBD-II port** | Present on all vehicles from 2001 (EU) | Usually under the dashboard on the driver's side |

> **OBD adapter note:** the firmware uses **BLE** (Bluetooth Low Energy) with standard UUIDs FFF0/FFF1/FFF2.
> Bluetooth Classic (BT 2.0 SPP) adapters are **not** supported.
> Tested adapters: **Vgate iCar Pro BLE**, **LELink BLE**, ELM327 BLE clones v1.5+.

---

## Wiring / Connections

The LILYGO T-Display-S3 has the display already integrated on the board. The only external connections needed are the two navigation buttons.

### Navigation Buttons

| ESP32-S3 GPIO | Signal | Button |
|---|---|---|
| GPIO 0 | BTN1 | Previous screen — connect between GPIO and GND |
| GPIO 14 | BTN2 | Next screen — connect between GPIO and GND |

> GPIOs 0 and 14 have internal pull-up enabled in software. Simply connect the button between the pin and GND with no external resistors.

### Display Pins (T-Display-S3 — reference only)

The ST7789 display is already wired internally on the LILYGO board. The pins used by the firmware are:

| Signal | GPIO | Notes |
|---|---|---|
| PIN_POWER_ON | 15 | Display power — MUST be driven HIGH |
| TFT Backlight | 38 | Display brightness control |

These pins are already defined in the `User_Setup.h` file included with the TFT_eSPI library for the T-Display-S3; no changes are needed if using the recommended board.

---

## Software Requirements

### Arduino IDE
- **Arduino IDE 2.x** (recommended) or 1.8.19+
- **ESP32 Arduino Core** v2.0.14 or later
  - Add in *File → Preferences → Additional board manager URLs*:
    ```
    https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
    ```
  - Install via *Tools → Board → Boards Manager* → search `esp32`

### Required Libraries

| Library | Min. version | Installation |
|---|---|---|
| `TFT_eSPI` | 2.5.x | Library Manager: search `TFT_eSPI` by Bodmer |
| `BLEDevice` | included in ESP32 core | Automatic with ESP32 Arduino Core |
| `WiFi` | included in ESP32 core | Automatic |
| `WebServer` | included in ESP32 core | Automatic |
| `Preferences` | included in ESP32 core | Automatic |

### TFT_eSPI Configuration for T-Display-S3

After installation, edit `User_Setup_Select.h` in the TFT_eSPI library folder to include the correct setup for the T-Display-S3, or use the TFT_eSPI library already configured in the official LILYGO repository.

If using the generic library, edit `User_Setup.h`:

```cpp
#define ST7789_DRIVER
#define TFT_WIDTH  170
#define TFT_HEIGHT 320

#define TFT_MOSI  11   // T-Display-S3
#define TFT_SCLK  12
#define TFT_CS    10
#define TFT_DC    42
#define TFT_RST   -1

#define TFT_BL   38
#define TFT_BACKLIGHT_ON HIGH

#define SPI_FREQUENCY  80000000
```

> Verify the exact pins from the datasheet of your specific T-Display-S3 — hardware revisions exist with slightly different pin assignments.

---

## Installation

1. **Clone the repository**
   ```bash
   git clone https://github.com/fpkekko/esp32-dpf-monitor.git
   cd esp32-dpf-monitor
   ```

2. **Install the libraries** listed in [Software Requirements](#software-requirements)

3. **Configure TFT_eSPI** for your board (see section above)

4. **Open the sketch** in Arduino IDE:
   ```
   dpf_monitor/dpf_monitor.ino
   ```

5. **Select the board:** `Tools → Board → esp32 → LILYGO T-Display-S3`
   - If not listed, select `ESP32S3 Dev Module` with these settings:
     - Flash Size: 16MB
     - Partition Scheme: 16M Flash (3MB APP / 9.9MB FATFS)
     - PSRAM: OPI PSRAM

6. **Select the serial port** (`Tools → Port`) by connecting the T-Display-S3 via USB

7. **Upload the firmware** with the *Upload* button (Ctrl+U)

8. **First boot:** the device creates the WiFi Access Point `DPF-Monitor`. Connect from your phone and open `192.168.4.1` to set the vehicle profile and the MAC address of your BLE adapter.

---

## Configuration

### Initial Connection

1. Power on the device (display lights up)
2. On your phone, connect to the WiFi network: **`DPF-Monitor`** — password: `dpfmonitor`
3. Open a browser and navigate to **`http://192.168.4.1`**

### Web UI Tabs

| Tab | What to configure |
|---|---|
| **Vehicle** | Select the active vehicle profile (Citroën / Opel / ...) |
| **Thresholds** | Alarm thresholds: DPF %, differential pressure, temperatures, battery voltage |
| **PIDs** | View the active OBD PIDs and calculation formulas for the selected vehicle |
| **Advanced** | BLE adapter MAC address, polling interval (ms), display brightness, boot screen |

All settings are saved to the ESP32 NVS flash and survive reboots and firmware updates (as long as the NVS partition is not erased).

### Live JSON Data

With the device connected to OBD, open `http://192.168.4.1/data` for a real-time JSON snapshot:

```json
{
  "car": "Citroën C4 Spacetourer",
  "dpfPct": 62,
  "dpfTemp": 487,
  "dpfDiff": 28,
  "dpfRegen": false,
  "dpfKm": 340,
  "oilTemp": 93,
  "coolant": 89,
  "turbo": 1.40,
  "rpm": 1860,
  "battV": 14.1,
  "altOk": true,
  "engine": true
}
```

---

## OBD PIDs Reference

### Citroën C4 Spacetourer 2019 — Delphi DCM6.2A

Protocol: ISO 15765-4 CAN 11bit 500 kbaud (`ATSP6`), ECU header: `7E0`

| Parameter | PID (hex) | Formula | Status |
|---|---|---|---|
| Soot load % | `2102FF` | `A / 255 × 100` | confirmed |
| Active regeneration | `2102FE` | `bit 0 = 1 → regen ON` | confirmed |
| DPF temperature | `210261` | `(A×256 + B) − 400` °C | needs live verification |
| Differential pressure | `210262` | `(A×256 + B) / 10` mbar | needs live verification |
| Km since last regen | `21026D` | `A×256 + B` km | confirmed |
| Oil temperature | `015C` | `A − 40` °C | standard OBD2 |
| Turbo pressure (MAP) | `010B` | `(A − 101) / 100` bar | standard OBD2 |
| Engine RPM | `010C` | `(A×256 + B) / 4` | standard OBD2 |
| Coolant temperature | `0105` | `A − 40` °C | standard OBD2 |

### Opel Astra J 2011 — Delco E87

Protocol: ISO 15765-4 CAN 11bit 500 kbaud (`ATSP6`), ECU header: `7E0`

| Parameter | PID (hex) | Formula | Status |
|---|---|---|---|
| Soot load % | `22336A` | `A` (0–100%) | confirmed |
| Active regeneration | `2220FA` | `bit 0 = 1 → regen ON` | needs live verification |
| DPF temperature | `222010` | `(A×256 + B) / 10 − 40` °C | needs live verification |
| Km since last regen | `22336B` | `A×256 + B` km | confirmed |
| Oil temperature | `015C` | `A − 40` °C | standard OBD2 |
| Turbo pressure (MAP) | `010B` | `(A − 101) / 100` bar | standard OBD2 |
| Engine RPM | `010C` | `(A×256 + B) / 4` | standard OBD2 |
| Coolant temperature | `0105` | `A − 40` °C | standard OBD2 |

> PIDs marked as "needs live verification" were identified from community research; the formula may vary slightly depending on model year and specific ECU variant. Use the `/data` endpoint to observe raw values and confirm accuracy.

---

## Supported Vehicles

| Vehicle | Engine | Year | ECU | Status |
|---|---|---|---|---|
| Citroën C4 Spacetourer | 2.0 BlueHDi 163 hp | 2019 | Delphi DCM6.2A | confirmed |
| Opel Astra J | 1.7 CDTI A17DTR 110 hp | 2011 | Delco E87 | confirmed |

The active profile can be changed at any time from the Web UI without reflashing.

### Adding a New Vehicle Profile

Open `dpf_monitor/dpf_monitor.ino` and add an entry to the `PROFILES[]` array:

```cpp
{
  "Vehicle Name",         // name shown on display
  "2.0 TDI 150hp",        // engine descriptor
  "2021",                 // year
  "Bosch EDC17",          // ECU name
  "ATSP6",                // ELM327 protocol command
  "7E0",                  // engine ECU CAN header
  "22XXYY",               // PID: soot %
  "22XXYY",               // PID: regen active
  "22XXYY",               // PID: DPF temperature
  "22XXYY",               // PID: differential pressure
  "22XXYY",               // PID: km since last regen
  false,                  // has AdBlue?
  0                       // DPF max grams (0 = use % directly)
}
```

Then open a Pull Request — new vehicle profiles are very welcome!

---

## Screens Overview

```
┌─────────────────────────────────┐  ┌─────────────────────────────────┐
│ Screen 0 — DPF / FAP            │  │ Screen 1 — Regen History        │
│                                 │  │                                 │
│          62%                    │  │  47  total regens               │
│  ████████████░░░░░░  bar        │  │  avg: 320 km  23 min            │
│                                 │  │                                 │
│ TEMP   PRESS DIFF   KM REGEN    │  │  #   km    duration  type       │
│ 487°C   28 mbar     340 km      │  │  47  340   22 min    routine    │
│                                 │  │  46  295   28 min    forced     │
│  [ regeneration: inactive ]     │  │  ...                            │
└─────────────────────────────────┘  └─────────────────────────────────┘

┌─────────────────────────────────┐  ┌─────────────────────────────────┐
│ Screen 2 — Engine & Turbo       │  │ Screen 3 — Battery              │
│                                 │  │                                 │
│ OIL TEMP      COOLANT TEMP      │  │          14.1V                  │
│  93°C          89°C             │  │  ████████████████  zone         │
│                                 │  │  10.5  11.8  12.6  13.8  14.8  │
│  TURBO BAR       RPM            │  │                                 │
│   1.4 bar        1860           │  │  [ ALTERNATOR OK    14.1V ]    │
│                                 │  │                                 │
│  [ all parameters normal ]      │  │  estimated range: ~480 km       │
└─────────────────────────────────┘  └─────────────────────────────────┘

         BTN1 = previous screen          BTN2 = next screen
```

---

## Alarm Reference

All alarms display a flashing bar at the bottom of the screen with a colour-coded message. Thresholds are configurable from the Web UI.

| Condition | Screen | Message |
|---|---|---|
| DPF >= 85% | 0 | `DPF FULL - REGENERATION NEEDED` |
| Regeneration active | 0 | `REGEN IN PROGRESS - KEEP 60+ km/h` |
| Critical differential pressure | 0 | `TURBO OVERPRESSURE` |
| Repeated forced regenerations | 1 | `REPEATED FORCED REGENS - CHECK DPF` |
| Critical oil temperature | 2 | `OIL CRITICAL - STOP THE VEHICLE` |
| Turbo overpressure | 2 | `TURBO OVERPRESSURE` |
| Alternator not charging | 3 | `ALTERNATOR FAULT - SEE A MECHANIC` |
| Low battery | 3 | `BATTERY LOW` |

---

## Project Structure

```
esp32-dpf-monitor/
├── LICENSE                    # GPL v3 + Commons Clause
├── README.md                  # this file
└── dpf_monitor/
    └── dpf_monitor.ino        # main sketch (single source file)
```

---

## Known Limitations

- **Bluetooth Classic not supported:** the firmware uses BLE exclusively. ELM327 BT Classic (SPP) adapters will not work.
- **PIDs are not universal:** proprietary PIDs (mode 21/22) vary by year, market, and ECU software version. The included PIDs were tested on the two specified vehicles; on different variants they may return incorrect data or no data at all.
- **Read-only ECU access:** the device is completely read-only. It never sends write commands to the ECU.
- **One vehicle per session:** monitoring two vehicles simultaneously is not possible. Changing the profile requires stopping and accessing the Web UI.
- **Slow adapters:** some cheap ELM327 clones have high BLE latency (> 500 ms per response). In these cases, increase the `POLL_MS` parameter from the Advanced tab in the Web UI.
- **No display after TFT_eSPI library update:** each library update overwrites `User_Setup.h`. Back up the file before updating.

---

## Troubleshooting

**OBD not found at startup**
- Verify the adapter is BLE (not Bluetooth Classic / WiFi)
- Manually set the BLE MAC address from the Web UI → Advanced tab
- Some adapters need to be paired at least once via phone before the ESP32 can connect

**PIDs return no data**
- Send `ATSP0` (automatic protocol) and retry — some vehicles use different CAN speeds
- Verify the CAN header matches your ECU (`ATSH 7E0` for most Euro 5/6 diesels)
- Use a phone OBD app (Torque Pro, Car Scanner) to confirm which PIDs respond on your specific vehicle

**Display does not turn on or shows distorted image**
- Verify `PIN_POWER_ON` (GPIO 15) is driven HIGH in code — required on T-Display-S3
- Check the SPI pins in `User_Setup.h` of the TFT_eSPI library
- Try lowering `SPI_FREQUENCY` to `40000000`

**Web UI does not load**
- Verify the phone is connected to the `DPF-Monitor` WiFi network and not to your home network
- Navigate directly to `http://192.168.4.1` — do not use `dpfmonitor.local`

---

## Contributing

Contributions are welcome, in particular:

- **New vehicle profiles** — if you have confirmed PIDs for your car, open a PR adding an entry to `PROFILES[]`
- **PID verification** — test the PIDs marked "needs live verification" on your vehicle and report results in an issue
- **Bug fixes and improvements** — standard flow: fork → branch → PR

Open an issue before starting significant changes to align on the approach.

### Developer Notes

- The `sendOBD()` function has a default timeout of 1000 ms — increase it for slow adapters
- The web server and BLE polling share the same core; for heavy additions consider `xTaskCreate` to assign tasks to separate cores
- All `Preferences` keys must be at most 15 characters (ESP32 NVS limit)
- The display uses `TFT_eSPI` with the ST7789 driver at 170×320 px in rotation 1 (landscape)

---

## License

Copyright (c) 2026 [Francesco](https://github.com/fpkekko)

This project is distributed under the **GNU General Public License v3.0** with the **Commons Clause** condition.

| Use | Permitted? |
|---|---|
| Personal use | Yes |
| Modify the code | Yes |
| Share modifications | Yes — under the same license |
| Open Pull Requests | Yes, welcome |
| Sell this software | No |
| Include in a commercial product | No |
| Offer as a paid service | No |

See the [LICENSE](LICENSE) file for the full terms.

---

## Disclaimer

This project is provided "as is", without any warranty. OBD communication is read-only — the device never sends write commands to the vehicle ECU. Always ensure you are driving safely before consulting the display. The author is not responsible for any damage to the vehicle or OBD adapter.

---

*Built with coffee and a clogged DPF — by [fpkekko](https://github.com/fpkekko)*
