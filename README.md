# CardputerGPIO

[![Release](https://img.shields.io/github/v/release/meister5/CardputerGPIO?style=flat-square)](https://github.com/meister5/CardputerGPIO/releases/latest)
[![License](https://img.shields.io/github/license/meister5/CardputerGPIO?style=flat-square)](LICENSE)

A GPIO workbench for the M5Stack Cardputer ADV: twenty-one tools behind one menu for driving, measuring and logging pins.

## Features

- Digital and analog: pin dashboard, digital in/out, calibrated-millivolt ADC, multimeter
- Signals: PWM up to 10 MHz, servo, stepper, frequency counter, 4-channel logic analyzer, NeoPixel
- Buses: I2C explorer, UART terminal with USB-to-TTL bridge, SPI probe, 1-Wire/DHT, IR transmitter
- CSV logging to microSD and USB serial (F2)
- Pin conflict checks, wiring diagrams and saved setups
- Optional web interface over WiFi that mirrors the device screen

## Hardware

Cardputer ADV only. The pin map, keyboard driver and I2C layout differ from the original Cardputer v1.1, and the firmware warns in red if it detects another board. See [docs/HARDWARE.md](docs/HARDWARE.md).

All GPIO is 3.3 V and not 5 V tolerant. G8 and G9 carry the system I2C bus and are never offered as GPIO. The microSD pins (G14, G39, G40) are held back unless you enable *allow SD pins*. Outputs must be armed before a tool drives a pin.

## Install

Prebuilt images are in the repo root:

- `CardputerGPIO-app.bin` for [M5Launcher](https://github.com/bmorcelli/Launcher): copy it to the SD card and choose Install, or use it as an `OTA > Favorites` URL.
- `CardputerGPIO-merged.bin` for M5Burner or `esptool` at offset `0x0`.

## Usage

The keyboard has no arrow or ESC keys, so the firmware maps them:

| Key | Action |
| --- | --- |
| `;` `.` `,` `/` | up, down, left, right |
| `` ` `` | back |
| Fn + `` ` `` | ESC |
| Fn + 1-0 | F1-F10 |
| F1 | help (global key reference, or a page for the running tool) |
| F2 | start/stop CSV logging |

The screen backlight turns off after 30 seconds idle by default; any key wakes it. For the web interface, open **Web Interface** on the device, choose access point or join a network, and open the address shown. It has no password by default, so set one under Settings -> Access on shared networks. See [docs/WEB.md](docs/WEB.md).

Tool list, logging format, keyboard details and code layout: [docs/REFERENCE.md](docs/REFERENCE.md).

## Build

Requires the M5Stack Arduino core 3.2.2 or newer (CI uses 3.3.9) and the M5Cardputer, M5Unified and M5GFX libraries.

```bash
arduino-cli core install m5stack:esp32 \
  --additional-urls https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json
arduino-cli lib install M5Cardputer M5Unified M5GFX
tools/package.sh   # writes dist/CardputerGPIO-app.bin and dist/CardputerGPIO-merged.bin
```

In the Arduino IDE, select the M5Cardputer board and set Partition Scheme to "8M with spiffs (3MB APP/1.5MB SPIFFS)" and PSRAM to Disabled. To drop the web interface and fit the default partition, set `CG_ENABLE_WEB` to `0` in `src/core/Config.h`.

## License

MIT. See [LICENSE](LICENSE).
