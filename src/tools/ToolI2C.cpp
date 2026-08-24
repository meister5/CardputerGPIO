#include "ToolI2C.h"
#include <string.h>

namespace cg {

ToolI2C toolI2C;

static const char* HELP[] = {
    "B      switch Grove <-> System bus",
    "S      rescan       F   bus speed",
    "ENTER  open the selected device",
    "",
    "In the register view:",
    "  ^ v   scroll      R   re-read",
    "  W     write a byte to the cursor reg",
    "  DEL   back to the device list",
    "",
    "On the System bus 0x34 is the keyboard,",
    "0x18 the audio codec and 0x68 the IMU.",
    "Writing to those will upset the board.",
    "",
    "Devices needing a pull-up: Grove has none",
    "fitted, so add 4.7k to 3V3 if a device",
    "does not answer.",
};
const char* const* ToolI2C::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

static m5::I2C_Class& bus(bool grove) {
    return grove ? M5Cardputer.Ex_I2C : M5Cardputer.In_I2C;
}

void ToolI2C::onEnter() {
    _view = View::Scan;
    if (_grove) M5Cardputer.Ex_I2C.begin();
    doScan();
}

void ToolI2C::onExit() {
    // Only the external bus is ours to release; the internal one belongs to
    // the keyboard and the IMU.
    M5Cardputer.Ex_I2C.release();
}

void ToolI2C::doScan() {
    bool present[120] = {};
    bus(_grove).scanID(present, _freq);

    _n = 0;
    for (int a = 0; a < 120 && _n < 24; a++)
        if (present[a]) _found[_n++] = (uint8_t)(a + 8);

    if (_sel >= _n) _sel = _n > 0 ? _n - 1 : 0;
}

void ToolI2C::readRegs() {
    if (_n == 0) return;
    uint8_t addr = _found[_sel];
    for (int r = 0; r < 256; r++) {
        uint8_t v = 0;
        _regOk[r] = bus(_grove).readRegister(addr, (uint8_t)r, &v, 1, _freq);
        _regs[r]  = _regOk[r] ? v : 0;
    }
}

void ToolI2C::doWrite() {
    if (_entryLen == 0) return;
    uint8_t val = (uint8_t)strtol(_entry, nullptr, 16);
    uint8_t addr = _found[_sel];
    bool ok = bus(_grove).writeRegister8(addr, (uint8_t)_regSel, val, _freq);
    ui.notify(ok ? "0x%02X <- 0x%02X" : "write failed", _regSel, val);
    if (ok) { _regs[_regSel] = val; _regOk[_regSel] = true; }
    _entryLen = 0;
    _entry[0] = 0;
    _view = View::Regs;
}

bool ToolI2C::onKey(const KeyEvent& ev) {
    // ── Hex entry ─────────────────────────────────────────────────────────
    if (_view == View::Write) {
        if (ev.key == Key::Enter) { doWrite(); return true; }
        if (ev.key == Key::Back) {
            if (_entryLen > 0) { _entry[--_entryLen] = 0; }
            else               { _view = View::Regs; }
            return true;
        }
        if (ev.key == Key::Esc) { _entryLen = 0; _entry[0] = 0; _view = View::Regs; return true; }
        if (ev.key == Key::Char && _entryLen < 2) {
            char c = ev.ch;
            if (c >= 'A' && c <= 'F') c = (char)(c - 'A' + 'a');
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
                _entry[_entryLen++] = c;
                _entry[_entryLen]   = 0;
            }
            return true;
        }
        return true;
    }

    // ── Register view ─────────────────────────────────────────────────────
    if (_view == View::Regs) {
        switch (ev.key) {
            case Key::Up:
                if (_regSel >= 8) _regSel -= 8;
                if (_regSel < _regTop) _regTop = _regSel & ~7;
                return true;
            case Key::Down:
                if (_regSel <= 247) _regSel += 8;
                if (_regSel >= _regTop + 48) _regTop = (_regSel & ~7) - 40;
                return true;
            case Key::Left:  if (_regSel > 0)   _regSel--; return true;
            case Key::Right: if (_regSel < 255) _regSel++; return true;
            case Key::Back:
            case Key::Esc:
                _view = View::Scan;
                return true;
            case Key::Char:
                if (ev.ci('r')) { readRegs(); ui.notify("re-read"); return true; }
                if (ev.ci('w')) { _view = View::Write; _entryLen = 0; _entry[0] = 0; return true; }
                return true;
            default:
                return true;
        }
    }

