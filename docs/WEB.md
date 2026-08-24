# The web interface

Turn on **Web Interface** on the device and the Cardputer serves a control
panel over WiFi. It is not a second implementation of the toolbox — it drives
the same state machine the keyboard drives and shows the same framebuffer, so
it cannot drift out of step with the device.

## Getting to it

**Access point** — the Cardputer makes its own network. Nothing else is needed:
no router, no internet. The tool screen shows the network name, the password
and the address to open.

**Join a network** — it appears on a network you already have, at the address
shown and also at `http://cardputer.local`.

First run is usually: start the access point, connect a laptop to it, open the
page, and set your home network from the Settings tab — typing a WPA2 password
on a 56-key thumb keyboard is not something to inflict on anyone. Scanning and
typing it on the device works too (press **S** in the Web Interface tool), for
when there is no other machine to hand.

The access point password is generated from hardware entropy on first use and
kept in NVS. Press **R** in the tool to throw it away and get a new one on the
next start. It is not derived from the MAC address, which anyone in range can
read off the SSID.

## What it costs

Two things, both real:

**ADC2 stops working.** G13 and G15 share their analog hardware with the radio.
While the portal is up they are withdrawn from the ADC pool, the pin picker
stops offering them, and an already-saved assignment shows red on the wiring
screen instead of quietly reading zero. Everything else — digital I/O, PWM,
I²C, UART, SPI, the ADC1 pins — is unaffected.

**Flash.** WiFi, the HTTP server and mDNS add about 630 kB, which does not fit
the board's default 1.2 MB app partition. The build needs
**Tools → Partition Scheme → "8M with spiffs (3MB APP/1.5MB SPIFFS)"**, where it
sits at about 41%. If you would rather have the lean radio-silent build that
fits the default partition, set `CG_ENABLE_WEB` to `0` in
[`src/core/Config.h`](../src/core/Config.h); everything else is identical.

There is also a small battery cost: modem sleep is turned off so a keypress
from the browser is not waiting on the next beacon.

## How it stays a duplicate

Three decisions do the work:

**The framebuffer is mirrored, not re-rendered.** Every screen in the firmware
already draws into one 240×135 sprite. `Mirror` dices it into 16×15 tiles,
hashes each one, and sends only the tiles that changed since the last request —
a frame with a couple of numbers ticking over moves two or three tiles, about
1.5 kB. Every tool, every dialog and every tool added later appears in the
browser with no web code written for it.

**Keys are injected, not interpreted.** The browser posts a key event and
`Keys::inject()` puts it in the same queue the physical keyboard feeds. Your
arrow keys, Esc, Tab, Enter, Backspace and F1–F10 map straight through, so you
do not need the device's Fn layer.

**Live values reuse the CSV hooks.** Any tool that implements `logHeader()` and
`logRow()` for logging gets a live readout in the browser for free, with columns
that match the CSV exactly. No per-tool web code, and no third place for the
numbers to disagree.

What the browser *does* implement natively is the part a browser is genuinely
better at: assigning pins for every tool from one table, editing settings,
naming and loading setups, reading the pinout, and downloading captures.

## HTTP API

Everything is plain HTTP with query-string parameters. Nothing parses JSON on
the device. Responses are JSON except where noted.

| Method | Path | Parameters | Returns |
| ------ | ---- | ---------- | ------- |
| GET  | `/` | — | the interface (one gzipped HTML file) |
| GET  | `/api/state` | — | board, shell state, open tool, heap, battery, WiFi, logging |
| GET  | `/api/tools` | — | every tool with its roles and current pins |
| POST | `/api/tool/open` | `id` | open a tool, exactly as picking it in the menu |
| POST | `/api/tool/start` | `confirm` | start it; `needsArm` comes back when it drives pins |
| POST | `/api/tool/back` | — | close the tool and return to the menu |
| GET  | `/api/pins` | — | live pin table: mode, level, millivolts, flags |
| POST | `/api/pin` | `tool`, `role`, `gpio` | assign a pin (`-1` clears it) |
| GET  | `/api/live` | — | the open tool's live row, columns matching its CSV |
| POST | `/api/key` | `k`, `c`, `ctrl`, `shift`, `alt` | inject a keypress |
| GET  | `/api/screen` | `full` | changed framebuffer tiles (binary, see below) |
| GET  | `/api/settings` | — | brightness, beep, arm, SD pins, log interval |
| POST | `/api/settings` | any of the above | save them |
| GET  | `/api/setups` | — | the six setup slots |
| POST | `/api/setup` | `action=save\|load\|delete`, `slot`, `name` | act on one |
| GET  | `/api/board` | — | board summary and the internal peripheral table |
| GET  | `/api/logs` | — | CSV captures on the card |
| GET  | `/api/log` | `f` | download one (`text/csv`) |
| POST | `/api/log/ctl` | `action=start\|stop` | start or stop a capture |
| POST | `/api/wifi` | `mode`, `ssid`, `pass`, `auto` | configure the radio |

`k` is one of `up`, `down`, `left`, `right`, `enter`, `back`, `esc`, `tab`,
`fdel`, `f1`–`f10`, or `char` with `c` set to a printable ASCII code.

### Screen format

`GET /api/screen` returns a binary blob, little-endian throughout:

```
offset  size  meaning
0       3     magic 'C' 'G' 'S'
3       1     version, currently 1
4       1     tile columns (15)
5       1     tile rows (9)
6       1     tile width in pixels (16)
7       1     tile height in pixels (15)
8       2     number of tiles that follow
10      ...   per tile: u16 index, then 16*15 u16 pixels, RGB565
```

Tile index is `row * columns + column`. Pass `full=1` to get all 135 tiles —
the client does this on its first request and after any error, and takes
deltas the rest of the time.

The pixels are plain RGB565 whatever byte order LovyanGFX is keeping
internally: `Mirror::tile()` reads through the sprite's own colour converter
with `setSwapBytes(true)` rather than assuming.

## A note on access control

There is none. Anyone who can reach the page can drive every pin on the
header, exactly as if they had picked up the device. On the access point that
means anyone with the password; on your own network it means anyone on that
network.

That is a deliberate match to what the thing is — a bench tool you point at
your own circuit — but it is worth knowing before you leave it running with
autostart on a network you share. If that matters for your setup, leave the
radio off and use the device's own keyboard; the firmware is identical either
way, and the lean build removes the radio entirely.
