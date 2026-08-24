# CardputerGPIO

A GPIO workbench for the **M5Stack Cardputer ADV**. Twenty tools behind one
menu: drive pins, measure them, talk to buses, generate signals, and log the
results to CSV — without writing a sketch for each thing you want to try.

Everything runs on the device. No WiFi, no companion app, no libraries beyond
the three M5Stack ones.

> **Board support:** Cardputer **ADV** only. The pin map, the keyboard driver
> and the I²C layout all differ from the original Cardputer v1.1 — see
> [docs/HARDWARE.md](docs/HARDWARE.md). The firmware detects the board at boot
> and says so in red if it is running somewhere it does not belong.

---

## Installing (Arduino IDE)

### 1. Board support

**File → Preferences → Additional boards manager URLs**, add:

```
https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json
```

Then **Tools → Board → Boards Manager**, search for **M5Stack** and install
**M5Stack by M5Stack official** — version **3.2.2 or newer** (developed and
tested against 3.3.9). Older cores do not know about the ADV, and their LEDC
API is the pre-3.x one this code does not use.

Select **Tools → Board → M5Stack → M5Cardputer**. That FQBN covers the ADV as
well; the ADV is distinguished at runtime, not at compile time.

### 2. Libraries

**Sketch → Include Library → Manage Libraries**, install these three:

| Library         | Minimum | Tested  | Why                                     |
| --------------- | ------- | ------- | --------------------------------------- |
| **M5Cardputer** | 1.1.1   | 1.1.1   | keyboard, display and power for this board |
| **M5Unified**   | 0.2.8   | 0.2.20  | board detection, I²C, speaker, power     |
| **M5GFX**       | 0.2.10  | 0.2.27  | display driver and the `M5Canvas` sprite |

Installing **M5Cardputer** pulls in the other two as dependencies — say yes
when the IDE offers. Nothing else is needed: 1-Wire, DHT, NeoPixel, IR and SPI
are all implemented in this repository rather than pulled from libraries, so
there is no version roulette and the timing stays where it can be read.

### 3. Board settings

The defaults that come with the M5Cardputer board selection are correct — you
do not need to change anything. Two worth knowing about:

| Setting            | Value                                      |
| ------------------ | ------------------------------------------ |
| PSRAM              | **Disabled** — the StampS3A has none, and enabling it will not boot |
| Partition scheme   | Default (1.2 MB app) — the sketch uses about half of it |

### 4. Flash it

Clone or download this repository, open `CardputerGPIO.ino`, pick the port and
press upload. The sketch is about 700 kB, roughly half the app partition.

<details>
<summary>Building from the command line instead</summary>

```bash
arduino-cli core install m5stack:esp32 \
  --additional-urls https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json
arduino-cli lib install M5Cardputer M5Unified M5GFX

arduino-cli compile --fqbn m5stack:esp32:m5stack_cardputer .
arduino-cli upload  --fqbn m5stack:esp32:m5stack_cardputer -p /dev/ttyACM0 .
```
</details>

---

## The keyboard

This keyboard has **no arrow keys and no ESC key**. They are printed on the
keycaps as an Fn layer, and the library does not resolve it for you — so this
firmware does:

```
  Fn + ;   up          Fn + ,   left        Fn + `   ESC
  Fn + .   down        Fn + /   right       Fn + Aa  caps lock
  Fn + 1-0 F1 - F10    Backspace  back      Fn + Bksp  forward delete