    // ── Scan list ─────────────────────────────────────────────────────────
    switch (ev.key) {
        case Key::Up:   if (_sel > 0)      _sel--; return true;
        case Key::Down: if (_sel < _n - 1) _sel++; return true;
        case Key::Enter:
            if (_n > 0) {
                _view   = View::Regs;
                _regSel = 0;
                _regTop = 0;
                readRegs();
            }
            return true;
        case Key::Char:
            if (ev.ci('s')) { doScan(); ui.notify("%d device(s)", _n); return true; }
            if (ev.ci('b')) {
                if (_grove) M5Cardputer.Ex_I2C.release();
                _grove = !_grove;
                if (_grove) M5Cardputer.Ex_I2C.begin();
                doScan();
                ui.notify("%s bus", busName());
                return true;
            }
            if (ev.ci('f')) {
                _freq = (_freq == 100000) ? 400000 : (_freq == 400000 ? 50000 : 100000);
                doScan();
                ui.notify("%lu kHz", (unsigned long)(_freq / 1000));
                return true;
            }
            return false;
        default:
            return false;
    }
}

void ToolI2C::draw() {
    switch (_view) {
        case View::Regs:  drawRegs();  break;
        case View::Write: drawWrite(); break;
        default:          drawScan();  break;
    }
}

void ToolI2C::drawScan() {
    char right[20];
    snprintf(right, sizeof(right), "%s %lukHz", busName(), (unsigned long)(_freq / 1000));
    ui.header("I2C Explorer", right, C_HDR);

    int sda = _grove ? PIN_GROVE_SDA : PIN_SYS_SDA;
    int scl = _grove ? PIN_GROVE_SCL : PIN_SYS_SCL;
    ui.textf(6, BODY_Y + 2, C_DIM, "SDA G%d  SCL G%d", sda, scl);

    if (_n == 0) {
        ui.text(6, BODY_Y + 22, C_WARN, "No devices responded.");
        ui.text(6, BODY_Y + 36, C_FAINT, "Check power, GND and pull-ups.");
        ui.footer("[S] rescan  [B] bus  [F] speed");
        return;
    }

    const int ROW_H = 13;
    int visible = 6;
    int top = _sel - visible / 2;
    if (top < 0) top = 0;
    if (top > _n - visible) top = _n - visible;
    if (top < 0) top = 0;

    int y = BODY_Y + 14;
    for (int i = top; i < _n && i < top + visible; i++) {
        bool sel = (i == _sel);
        ui.listRow(y, ROW_H, sel, C_ROLE_BUS);
        ui.textf(8, y + 3, sel ? C_WHITE : C_TEXT, "0x%02X", _found[i]);

        const char* nm = i2cKnownName(_found[i]);
        if (nm) ui.textf(48, y + 3, sel ? C_TEXT : C_DIM, "%-26.26s", nm);
        else    ui.text(48, y + 3, C_FAINT, "unknown device");

        if (!_grove && (_found[i] == 0x34 || _found[i] == 0x18 || _found[i] == 0x68))
            ui.textRight(SCR_W - 6, y + 3, C_WARN, "onboard");
        y += ROW_H;
    }

    ui.scrollbar(SCR_W - 4, BODY_Y + 14, visible * ROW_H, top, visible, _n);
    ui.footerf("%d found  [ENT] open  [S] scan  [B] bus", _n);
}

void ToolI2C::drawRegs() {
    char hdr[28];
    snprintf(hdr, sizeof(hdr), "0x%02X registers", _found[_sel]);
    ui.header(hdr, busName(), C_HDR);

    const int ROWS = 6;
    for (int r = 0; r < ROWS; r++) {
        int base = _regTop + r * 8;
        if (base > 255) break;
        int y = BODY_Y + 2 + r * 13;

        ui.textf(4, y, C_DIM, "%02X", base);

        for (int c = 0; c < 8; c++) {
            int reg = base + c;
            if (reg > 255) break;
            int x = 26 + c * 26;
            bool sel = (reg == _regSel);
            if (sel) ui.g().fillRect(x - 2, y - 2, 24, 11, C_SEL);
            uint16_t col = !_regOk[reg] ? C_FAINT : (sel ? C_WHITE : C_TEXT);
            if (_regOk[reg]) ui.textf(x, y, col, "%02X", _regs[reg]);
            else             ui.text(x, y, col, "--");
        }
    }

    uint8_t v = _regs[_regSel];
    ui.textf(4, BODY_B - 9, C_INFO, "reg %02X = %02X  %3u  ", _regSel, v, v);
    for (int b = 7; b >= 0; b--)
        ui.textf(130 + (7 - b) * 7, BODY_B - 9,
                 ((v >> b) & 1) ? C_HIGH : C_FAINT, "%d", (v >> b) & 1);

    ui.footer("[^v<>] move  [W] write  [R] re-read  [DEL] list");
}

void ToolI2C::drawWrite() {
    drawRegs();
    int y = ui.modal("Write byte", 150, 54, C_WARN);
    ui.textf((SCR_W - 150) / 2 + 8, y, C_TEXT, "0x%02X <- 0x%-2s_", _regSel, _entry);
    ui.textf((SCR_W - 150) / 2 + 8, y + 14, C_DIM, "hex digits, ENT to send");
}

}  // namespace cg
