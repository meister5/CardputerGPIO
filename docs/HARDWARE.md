# Cardputer ADV hardware notes

Everything in this firmware that touches a pin comes out of the table below.
It is reproduced here so you can check it against your own board before wiring
anything expensive to it.

The authoritative copy lives in [`src/core/Board.cpp`](../src/core/Board.cpp) —
if the two ever disagree, the code is what runs.

## Which board is this

The Cardputer ADV is the second-generation Cardputer. It is **not** pin
compatible with the original v1.1 in the places that matter here.

|                | Cardputer v1.1        | Cardputer ADV                 |
| -------------- | --------------------- | ----------------------------- |
| Module         | StampS3               | **StampS3A**                  |
| SoC            | ESP32-S3FN8           | ESP32-S3FN8                   |
| Flash / PSRAM  | 8 MB / none           | 8 MB / **none**               |
| Keyboard       | 74HC138 GPIO matrix   | **TCA8418 I²C keypad @ 0x34** |
| Keyboard pins  | G3–G7, G8, G9, G11, G13, G15 | **G8/G9 (I²C) + G11 (INT)** |
| Display        | 240×135 ST7789V2      | 240×135 ST7789V2              |
| IR emitter     | G44                   | G44                           |

The keyboard change is the important one. On the original, the matrix occupies
most of the EXT header; on the ADV those pins are free, and instead **G8 and G9
carry a system I²C bus** shared by the keyboard controller, the IMU and the
audio codec. Driving either of them as GPIO takes the keyboard down with it,
and the only way back is a reflash — which is why this firmware refuses them
everywhere rather than warning about them.

`boardBegin()` checks `M5.getBoard() == board_M5CardputerADV` at runtime. On any
other board the board-info screen says so in red, because the pin map would be
wrong.

## EXT header (2.54 mm, 2×7)

Looking at the connector with pin 1 at the top left:

| Pin | Signal | Silk    | Notes                          | Pin | Signal | Silk    | Notes                    |
| --- | ------ | ------- | ------------------------------ | --- | ------ | ------- | ------------------------ |
| 1   | G3     | RESET   | free, ADC1                     | 2   | 5VIN   |         | power in                 |
| 3   | G4     | INT     | free, ADC1                     | 4   | GND    |         |                          |
| 5   | G6     | BUSY    | free, ADC1                     | 6   | 5VOUT  |         | power out                |
| 7   | G40    | SCK     | **shared with microSD CLK**    | 8   | G8     | I2C_SDA | **system I²C — locked**  |
| 9   | G14    | MOSI    | **shared with microSD MOSI**   | 10  | G9     | I2C_SCL | **system I²C — locked**  |
| 11  | G39    | MISO    | **shared with microSD MISO**   | 12  | G13    | UART_TX | free, ADC2               |
| 13  | G5     | CS      | free, ADC1                     | 14  | G15    | UART_RX | free, ADC2               |

## Grove port (HY2.0-4P)

| Wire   | Signal | Notes                                   |
| ------ | ------ | --------------------------------------- |
| Black  | GND    |                                         |
| Red    | 5V     |                                         |
| Yellow | G2     | ADC1; M5Unified's external I²C **SDA**  |
| White  | G1     | ADC1; M5Unified's external I²C **SCL**  |

Note the ordering: on this port SCL is the pin closer to 5V, which is the
opposite of what most Grove diagrams lead you to expect.

## Pins you cannot have

| Pin(s)              | Owner                                              |
| ------------------- | -------------------------------------------------- |
| G8, G9              | system I²C: keyboard (TCA8418 @0x34), IMU, ES8311   |
| G11                 | keyboard interrupt                                 |
| G33–G37, G38        | display bus; G38 also gates the backlight/RGB power |
| G41, G42, G43, G46  | audio codec, speaker, microphone                   |
| G44                 | IR emitter (used by the IR tool, no wiring needed)  |
| G10                 | battery voltage divider                            |
| G12                 | microSD chip select                                 |
| G14, G39, G40       | microSD SPI **and** EXT header — see below          |

## The microSD overlap

G14, G39 and G40 appear on the EXT header *and* on the card reader's SPI bus.
Using them for something else while a card is inserted means two drivers on the
same wires.

The firmware keeps them out of the pin pools by default. Turn on
**Settings → allow SD pins** to unlock them, and take the card out first.

CSV logging to card checks the reverse direction: if a running tool has claimed
any of those pins, mounting the card is refused and logging falls back to
serial with the reason on screen.