```

Held keys auto-repeat. **F1** opens help from anywhere — the global key
reference in the menu, and a page about the tool itself when one is running.
**F2** starts and stops CSV logging.

---

## The tools

### Digital

| Tool | What it does |
| ---- | ------------ |
| **Pin Dashboard** | every exposed pin, live, on one screen — level, mode and which tool owns it |
| **Digital Out** | up to 8 outputs by hand, plus pattern generators (walk, bounce, count, alternate) for clocking something without a second device |
| **Digital In** | 6-channel logic probe with pull-up/down and **interrupt-driven** edge counts, good into the tens of kHz |
| **Decoder / Mux** | drives address lines for a 74HC138, CD4051, 74HC151 or 74HC154 and reads back the selected line; walks every channel and builds a table |

### Analog

| Tool | What it does |
| ---- | ------------ |
| **Analog In** | 4-channel ADC in **calibrated millivolts**, with oversampling, min/max hold and divider presets for reading past 3.3 V |
| **Multimeter** | volts, continuity (with the onboard beeper), resistance against a reference resistor, and a logic-level probe |

### Signal

| Tool | What it does |
| ---- | ------------ |
| **PWM Generator** | 1 Hz – 10 MHz with the LEDC resolution derived from the frequency, so the high end actually works |
| **Servo Driver** | 4 channels, in degrees or raw microseconds, with sweep |
| **Stepper Driver** | STEP/DIR drivers (A4988, DRV8825, TMC) or a 4-wire coil sequence (28BYJ-48), with speed and step modes |
| **Frequency Counter** | hardware **PCNT** counting — MHz-capable rather than ISR-limited — plus duty cycle and a running total |
| **Logic Analyzer** | 4 channels, 1024 samples, **10 kSa/s to 4 MSa/s** burst capture with a real trigger, then pan and zoom through the result |
| **NeoPixel** | WS2812/SK6812 strips over RMT, five effects, GRB/RGB order and a current estimate so you know when to stop hanging it off the 5 V pin |

### Bus

| Tool | What it does |
| ---- | ------------ |
| **I2C Explorer** | scans **both** buses (system G8/G9 and Grove G1/G2), names known addresses, then dumps and writes registers |
| **UART Terminal** | terminal with hex view, configurable baud and swappable TX/RX, plus a **USB bridge** mode that turns the Cardputer into a USB-to-TTL adapter with a screen |
| **SPI Probe** | clock arbitrary bytes out and read MISO back, for identifying a flash chip or a sensor |
| **1-Wire / DHT** | DS18B20 (including its ROM code), DHT11 and DHT22, bit-banged so the timing is visible |
| **IR Transmitter** | NEC, Sony SIRC and RC5 on the **onboard emitter** — no wiring at all, just aim the top edge |

### System

| Tool | What it does |
| ---- | ------------ |
| **Board Info** | chip, memory, battery, the full header table and what owns every internal pin |
| **Saved Setups** | snapshot every tool's pin assignments under a name and recall it — one setup per rig |
| **Settings** | brightness, key beep, log interval, and the two safety switches |

---

## Safety

This thing gets wired to real hardware, so a few things are deliberate:

**G8 and G9 are refused everywhere.** They carry the system I²C bus that the
keyboard controller sits on. Driving them takes the keyboard down with it and
the only way back is a reflash. They are visible in the pin table and in the
I²C scanner; they are never offered as GPIO.

**Outputs are armed, not assumed.** Any tool that can drive a pin asks for
confirmation before it starts, showing the pins it is about to take. A pin set
saved weeks ago cannot start sourcing current the moment you press Enter. Turn
it off in Settings if you find it tiresome.

**The microSD pins are held back.** G14, G39 and G40 are on the EXT header
*and* on the card reader's SPI bus. They stay out of the pin pools until you
turn on *allow SD pins*, and CSV-to-card logging refuses to mount while a tool
holds one of them.

**Everything is 3.3 V.** The 5 V pins on the headers are supply rails; the GPIO
is not 5 V tolerant, and nothing in the firmware can protect you from that.

---

## Logging

Press **F2** inside any tool that measures something. It writes CSV to the
microSD card *and* to USB serial at 115200:

```
ms,ch0_mV,ch1_mV,ch2_mV,ch3_mV
0,3298.0,1651.0,,
200,3297.0,1649.0,,
```

Files are named `/<toolid>NNN.csv` and never overwrite a previous run. If there
is no card — or its pins are in use — logging carries on to serial alone and
says why. The sample interval is in Settings (20 ms to 5 s).

Analog In, Multimeter, Digital In, Frequency Counter, 1-Wire/DHT and Decoder/Mux
log; the rest have nothing meaningful to put in a row.

---

## How it is put together

```
CardputerGPIO.ino        registers tools; the loop is three lines
src/core/
  Board.{h,cpp}          the pin map and every safety decision
  Keys.{h,cpp}           Fn layer, auto-repeat, Ctrl/Shift normalisation
  UI.{h,cpp}             one full-screen sprite + the widget kit
  Pins.{h,cpp}           safety-gated GPIO, calibrated ADC, LEDC, PCNT
  Settings.{h,cpp}       NVS preferences and per-tool pin assignments
  Logger.{h,cpp}         CSV to SD and/or serial
  Tool.{h,cpp}           the contract every screen implements
src/ui/
  Shell.{h,cpp}          menu, search, arming, help, the frame loop
  PinPicker.{h,cpp}      pin assignment with conflict detection
  WiringGuide.{h,cpp}    list and physical-header views of the wiring
src/tools/               twenty tools, one pair of files each
docs/HARDWARE.md         the pin map, the quirks, and the sources
```

A tool declares its **roles** — "this one is an output that goes to your
driver, this one is an ADC input" — and gets pin assignment, conflict checking,
a wiring diagram, persistence and the arm prompt for free. It implements
`draw()` and `onKey()`, and optionally `logHeader()` / `logRow()` to gain CSV
logging. It never touches the keyboard, never pushes the sprite, and never
calls `M5Cardputer.update()`; the shell owns all of that, which is why the
frame rate and the navigation are the same everywhere.

Adding a tool is one header, one source file, and one `shell.add()` line.

## Adding your own tool

```cpp
class ToolThing : public cg::Tool {
    const char* id()    const override { return "thing"; }   // <= 8 chars, permanent
    const char* name()  const override { return "My Thing"; }
    const char* blurb() const override { return "does a thing"; }
    cg::Cat     cat()   const override { return cg::Cat::Digital; }

    const cg::Role* roles()     const override { return ROLES; }
    int             roleCount() const override { return 1; }

    void draw() override { cg::ui.header("My Thing"); /* ... */ }
    bool onKey(const cg::KeyEvent& ev) override { return false; }
};
```

`id()` is a persistence key: it is what saved pin assignments are filed under,
it is capped at 8 characters by the 15-character NVS key limit, and renaming
one orphans the user's saved pins.

---

## License

MIT — see [LICENSE](LICENSE).
