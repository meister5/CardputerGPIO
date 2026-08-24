#include "ToolMeter.h"

namespace cg {

ToolMeter toolMeter;

static const Role ROLES[] = {
    { "PROBE", RoleDir::Adc, "-> point under test", -1 },
};
const Role* ToolMeter::roles() const { return ROLES; }

static const char* HELP[] = {
    "M cycles mode. GND must be common.",
    "",
    "VOLTS   0-3.3 V, averaged 16x.",
    "CONT    pull-up + beeper. Beeps when the",
    "        probe is pulled below ~0.4 V,",
    "        i.e. shorted to ground.",
    "OHMS    wire 3V3 -- Rref -- PROBE -- Rx",
    "        -- GND. Press R to pick Rref.",
    "        Best accuracy when Rx ~ Rref.",
    "LOGIC   HIGH / LOW / floating, with the",
    "        actual voltage next to it.",
    "",
    "Never put more than 3.3 V on a pin.",
};
const char* const* ToolMeter::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

static const uint32_t REFS[] = { 1000, 10000, 100000 };
uint32_t ToolMeter::refOhms(uint8_t i) { return REFS[i % 3]; }

const char* ToolMeter::modeName(Mode m) {
    switch (m) {
        case Mode::Volts:      return "VOLTS";
        case Mode::Continuity: return "CONT";
        case Mode::Ohms:       return "OHMS";
        case Mode::Logic:      return "LOGIC";
        default:               return "?";
    }
}

void ToolMeter::applyMode() {
    int g = pin(0);
    if (g < 0) return;
    if (_mode == Mode::Continuity) pins.setInput(g, 1);   // pull-up
    else if (_mode == Mode::Logic) pins.setInput(g, 0);   // floating
    else                           pins.setAdc(g);
    _mn = 0xFFFFFFFF;
    _mx = 0;
}

void ToolMeter::onEnter() { applyMode(); }

void ToolMeter::onExit() {
    if (pin(0) >= 0) pins.release(pin(0));
}

void ToolMeter::tick() {
    int g = pin(0);
    if (g < 0) return;

    uint32_t now = millis();
    if ((uint32_t)(now - _lastPoll) < 90) return;
    _lastPoll = now;

    if (_mode == Mode::Continuity) {
        _cont = !pins.read(g);            // pulled low == shorted to GND
        if (_cont && !_lastCont) ui.beep(3000, 60);
        _lastCont = _cont;
        return;
    }

    if (_mode == Mode::Logic) {
        // Read the level while floating, then take a voltage to tell a real
        // drive apart from a high-impedance node sitting at mid-rail.
        _cont = pins.read(g);
        pins.setAdc(g);
        _mv = pins.adcMilliVoltsAvg(g, 8);
        pins.setInput(g, 0);
        return;
    }

    _mv = pins.adcMilliVoltsAvg(g, 16);
    if (_mv < _mn) _mn = _mv;
    if (_mv > _mx) _mx = _mv;

    if (_mode == Mode::Ohms) {
        // 3V3 -- Rref -- node -- Rx -- GND  =>  Rx = Rref * V / (3.3 - V)
        float v = _mv / 1000.0f;
        float head = 3.3f - v;
        _ohms = (head <= 0.02f) ? -1.0f                       // open circuit
              : (v   <= 0.002f) ? 0.0f                        // dead short
                                : (refOhms(_refIdx) * v) / head;
    }
}

bool ToolMeter::onKey(const KeyEvent& ev) {
    if (ev.ci('m')) {
        _mode = (Mode)(((int)_mode + 1) % (int)Mode::COUNT);
        applyMode();
        ui.notify("%s", modeName(_mode));
        return true;
    }
    if (ev.ci('r')) {
        if (_mode == Mode::Ohms) {
            _refIdx = (uint8_t)((_refIdx + 1) % 3);
            ui.notify("Rref %lu ohm", (unsigned long)refOhms(_refIdx));
        } else {
            _mn = 0xFFFFFFFF; _mx = 0;
            ui.notify("min/max cleared");
        }
        return true;
    }
    return false;
}

void ToolMeter::draw() {
    char right[16];
    pinLabel(pin(0), right, sizeof(right));
    ui.header("Multimeter", right, C_HDR);

    int cy = BODY_Y + 6;

    switch (_mode) {
        case Mode::Volts: {
            ui.textBigf(18, cy + 6, C_ROLE_ADC, 4, "%5.3f", _mv / 1000.0f);
            ui.textBig(160, cy + 18, C_DIM, 2, "V");
            ui.hbar(14, cy + 48, 212, 10, (_mv * 100.0f) / 3300.0f, C_ROLE_ADC);
            if (_mn != 0xFFFFFFFF)
                ui.textf(14, cy + 62, C_FAINT, "min %.3f    max %.3f",
                         _mn / 1000.0f, _mx / 1000.0f);
            break;
        }
        case Mode::Continuity: {
            uint16_t col = _cont ? C_HIGH : C_DIM;
            ui.g().fillRoundRect(40, cy + 6, 160, 44, 6, _cont ? C_HIGH : C_PANEL);
            ui.textBig(_cont ? 62 : 74, cy + 20, _cont ? C_BLACK : C_DIM, 2,
                       _cont ? "CONTINUITY" : "OPEN");
            ui.textCenter(SCR_W / 2, cy + 60, col,
                          _cont ? "probe is shorted to GND"
                                : "no path to GND");
            break;
        }
        case Mode::Ohms: {
            if (_ohms < 0) {
                ui.textBig(56, cy + 16, C_WARN, 3, "OPEN");
            } else if (_ohms < 1000.0f) {
                ui.textBigf(24, cy + 12, C_ROLE_ADC, 4, "%4.0f", _ohms);
                ui.textBig(150, cy + 24, C_DIM, 2, "ohm");
            } else if (_ohms < 1000000.0f) {
                ui.textBigf(24, cy + 12, C_ROLE_ADC, 4, "%4.1f", _ohms / 1000.0f);
                ui.textBig(150, cy + 24, C_DIM, 2, "k");
            } else {
                ui.textBigf(24, cy + 12, C_ROLE_ADC, 4, "%4.2f", _ohms / 1000000.0f);
                ui.textBig(150, cy + 24, C_DIM, 2, "M");
            }
            ui.textf(14, cy + 62, C_FAINT, "Rref %lu ohm   node %.3f V",
                     (unsigned long)refOhms(_refIdx), _mv / 1000.0f);
            break;
        }
        case Mode::Logic: {
            bool floating = (_mv > 900 && _mv < 2400);
            uint16_t col = floating ? C_WARN : (_cont ? C_HIGH : C_LOW);
            const char* txt = floating ? "FLOAT" : (_cont ? "HIGH" : "LOW");
            ui.g().fillRoundRect(40, cy + 6, 160, 44, 6, col);
            ui.textBig(floating ? 70 : (_cont ? 78 : 84), cy + 18, C_BLACK, 3, txt);
            ui.textCenter(SCR_W / 2, cy + 60, C_DIM, "");
            ui.textf(14, cy + 60, C_FAINT, "%.3f V on a floating input",
                     _mv / 1000.0f);
            break;
        }
        default: break;
    }

    ui.footerf("[M] %s  [R] %s", modeName(_mode),
               _mode == Mode::Ohms ? "Rref" : "reset");
}


const char* ToolMeter::logHeader() const { return "mode,mV,ohms,continuity"; }

bool ToolMeter::logRow(char* out, size_t n) {
    snprintf(out, n, "%s,%lu,%.1f,%d", modeName(_mode),
             (unsigned long)_mv, _ohms, _cont ? 1 : 0);
    return true;
}

}  // namespace cg
