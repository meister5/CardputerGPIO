#include "ToolDashboard.h"

namespace cg {

ToolDashboard toolDashboard;

static const char* HELP[] = {
    "Every exposed pin, updated 10x a second.",
    "",
    "  M      cycle mode on the selected pin",
    "         IN-PU > IN-PD > IN > OUT > ADC",
    "  SPACE  toggle an OUT pin",
    "  A / Z  all outputs high / low",
    "  R      release every pin to input",
    "",
    "ADC is offered only on ADC-capable pins.",
    "G8/G9 are never listed: they are the",
    "system I2C bus (keyboard, IMU, codec).",
};

const char* const* ToolDashboard::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

void ToolDashboard::onEnter() {
    _pool = poolGpio(_n);
    if (_n > 16) _n = 16;
    _cursor = 0;
    _scroll = 0;
    // Start everything as a pulled-up input: safe, and reads meaningfully
    // against an unconnected header.
    for (int i = 0; i < _n; i++) pins.setInput(_pool[i], 1);
}

void ToolDashboard::onExit() {
    pins.releaseAll();
}

void ToolDashboard::tick() {
    uint32_t now = millis();
    if ((uint32_t)(now - _lastPoll) < 100) return;
    _lastPoll = now;

    for (int i = 0; i < _n; i++) {
        int g = _pool[i];
        PMode m = pins.mode(g);
        if (m == PMode::Adc) {
            _mv[i]  = pins.adcMilliVoltsAvg(g, 4);
            _lvl[i] = false;
        } else if (m == PMode::Out) {
            _lvl[i] = pins.level(g);
        } else {
            _lvl[i] = pins.read(g);
        }
    }
}

void ToolDashboard::cycleMode(int idx) {
    int g = _pool[idx];
    switch (pins.mode(g)) {
        case PMode::InPull:  pins.setInput(g, 2);      break;   // -> pull-down
        case PMode::InPulld: pins.setInput(g, 0);      break;   // -> floating
        case PMode::In:      pins.setOutput(g, false); break;   // -> output
        case PMode::Out:
            if (pinAdcOk(g)) pins.setAdc(g);
            else             pins.setInput(g, 1);
            break;
        case PMode::Adc:     pins.setInput(g, 1);      break;
        default:             pins.setInput(g, 1);      break;
    }
}

bool ToolDashboard::onKey(const KeyEvent& ev) {
    switch (ev.key) {
        case Key::Up:
            if (_cursor > 0) _cursor--;
            if (_cursor < _scroll) _scroll = _cursor;
            return true;
        case Key::Down:
            if (_cursor < _n - 1) _cursor++;
            if (_cursor >= _scroll + VISIBLE) _scroll = _cursor - VISIBLE + 1;
            return true;
        case Key::Char:
            if (ev.ci('m')) { cycleMode(_cursor); return true; }
            if (ev.ch == ' ') {
                int g = _pool[_cursor];
                if (pins.mode(g) == PMode::Out) {
                    pins.toggle(g);
                    ui.beep(pins.level(g) ? 2800 : 1900, 15);
                } else {
                    ui.notify("not an output - press M");
                }
                return true;
            }
            if (ev.ci('a') || ev.ci('z')) {
                bool hi = ev.ci('a');
                for (int i = 0; i < _n; i++)
                    if (pins.mode(_pool[i]) == PMode::Out) pins.write(_pool[i], hi);
                ui.notify(hi ? "all outputs HIGH" : "all outputs LOW");
                return true;
            }
            if (ev.ci('r')) {
                for (int i = 0; i < _n; i++) pins.setInput(_pool[i], 1);
                ui.notify("all pins released to input");
                return true;
            }
            return false;
        default:
            return false;
    }
}

void ToolDashboard::draw() {
    ui.header("Pin Dashboard", "live");

    int y   = BODY_Y + 1;
    int end = _scroll + VISIBLE;
    if (end > _n) end = _n;

    for (int i = _scroll; i < end; i++) {
        int  g   = _pool[i];
        bool sel = (i == _cursor);
        PMode m  = pins.mode(g);

        uint16_t modeCol = (m == PMode::Out) ? C_ROLE_OUT
                         : (m == PMode::Adc) ? C_ROLE_ADC
                                             : C_ROLE_IN;
        ui.listRow(y, ROW_H, sel, modeCol);

        char lbl[20];
        pinLabel(g, lbl, sizeof(lbl));
        ui.textf(8, y + 4, sel ? C_WHITE : C_TEXT, "%-11.11s", lbl);

        ui.chip(80, y + 2, pins.modeName(g), C_BLACK, modeCol);

        if (m == PMode::Adc) {
            ui.textf(132, y + 4, C_ROLE_ADC, "%4lu mV", (unsigned long)_mv[i]);
            ui.hbar(196, y + 4, 38, 8, (_mv[i] * 100.0f) / 3300.0f, C_ROLE_ADC);
        } else {
            ui.state(134, y + 3, _lvl[i]);
            const PinInfo* pi = pinInfo(g);
            if (pi && pi->warn) ui.text(170, y + 4, C_WARN, "SD");
            else if (m == PMode::Out) ui.text(170, y + 4, C_DIM, "driving");
        }
        y += ROW_H;
    }

    ui.scrollbar(SCR_W - 4, BODY_Y + 1, VISIBLE * ROW_H, _scroll, VISIBLE, _n);
    ui.footer("[M]mode [SPC]tog [A/Z]all [R]rst");
}

}  // namespace cg