## ADC

ESP32-S3 has two ADC blocks:

* **ADC1** — GPIO 1–10. Usable while WiFi is up.
* **ADC2** — GPIO 11–20. Shared with the WiFi radio.

With the radio off — the default — ADC2 pins work here as well as ADC1 ones.
Turning on the web interface changes that: **G13 and G15 stop being usable as
analog inputs for as long as the radio is up**. `pinAdcOk()` withdraws them
from the ADC pool, the pin picker stops offering them, and the wiring screen
marks an already-saved assignment in red rather than letting the tool report
zeros. G14 is the third ADC2 pin on the header, but it is held back for the
microSD bus anyway.

**G39 and G40 have no ADC at all.** The v1 firmware offered them as analog
inputs; they read nothing. That is fixed in the table.

Readings use `analogReadMilliVolts()`, which applies the per-chip calibration
burned into eFuse. The naive `raw / 4095 * 3.3` is good for a couple of hundred
millivolts of error near the rails, because the 12 dB attenuator is distinctly
non-linear there.

## Keyboard

`M5Cardputer` 1.1.1 hands you a resolved snapshot of held keys and nothing else.
This keyboard has **no arrow keys and no ESC key** in hardware; they are printed
on the keycaps as an Fn layer that the library does not resolve for you. Three
further quirks bite anyone reading `KeysState` directly:

* `Fn+Shift+;` returns `:` rather than `;`, so the Fn layer must un-shift before
  matching.
* **Ctrl forces the shifted glyph** into `word` — `Ctrl+C` arrives as `'C'`, and
  a naive `ch == 'c'` test never fires.
* There is no auto-repeat, and Aa is momentary rather than a caps-lock latch.

[`src/core/Keys.cpp`](../src/core/Keys.cpp) resolves all of that into a queue of
single-meaning events:

| Key       | Combination |
| --------- | ----------- |
| Up        | `Fn` + `;`  |
| Down      | `Fn` + `.`  |
| Left      | `Fn` + `,`  |
| Right     | `Fn` + `/`  |
| Esc       | `` Fn + ` `` |
| F1–F10    | `Fn` + `1`–`0` |
| Caps lock | `Fn` + `Aa` (toggles; the hardware key is momentary) |
| Forward delete | `Fn` + `Backspace` |
| Back      | `Backspace` |

Auto-repeat is synthesised at 380 ms delay / 55 ms rate, for navigation keys and
printable characters only.

Newer, unreleased M5Cardputer revisions resolve the Fn layer inside the library
via a third keymap column (`value_third`, plus `esc` and `f1`..`f10` members).
This firmware deliberately does not depend on that, because Library Manager
installs 1.1.1.

## Display

240×135 ST7789V2, no PSRAM behind it.

Every screen renders into one full-size `M5Canvas` sprite and is blitted in a
single push. At 240 × 135 × 2 bytes that is 64.8 kB of the roughly 290 kB of
heap the sketch leaves free — comfortable, but only because nothing else
allocates a frame buffer. If the sprite cannot be allocated, `UI::begin()` falls
back to drawing on the panel directly, so a low-memory board still boots and is
merely uglier.

The v1 firmware drew straight to the panel and called `fillScreen()` every
frame, which is what made its live screens flicker.

The backlight is on **G38**, which also gates RGB power. `Settings` drives it
through `M5Cardputer.Display.setBrightness()` and never below 10, so the screen
cannot be turned off to the point of looking bricked.

## Sources

* M5Stack Cardputer ADV product and schematic documentation, docs.m5stack.com
* M5Stack StampS3A module documentation
* `M5Unified` / `M5GFX` board definitions for `board_M5CardputerADV`
* `M5Cardputer` 1.1.1 keyboard implementation
* Espressif ESP32-S3 technical reference manual (ADC, LEDC, PCNT, RMT)

## Radio and timing

The logic analyser and the 1-Wire/DHT tool both run their timing loops inside
`noInterrupts()`, which on this chip disables interrupts on the calling core
only. The Arduino sketch runs on core 1 and the WiFi task on core 0, so the
radio does not land in the middle of a capture. It is not perfectly free —
there is still bus contention and the WiFi task can preempt between captures —
but it is much smaller than it would be on a single-core part. Anything driven
by RMT (IR, NeoPixel) or LEDC (PWM, servo) is clocked in hardware and is
unaffected either way.
