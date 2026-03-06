# CardputerGPIO

A general-purpose GPIO control and electronics debugging tool for the **M5Stack Cardputer** (ESP32-S3). Flash it once and get an interactive, menu-driven lab bench in your pocket — no laptop required.

---

## Features

| Profile | What it does |
|---|---|
| **IC Control Mode** | Drive address lines (A0/A1/A2) on logic ICs — 74138 decoder, 74151 mux, shift registers, binary counters |
| **Digital Output Panel** | Toggle up to 8 output pins individually or all at once with number keys |
| **Digital Input Monitor** | Live logic probe on up to 6 pins with edge counters |
| **Analog Sensor Reader** | Read up to 4 ADC channels simultaneously with voltage display and bar graphs |
| **PWM Generator** | Configurable frequency (1 Hz – 1 MHz) and duty cycle on any safe pin |
| **Signal Monitor** | Single-pin edge detection with frequency and period measurement |
| **I2C Scanner** | Scan the I2C bus and display all found device addresses |
| **Logic Analyzer** | 4-channel digital capture with adjustable sample rate and waveform display |

Pin assignments for every profile are **stored in NVS flash** and survive power cycles. Reconfigure any time without reflashing.

---

## Hardware

**M5Stack Cardputer** — no additional hardware required to run the tool itself. Connect your target circuit to the expansion header pins listed below.

### Expansion Header Pinout

```
┌─────────────────────────────────────────┐
│  G3   G4   G6  G40  G14  G39   G5      │  ← Row 1
│  5V        5V   G8   G9  G13  G15      │  ← Row 2
│  IN   GND  OUT  SDA  SCL               │
└─────────────────────────────────────────┘
```

| GPIO | Notes |
|------|-------|
| G3, G4, G5, G6 | General I/O, ADC1-capable |
| G13, G14, G15 | General I/O (avoid for ADC — ADC2, conflicts with WiFi) |
| G39, G40 | General I/O, ADC1-capable |
| G8 (SDA) | I2C data — default for I2C Scanner |
| G9 (SCL) | I2C clock — default for I2C Scanner |

**Always connect GND** between the Cardputer and your target circuit. The 3.3V logic level applies to all GPIO pins.

---

## Installation

### Requirements

