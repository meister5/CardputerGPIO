#include "ToolIC.h"
#include <stdio.h>

namespace cg {

ToolIC toolIC;

static const Role ROLES[] = {
    { "A0", RoleDir::Out, "address bit 0 (LSB)",      -1 },
    { "A1", RoleDir::Out, "address bit 1",            -1 },
    { "A2", RoleDir::Out, "address bit 2",            -1 },
    { "A3", RoleDir::Out, "address bit 3 (74154)",    -1 },
    { "EN", RoleDir::Out, "enable / inhibit",         -1 },
    { "IO", RoleDir::Adc, "the line being selected",  -1 },
};
const Role* ToolIC::roles() const { return ROLES; }

static const char* HELP[] = {
    "74HC138  A0-A2 -> A,B,C   EN -> E3",
    "         (tie E1,E2 low)  IO -> a Yn",
    "CD4051   A0-A2 -> A,B,C   EN -> INH",
    "         IO -> the common Z pin",
    "74HC151  A0-A2 -> A,B,C   IO -> Y",
    "74HC154  A0-A3 -> A,B,C,D EN -> E1",
    "",
    "  P      part",
    "  R      sense: digital / mV / off",
    "  < >    address -1 / +1",
    "  E      toggle enable",
    "  S      walk every channel",
    "  0-9    jump to that channel",
    "",
    "In scan mode each channel's reading is",
    "kept, so a decoder with a stuck output",
    "shows up as the one cell that never",
    "changes. F2 logs the table as CSV.",
};
const char* const* ToolIC::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

int ToolIC::addrLines(Part p) { return (p == Part::HC154) ? 4 : 3; }
bool ToolIC::enableActiveLow(Part p) { return p != Part::HC151; }

const char* ToolIC::partName(Part p) {
    switch (p) {
        case Part::HC138:  return "74HC138";
        case Part::HC4051: return "CD4051";
        case Part::HC151:  return "74HC151";
        case Part::HC154:  return "74HC154";
        default:           return "?";
    }
}

const char* ToolIC::readName(Read r) {
    switch (r) {
        case Read::Digital: return "logic";
        case Read::Analog:  return "mV";
        default:            return "off";
    }
}

void ToolIC::onEnter() {
    for (int i = 0; i < addrLines(_part); i++)
        if (pin(i) >= 0) pins.setOutput(pin(i), false);
    // A part with fewer address lines must let go of the ones it no longer
    // uses, or A3 sits there driving whatever the 74154 last left on it.
    for (int i = addrLines(_part); i < 4; i++)
        if (pin(i) >= 0) pins.release(pin(i));
    if (pin(4) >= 0) pins.setOutput(pin(4), false);

    if (pin(5) >= 0) {
        if (_read == Read::Analog && pinAdcOk(pin(5))) pins.setAdc(pin(5));
        else if (_read == Read::Digital)               pins.setInput(pin(5), 1);
    }
    for (int i = 0; i < MAXCH; i++) { _val[i] = 0; _seen[i] = false; }
    applyAddress();
}

void ToolIC::onExit() {
    for (int i = 0; i < roleCount(); i++)
        if (pin(i) >= 0) pins.release(pin(i));
    _scan = false;
}

void ToolIC::applyAddress() {
    int lines = addrLines(_part);
    for (int i = 0; i < lines; i++)
        if (pin(i) >= 0) pins.write(pin(i), (_addr >> i) & 1);

    if (pin(4) >= 0) {
        // "Enabled" is what the user asked for; the level depends on the part.
        bool lvl = enableActiveLow(_part) ? !_enabled : _enabled;
        pins.write(pin(4), lvl);
    }
}

void ToolIC::sample() {
    int g = pin(5);
    if (g < 0 || _read == Read::None) return;
    if (_addr < 0 || _addr >= MAXCH) return;

    if (_read == Read::Analog) _val[_addr] = (int)pins.adcMilliVoltsAvg(g, 4);
    else                       _val[_addr] = pins.read(g) ? 1 : 0;
    _seen[_addr] = true;
}

void ToolIC::tick() {
    if (_scan) {
        if (millis() - _lastStep < _dwellMs) return;
        _lastStep = millis();
        sample();                                   // read where we are
        _addr = (_addr + 1) % channels();           // then move on
        applyAddress();
        delayMicroseconds(50);                      // settle before next read
    } else {
        // Manual mode still refreshes, so a probe moved by hand updates.
        if (millis() - _lastStep < 60) return;
        _lastStep = millis();
        sample();
    }
}

bool ToolIC::onKey(const KeyEvent& ev) {
    switch (ev.key) {
        case Key::Left:
            _addr = (_addr + channels() - 1) % channels();
            applyAddress();
            return true;
        case Key::Right:
            _addr = (_addr + 1) % channels();
            applyAddress();
            return true;
        case Key::Up:
            _dwellMs = _dwellMs >= 1000 ? 1000 : _dwellMs + 20;
            return true;
        case Key::Down:
            _dwellMs = _dwellMs <= 20 ? 20 : _dwellMs - 20;
            return true;
        case Key::Char: {
            int d = ev.digit();
            if (d >= 0 && d < channels()) {
                _addr = d;
                _scan = false;
                applyAddress();
                return true;
            }
            if (ev.ci('p')) {
                _part = (Part)(((int)_part + 1) % (int)Part::COUNT);
                _addr = 0;
                for (int i = 0; i < MAXCH; i++) _seen[i] = false;
                onEnter();
                ui.notify("%s", partName(_part));
                return true;
            }
            if (ev.ci('r')) {
                _read = (Read)(((int)_read + 1) % (int)Read::COUNT);
                for (int i = 0; i < MAXCH; i++) _seen[i] = false;
                onEnter();
                ui.notify("sense: %s", readName(_read));
                return true;
            }
            if (ev.ci('e')) {
                _enabled = !_enabled;
                applyAddress();
                return true;
            }
            if (ev.ci('s')) {
                _scan = !_scan;
                ui.notify(_scan ? "scanning" : "manual");
                return true;
            }
            return false;
        }
        default:
            return false;
    }
}

void ToolIC::draw() {
    char right[20];
    snprintf(right, sizeof(right), "%s", partName(_part));
    ui.header("Decoder / Mux", right, catColor(cat()));

    int lines = addrLines(_part);

    // Address, as bits and as a number — the two ways you think about it.
    ui.text(6, BODY_Y + 2, C_DIM, "addr");
    for (int i = lines - 1; i >= 0; i--) {
        int x = 34 + (lines - 1 - i) * 13;
        bool bit = (_addr >> i) & 1;
        ui.g().fillRoundRect(x, BODY_Y, 11, 12, 2, bit ? C_HIGH : C_PANEL2);
        ui.textf(x + 3, BODY_Y + 3, bit ? C_BLACK : C_DIM, "%d", bit ? 1 : 0);
    }
    ui.textBigf(34 + lines * 13 + 8, BODY_Y - 2, C_TITLE, 2, "%2d", _addr);

    ui.chip(150, BODY_Y, _enabled ? "EN" : "DIS",
            _enabled ? C_BLACK : C_DIM, _enabled ? C_HIGH : C_PANEL2);
    ui.chip(180, BODY_Y, _scan ? "SCAN" : "MAN",
            _scan ? C_BLACK : C_DIM, _scan ? C_INFO : C_PANEL2);

    // Channel table. 8 or 16 cells, coloured by what was read there.
    int cols = 8;
    int cw   = 28;
    int rows = channels() / cols;
    for (int c = 0; c < channels(); c++) {
        int x = 6 + (c % cols) * cw;
        int y = BODY_Y + 22 + (c / cols) * 26;
        bool cur = (c == _addr);

        uint16_t bg = C_PANEL;
        if (_seen[c]) {
            if (_read == Read::Digital) bg = _val[c] ? C_SEL : C_PANEL2;
            else if (_read == Read::Analog) {
                uint8_t lvl = (uint8_t)((_val[c] > 3300 ? 3300 : _val[c]) * 255 / 3300);
                bg = ui.g().color565(lvl / 3, lvl / 2, 60);
            }
        }
        ui.g().fillRoundRect(x, y, cw - 3, 22, 3, bg);
        ui.g().drawRoundRect(x, y, cw - 3, 22, 3, cur ? C_CURSOR : C_LINE);

        ui.textf(x + 3, y + 2, cur ? C_TITLE : C_DIM, "%d", c);
        if (!_seen[c] || _read == Read::None)
            ui.text(x + 3, y + 12, C_FAINT, "--");
        else if (_read == Read::Digital)
            ui.text(x + 3, y + 12, _val[c] ? C_HIGH : C_LOW, _val[c] ? "HI" : "LO");
        else
            ui.textf(x + 1, y + 12, C_TEXT, "%d", _val[c]);
    }

    (void)rows;
    ui.textf(6, BODY_B - 9, C_FAINT, "sense %s  dwell %lums",
             readName(_read), (unsigned long)_dwellMs);

    ui.footer("[<>] addr [S] scan [E] en [R] sense [P] part");
}

const char* ToolIC::logHeader() const { return "channel,value,mode"; }

bool ToolIC::logRow(char* out, size_t n) {
    if (_read == Read::None || _addr >= MAXCH || !_seen[_addr]) return false;
    snprintf(out, n, "%d,%d,%s", _addr, _val[_addr], readName(_read));
    return true;
}

}  // namespace cg
