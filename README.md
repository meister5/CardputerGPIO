# CardputerGPIO

A GPIO workbench for the **M5Stack Cardputer ADV**. Twenty-one tools behind one
menu: drive pins, measure them, talk to buses, generate signals, and log the
results to CSV — without writing a sketch for each thing you want to try.

Everything runs on the device, and there is nothing to install beyond the three
M5Stack libraries. Optionally it also serves a **web interface** over WiFi —
the same tools, driven from a browser with a real keyboard and a big screen,
which is a much nicer place to assign pins. See [docs/WEB.md](docs/WEB.md).

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
when the IDE offers. **That is the whole list.** 1-Wire, DHT, NeoPixel, IR, SPI
and the JSON the web interface speaks are all implemented in this repository
rather than pulled from libraries, so there is no version roulette and the
timing stays where it can be read. WiFi, WebServer, ESPmDNS, SD and Preferences
ship with the ESP32 core itself and need no action in Library Manager.

### 3. Board settings

One setting **must** be changed from the default:

**Tools → Partition Scheme → "8M with spiffs (3MB APP/1.5MB SPIFFS)"**

The web interface brings in WiFi, the HTTP server and mDNS, which together add
about 630 kB. That does not fit the board's default 1.2 MB app partition, and
the error you get if you forget is the unhelpful `text section exceeds
available space in board`. With the 8 MB scheme the build sits at about 41%.

If you would rather not have the radio at all, set `CG_ENABLE_WEB` to `0` in
[`src/core/Config.h`](src/core/Config.h). That build is 703 kB and fits the
default partition; everything except the web interface is identical.

| Setting            | Value                                      |
| ------------------ | ------------------------------------------ |
| Partition scheme   | **8M with spiffs (3MB APP)** — required for the web build |
| PSRAM              | **Disabled** — the StampS3A has none, and enabling it will not boot |

### 4. Flash it

Clone or download this repository, open `CardputerGPIO.ino`, pick the port and
press upload. The sketch is about 1.38 MB, roughly 41% of the 3 MB partition —
or 703 kB with `CG_ENABLE_WEB` set to `0`.

<details>
<summary>Building from the command line instead</summary>

```bash
arduino-cli core install m5stack:esp32 \
  --additional-urls https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json
arduino-cli lib install M5Cardputer M5Unified M5GFX

FQBN=m5stack:esp32:m5stack_cardputer:PartitionScheme=default_8MB,FlashSize=8M
arduino-cli compile --fqbn "$FQBN" .
arduino-cli upload  --fqbn "$FQBN" -p /dev/ttyACM0 .

# the lean build, no radio, fits the default partition
arduino-cli compile --fqbn m5stack:esp32:m5stack_cardputer \
  --build-property "compiler.cpp.extra_flags=-DCG_ENABLE_WEB=0" .
```

Or build both release binaries at once:

```bash
tools/package.sh
```

`tools/package.sh` writes:

| File | Use |
|---|---|
| `dist/CardputerGPIO-app.bin` | **M5Launcher** — copy to SD, or upload via WebUI/OTA |
| `dist/CardputerGPIO-merged.bin` | **M5Burner** custom firmware, or `esptool` at offset `0x0` |

After editing `web/index.html`, regenerate the embedded copy with
`python3 web/build_assets.py`.
</details>

### 5. Or install it with M5Launcher