- Arduino IDE 2.x
- ESP32 board package (esp32 by Espressif, version 3.x recommended)
- [M5Cardputer library](https://github.com/m5stack/M5Cardputer) v1.1.1+
- M5Unified v0.2.13+
- M5GFX v0.2.19+

### Steps

1. Install the ESP32 board package via **File → Preferences → Additional Board URLs**:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```

2. Install libraries via **Tools → Manage Libraries**: search and install `M5Cardputer`.

3. Extract `CardputerGPIO.zip` — you'll get a folder called `CardputerGPIO/`.

4. Open `CardputerGPIO/CardputerGPIO.ino` in Arduino IDE.

5. Select board: **Tools → Board → ESP32 Arduino → M5Stack-StampS3** (or M5Cardputer if listed).

6. Select the correct COM port and upload.

---

## Folder Structure

```
CardputerGPIO/
├── CardputerGPIO.ino          # Main sketch — setup/loop, profile registration
└── src/
    ├── PinManager.h / .cpp    # GPIO abstraction — safe pin registry, read/write/ADC/PWM
    ├── PinConfig.h / .cpp     # NVS-backed pin assignment storage per profile
    ├── PinConfigurator.h/.cpp # Interactive pin assignment UI widget
    ├── WiringGuide.h / .cpp   # Pre-run wiring reference screen
    ├── MenuSystem.h / .cpp    # Main menu and full launch-flow state machine
    ├── Profile.h              # Abstract base class for all profiles
    └── profiles/
        ├── ProfileICControl.h / .cpp
        ├── ProfileDigitalOut.h / .cpp
        ├── ProfileDigitalIn.h / .cpp
        ├── ProfileAnalogReader.h / .cpp
        ├── ProfilePWMGen.h / .cpp
        ├── ProfileSignalMonitor.h / .cpp
        ├── ProfileI2CScanner.h / .cpp
        └── ProfileLogicAnalyzer.h / .cpp
```

---

## Usage

### Navigation

| Key | Action |
|-----|--------|
| `j` or `↓` (Fn + `.`) | Move cursor down |
| `k` or `↑` (Fn + `;`) | Move cursor up |
| `1`–`8` | Jump directly to profile by number |
| `Enter` | Select profile / confirm |
| `Backspace` (DEL) | Go back / cancel |
| `C` | Open pin configurator (from wiring guide) |

### Launch Flow

Every profile follows the same flow:

```
Main Menu
   ↓ [Enter]
Wiring Guide  ←──────────────────────┐
   │ [Enter] Start                   │
   │ [C]     Reconfigure pins        │
   │ [DEL]   Back to menu            │
   ↓                                 │
Running Profile                      │
   │ [DEL]   Exit → back to menu     │
   └─────────────────────────────────┘
         (reconfigure returns here)
```

### Wiring Guide

Before starting, the Wiring Guide shows exactly which GPIO to connect for each signal. Pin assignments are saved per profile so you only configure once.

If you need different pins (e.g. you have a conflict or want a specific header position), press `C` to open the configurator and reassign any role.

---

## Profile Key Bindings

### IC Control Mode
| Key | Action |
|-----|--------|
| `0` / `1` / `2` | Toggle address bit A0 / A1 / A2 |
| `+` / `-` | Increment / decrement address value |
| `Enter` | Cycle IC type label (74151 mux, 74138 decoder, etc.) |

### Digital Output Panel
| Key | Action |
|-----|--------|
| `1`–`8` | Toggle output pin 1–8 |
| `A` | Set all pins HIGH |
| `Z` | Set all pins LOW |

### Digital Input Monitor
| Key | Action |
|-----|--------|
| `R` | Reset all edge counters |

### PWM Generator
| Key | Action |
|-----|--------|
| `F` / `f` | Frequency up / down (stepped presets) |
| `D` / `d` | Duty cycle up / down (5% steps) |
| `Space` | Start / stop PWM output |

### Signal Monitor
| Key | Action |
|-----|--------|
| `R` | Reset edge counter and frequency measurement |

### Logic Analyzer
| Key | Action |
|-----|--------|
| `+` / `-` | Faster / slower sample rate |

### I2C Scanner
| Key | Action |
|-----|--------|
| `S` or `Enter` | Rescan the I2C bus |

---

## Adding a New Profile

1. Create `src/profiles/ProfileMyTool.h` and `.cpp`.
2. Inherit from `Profile` and implement the five virtual methods: `onEnter`, `onExit`, `update`, `onKey`, `name`.
3. Declare `static const PinRole ROLES[]` and `static const int ROLE_COUNT` — these define what pins your profile needs and how they appear in the configurator and wiring guide.
4. In `onEnter()`, read pin assignments from `cfg->pin(0)`, `cfg->pin(1)`, etc.
5. Register in `CardputerGPIO.ino`:
   ```cpp
   ProfileMyTool profMyTool;
   // in setup():
   menuSystem.addProfile(&profMyTool, "mytool");  // "mytool" = NVS key prefix, never change
   ```

Example role declaration:
```cpp
// In header:
static const PinRole ROLES[];
static const int ROLE_COUNT = 2;
const PinRole* roles()    const override { return ROLES; }
int            roleCount() const override { return ROLE_COUNT; }

// In .cpp:
const PinRole ProfileMyTool::ROLES[] = {
    { "CLK",  PinDir::OUTPUT_ROLE, "-> device CLK pin" },
    { "DATA", PinDir::INPUT_ROLE,  "-> device DOUT pin" },
};
```

---

## Pin Safety

`PinManager` maintains a list of safe pins that are cleared for user use. All GPIO operations are guarded — attempting to use an unlisted pin logs a warning and does nothing. The safe pin list matches the physical expansion header and excludes all pins used internally by the Cardputer (display SPI, keyboard I2C, SD card, speaker, USB/JTAG, strapping pins).

To add a pin, edit `SAFE_PINS[]` in `src/PinManager.h`. Do not add pins marked as internal in the Cardputer schematic.

---

## Troubleshooting

**Black screen after selecting a profile** — ensure you are using M5Cardputer library v1.1.1+ and that `M5Cardputer.begin()` is called before any display operations. Do not call `setTextFont()` — the default font set by `begin()` is correct.

**Keyboard not responding** — confirm the board is selected as M5Stack-StampS3 (not a generic ESP32). The TCA8418 keyboard controller is only initialised by `M5Cardputer.begin()`.

**Pin reads always LOW / ADC reads 0** — verify the GPIO number matches a pin physically present on the expansion header (G3, G4, G5, G6, G13, G14, G15, G39, G40). Old pin configurations (from a previous firmware) may be stored in NVS with wrong numbers — use the pin configurator (`C` from the wiring guide) to reassign.

**I2C Scanner finds nothing** — confirm SDA/SCL are connected to G8/G9 on the expansion header and that the target device is powered. Press `S` to rescan after wiring.

**PWM frequency presets** — `F` requires Shift held. `f` (lowercase) steps down. If neither responds, ensure you are in the PWM Generator profile and the key is not being consumed by another handler.

---

## License

MIT — do whatever you want with it.
