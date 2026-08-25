#include "ToolSPI.h"
#include <SPI.h>
#include <string.h>

namespace cg {

ToolSPI toolSPI;

static const Role ROLES[] = {
    { "SCK",  RoleDir::Out, "-> device SCK / CLK", 40 },
    { "MOSI", RoleDir::Out, "-> device MOSI / DI", 14 },
    { "MISO", RoleDir::In,  "<- device MISO / DO", 39 },
    { "CS",   RoleDir::Out, "-> device CS (low)",   5 },
};
const Role* ToolSPI::roles() const { return ROLES; }

static const char* HELP[] = {
    "Type hex digits to build a byte, SPACE",
    "to commit it to the queue, ENTER to",
    "clock the whole queue out.",
    "",
    "  DEL   remove the last byte",
    "  M     SPI mode 0-3",
    "  F     clock speed",
    "  Fn+`  clear the queue",
    "",
    "CS is asserted low for the whole",
    "transfer and released afterwards.",
    "",
    "Example: a 25-series flash answers 9F",
    "with three ID bytes -- send 9F 00 00 00.",
};
const char* const* ToolSPI::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

static const uint32_t SPEEDS[] = { 100000, 500000, 1000000, 4000000, 8000000 };
uint32_t ToolSPI::speed(uint8_t i) { return SPEEDS[i % 5]; }

static SPIClass Bus(HSPI);

void ToolSPI::onEnter() {
    _len = 0;
    _done = false;
    _entryLen = 0;
    _entry[0] = 0;

    int sck = pin(0), mosi = pin(1), miso = pin(2), cs = pin(3);
    if (sck < 0 || mosi < 0 || miso < 0 || cs < 0) {
        ui.notify("assign all four pins");
        _ok = false;
        return;
    }
    pins.claimBus(sck);
    pins.claimBus(mosi);
    pins.claimBus(miso);
    pins.setOutput(cs, true);          // idle high
    Bus.begin(sck, miso, mosi, -1);
    _ok = true;
}

void ToolSPI::onExit() {
    if (_ok) Bus.end();
    for (int i = 0; i < 4; i++)
        if (pin(i) >= 0) pins.release(pin(i));
    _ok = false;
}

void ToolSPI::transact() {
    if (!_ok || _len == 0) { ui.notify("nothing to send"); return; }
    int cs = pin(3);

    Bus.beginTransaction(SPISettings(speed(_speedIdx), MSBFIRST, _mode));
    pins.write(cs, false);
    for (int i = 0; i < _len; i++) _rx[i] = Bus.transfer(_tx[i]);
    pins.write(cs, true);
    Bus.endTransaction();

    _done = true;
    ui.beep(2400, 15);
}

bool ToolSPI::onKey(const KeyEvent& ev) {
    switch (ev.key) {
        case Key::Enter: transact(); return true;
        case Key::Esc:   _len = 0; _done = false; _entryLen = 0; _entry[0] = 0; return true;
        case Key::Back:
            if (_entryLen > 0)   { _entry[--_entryLen] = 0; return true; }
            if (_len > 0)        { _len--; _done = false;   return true; }
            return false;
        case Key::Char: {
            char c = ev.ch;
            if (c == ' ') {
                if (_entryLen > 0 && _len < MAXB) {
                    _tx[_len++] = (uint8_t)strtol(_entry, nullptr, 16);
                    _entryLen = 0;
                    _entry[0] = 0;
                    _done = false;
                }
                return true;
            }
            if (ev.ci('m')) { _mode = (uint8_t)((_mode + 1) % 4); ui.notify("mode %u", _mode); return true; }
            if (ev.ci('f')) {
                _speedIdx = (uint8_t)((_speedIdx + 1) % 5);
                ui.notify("%lu kHz", (unsigned long)(speed(_speedIdx) / 1000));
                return true;
            }
            if (c >= 'A' && c <= 'F') c = (char)(c - 'A' + 'a');
            if (((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) && _entryLen < 2) {
                _entry[_entryLen++] = c;
                _entry[_entryLen]   = 0;
                // Two digits complete a byte: commit it so typing flows.
                if (_entryLen == 2 && _len < MAXB) {
                    _tx[_len++] = (uint8_t)strtol(_entry, nullptr, 16);
                    _entryLen = 0;
                    _entry[0] = 0;
                    _done = false;
                }
            }
            return true;
        }
        default:
            return false;
    }
}

void ToolSPI::draw() {
    char right[20];
    snprintf(right, sizeof(right), "m%u %lukHz", _mode,
             (unsigned long)(speed(_speedIdx) / 1000));
    ui.header("SPI Probe", right, C_HDR);

    if (!_ok) {
        ui.text(8, BODY_Y + 20, C_LOW, "Pins not assigned.");
        ui.text(8, BODY_Y + 34, C_DIM, "Press DEL, then C on the wiring");
        ui.text(8, BODY_Y + 44, C_DIM, "screen to pick SCK/MOSI/MISO/CS.");
        ui.footer("");
        return;
    }

    ui.text(6, BODY_Y + 2, C_DIM, "MOSI");
    int x = 44;
    for (int i = 0; i < _len; i++) {
        ui.textf(x, BODY_Y + 2, C_TEXT, "%02X", _tx[i]);
        x += 16;
    }
    if (_entryLen) ui.textf(x, BODY_Y + 2, C_WARN, "%-2s_", _entry);
    else if (_len == 0) ui.text(44, BODY_Y + 2, C_FAINT, "type hex bytes");

    ui.text(6, BODY_Y + 18, C_DIM, "MISO");
    if (_done) {
        x = 44;
        for (int i = 0; i < _len; i++) {
            bool interesting = (_rx[i] != 0x00 && _rx[i] != 0xFF);
            ui.textf(x, BODY_Y + 18, interesting ? C_HIGH : C_FAINT, "%02X", _rx[i]);
            x += 16;
        }
        // ASCII gloss, since half of what comes back is text.
        x = 44;
        for (int i = 0; i < _len; i++) {
            char c = (_rx[i] >= 32 && _rx[i] < 127) ? (char)_rx[i] : '.';
            ui.textf(x + 4, BODY_Y + 30, C_DIM, "%c", c);
            x += 16;
        }
        bool allSame = true;
        for (int i = 1; i < _len; i++) if (_rx[i] != _rx[0]) allSame = false;
        if (allSame && _len > 1)
            ui.textf(6, BODY_Y + 46, C_WARN, "all 0x%02X - device may be absent", _rx[0]);
    } else {
        ui.text(44, BODY_Y + 18, C_FAINT, "press ENTER to clock it out");
    }

    // Pin summary
    int py = BODY_Y + 60;
    static const char* NM[] = { "SCK", "MOSI", "MISO", "CS" };
    for (int i = 0; i < 4; i++) {
        ui.textf(6 + i * 58, py, C_DIM, "%s", NM[i]);
        ui.textf(6 + i * 58, py + 9, C_TEXT, "G%d", pin(i));
    }

    ui.footerf("%dB  [ENT]send [M]mode [F]speed", _len);
}

}  // namespace cg