[M5Launcher](https://github.com/bmorcelli/Launcher) keeps several firmwares on
one device and boots whichever you pick, so you do not have to reflash to swap
between them. It installs application binaries into an OTA app partition, which
means the **app** binary is the one to hand it — not the merged image:

A prebuilt copy sits at the repo root, refreshed at each release, so you do
not have to build anything:

<https://raw.githubusercontent.com/meister5/CardputerGPIO/main/CardputerGPIO-app.bin>

That URL is directly downloadable, which is what an `OTA > Favorites` entry
needs. To install from SD instead:

1. Copy `CardputerGPIO-app.bin` to a FAT32 SD card (MBR, 32 GB or less).
2. In Launcher, open `SD`, select the file, choose `Install`.

It also works through `WUI` (browser upload) or as an `OTA > Favorites` entry
pointing at a release asset URL. The web build is about 1.38 MB, well inside a
standard OTA slot, and `tools/package.sh` fails rather than emit an image that
has outgrown one.

Nothing here needs a filesystem partition: settings live in NVS and the SD card
is accessed directly, so there is no SPIFFS image for Launcher to install
alongside the app and nothing to lose when you switch firmware. The firmware
never writes the OTA boot partition either, so it cannot brick a Launcher
install — return to Launcher the way Launcher documents for your device.

The partition scheme above applies to a **direct** flash over USB. Under
Launcher the layout is Launcher's, and it sizes the OTA slot itself.

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

## The web interface

Open **Web Interface** on the device, pick *access point* or *join a network*,
and press Enter. The screen shows the network name, the password and the
address to open.

It is not a second implementation of the toolbox. The browser drives the same
state machine the keyboard drives, and sees the device's actual framebuffer
streamed as changed tiles — so every tool, every dialog and anything added
later shows up with no web code written for it. Your arrow keys, Esc, Tab,
Enter, Backspace and F1–F10 map straight through, which means you do not need
the Fn layer.

On top of that it adds native pages for the things a browser is genuinely
better at: assigning pins for **every** tool from one table, editing settings,
naming and loading setups, reading the pinout, and downloading CSV captures.

Two costs, both real:

* **G13 and G15 stop working as analog inputs** while the radio is up — ADC2
  shares its hardware with WiFi. They are withdrawn from the pin picker and an
  already-saved assignment turns red on the wiring screen, rather than quietly
  reading zero. Nothing else is affected.
* **It needs the 3 MB app partition**, as described above.

**Set a password if you put it on a shared network.** Out of the box there is
none, which is the right default on the board's own access point — that radio
is already WPA2. On a network you share, anyone who can reach the page can
drive every pin, exactly as if they had picked the device up. Settings ->
Access sets one; the username is `cardputer`. Forgot it? Open the Web
Interface tool and press `W`, which clears the password without touching your
saved pin assignments. Full details and the HTTP API are in
[docs/WEB.md](docs/WEB.md).

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
| **Web Interface** | access point or join a network; scan, connect, show the address, clear the web password |

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

**A pin that cannot do its job says so.** The wiring screen shown before every
tool starts marks an unusable assignment in red with the reason — locked, held
back for the microSD bus, no ADC on that pin, or ADC2 while the radio is up —
instead of letting the tool run and report zeros.

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
  Config.h               build switches (CG_ENABLE_WEB)
src/ui/
  Shell.{h,cpp}          menu, search, arming, help, the frame loop
  PinPicker.{h,cpp}      pin assignment with conflict detection
  WiringGuide.{h,cpp}    list and physical-header views of the wiring
src/net/
  WebPortal.{h,cpp}      WiFi lifecycle, HTTP routes and the API
  Mirror.{h,cpp}         framebuffer tile-delta encoder
  Json.h                 a JSON writer, so there is no dependency for it
  WebAssets.h            generated: the interface, gzipped into flash
src/tools/               twenty-one tools, one pair of files each
web/index.html           the web interface source; build_assets.py embeds it
docs/HARDWARE.md         the pin map, the quirks, and the sources
docs/WEB.md              the web interface and its HTTP API
```

A tool declares its **roles** — "this one is an output that goes to your
driver, this one is an ADC input" — and gets pin assignment, conflict checking,
a wiring diagram, persistence and the arm prompt for free. It implements
`draw()` and `onKey()`, and optionally `logHeader()` / `logRow()` to gain CSV
logging. It never touches the keyboard, never pushes the sprite, and never
calls `M5Cardputer.update()`; the shell owns all of that, which is why the
frame rate and the navigation are the same everywhere — and why a new tool
appears in the web interface without a line of web code.

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
