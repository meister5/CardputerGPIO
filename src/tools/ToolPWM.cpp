#include "ToolPWM.h"

namespace cg {

ToolPWM toolPWM;

static const Role ROLES[] = {
    { "PWM", RoleDir::Pwm, "-> driver / scope probe", -1 },
};
const Role* ToolPWM::roles() const { return ROLES; }

static const char* HELP[] = {
    "SPACE  output on / off",
    "< >    frequency, one step",
    "F      next preset frequency",
    "^ v    duty +/- 1%",
    "+ -    duty +/- 5%",
    "D      duty 50%",
    "",
    "Below 5 Hz the output is toggled in",
    "software, because LEDC's divider cannot",
    "reach that low. 'sw' appears in the",
    "header when that engine is running.",
    "",
    "'act' is what the hardware really made;",
    "the divider seldom lands exactly.",
};
const char* const* ToolPWM::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

static const uint32_t PRESETS[] = {
    1, 10, 50, 100, 1000, 5000, 10000, 25000, 50000,
    100000, 500000, 1000000, 5000000, 10000000
};
static constexpr int PRESET_N = (int)(sizeof(PRESETS) / sizeof(PRESETS[0]));

void ToolPWM::onEnter() {
    _on = false;
    _actual = 0;
    _soft = false;
}

void ToolPWM::onExit() { stop(); }

void ToolPWM::stop() {
    int g = pin(0);
    if (g >= 0) {
        pins.release(g);
        pinMode(g, INPUT);
    }
    _on     = false;
    _actual = 0;
}

void ToolPWM::apply() {
    int g = pin(0);
    if (g < 0) { ui.notify("no pin assigned"); return; }

    _soft = (_want < Pins::PWM_MIN_HZ);

    if (_soft) {
        pins.setOutput(g, false);
        _swLevel = false;
        _swNext  = millis();
        _actual  = _want;
        _on      = true;
        return;
    }

    _actual = pins.setPwm(g, _want, _duty);
    if (_actual == 0) {
        ui.notify("%lu Hz not reachable", (unsigned long)_want);
        _on = false;
    } else {
        _on = true;
    }
}

void ToolPWM::tick() {
    if (!_on || !_soft) return;
    int g = pin(0);
    if (g < 0) return;

    // Period split by duty, both halves at least 1 ms.
    uint32_t periodMs = (_want > 0) ? (1000UL / _want) : 1000UL;
    if (periodMs < 2) periodMs = 2;
    uint32_t highMs = (uint32_t)(periodMs * (_duty / 100.0f));
    if (highMs < 1)              highMs = 1;
    if (highMs > periodMs - 1)   highMs = periodMs - 1;

    uint32_t now = millis();
    if ((int32_t)(now - _swNext) < 0) return;

    _swLevel = !_swLevel;
    pins.write(g, _swLevel);
    _swNext = now + (_swLevel ? highMs : (periodMs - highMs));
}

void ToolPWM::nudgeFreq(int dir, bool coarse) {
    if (coarse) {
        _presetIdx = (uint8_t)((_presetIdx + (dir > 0 ? 1 : PRESET_N - 1)) % PRESET_N);
        _want = PRESETS[_presetIdx];
    } else {
        // Proportional steps so one keypress means the same thing at 10 Hz
        // and at 10 MHz.
        uint32_t step = _want / 10;
        if (step < 1) step = 1;
        _want = (dir > 0) ? (_want + step)
                          : (_want > step ? _want - step : 1);
        if (_want > Pins::PWM_MAX_HZ) _want = Pins::PWM_MAX_HZ;
    }
    if (_on) apply();
}

bool ToolPWM::onKey(const KeyEvent& ev) {
    switch (ev.key) {
        case Key::Left:  nudgeFreq(-1, false); return true;
        case Key::Right: nudgeFreq(+1, false); return true;
        case Key::Up:
            _duty += 1.0f;
            if (_duty > 100) _duty = 100;
            if (_on && !_soft) pins.pwmSetDuty(pin(0), _duty);
            return true;
        case Key::Down:
            _duty -= 1.0f;
            if (_duty < 0) _duty = 0;
            if (_on && !_soft) pins.pwmSetDuty(pin(0), _duty);
            return true;
        case Key::Char:
            if (ev.ch == ' ') {
                if (_on) { stop(); ui.notify("output off"); }
                else     { apply(); }
                return true;
            }
            if (ev.ci('f')) { nudgeFreq(+1, true); return true; }
            if (ev.ci('d')) {
                _duty = 50.0f;
                if (_on && !_soft) pins.pwmSetDuty(pin(0), _duty);
                ui.notify("duty 50%%");
                return true;
            }
            if (ev.ch == '+' || ev.ch == '=') {
                _duty += 5.0f;
                if (_duty > 100) _duty = 100;
                if (_on && !_soft) pins.pwmSetDuty(pin(0), _duty);
                return true;
            }
            if (ev.ch == '-' || ev.ch == '_') {
                _duty -= 5.0f;
                if (_duty < 0) _duty = 0;
                if (_on && !_soft) pins.pwmSetDuty(pin(0), _duty);
                return true;
            }
            return false;
        default:
            return false;
    }
}

static void fmtHz(char* out, int n, uint32_t hz) {
    if      (hz >= 1000000UL) snprintf(out, n, "%.3f MHz", hz / 1000000.0);
    else if (hz >= 1000UL)    snprintf(out, n, "%.3f kHz", hz / 1000.0);
    else                      snprintf(out, n, "%lu Hz", (unsigned long)hz);
}

void ToolPWM::draw() {
    char right[16];
    snprintf(right, sizeof(right), "%s%s", _on ? "ON" : "off", _soft ? " sw" : "");
    ui.header("PWM Generator", right, _on ? C_HDR : C_FTR);

    char fbuf[24];
    fmtHz(fbuf, sizeof(fbuf), _want);

    ui.text(6, BODY_Y + 3, C_DIM, "frequency");
    ui.textBig(6, BODY_Y + 13, _on ? C_ROLE_PWM : C_DIM, 2, fbuf);

    if (_on && !_soft && _actual) {
        char abuf[24];
        fmtHz(abuf, sizeof(abuf), _actual);
        ui.textf(6, BODY_Y + 32, C_FAINT, "act %s  %u-bit", abuf,
                 (unsigned)pins.pwmBits(pin(0)));
    } else if (!_on) {
        ui.text(6, BODY_Y + 32, C_FAINT, "press SPACE to start");
    }

    ui.text(6, BODY_Y + 46, C_DIM, "duty");
    ui.textBigf(6, BODY_Y + 56, _on ? C_HIGH : C_DIM, 2, "%3.0f%%", _duty);
    ui.hbar(80, BODY_Y + 58, 152, 12, _duty, _on ? C_HIGH : C_FAINT);

    // Little waveform preview so the duty number has a shape next to it.
    int wx = 80, wy = BODY_Y + 20, ww = 152, wh = 14;
    ui.g().drawRect(wx, wy, ww, wh, C_LINE);
    int cycles = 3;
    for (int c = 0; c < cycles; c++) {
        int x0 = wx + 1 + (c * (ww - 2)) / cycles;
        int x1 = wx + 1 + ((c + 1) * (ww - 2)) / cycles;
        int xm = x0 + (int)((x1 - x0) * (_duty / 100.0f));
        uint16_t col = _on ? C_ROLE_PWM : C_FAINT;
        ui.g().drawFastHLine(x0, wy + 2,      xm - x0, col);
        ui.g().drawFastVLine(xm, wy + 2,      wh - 5,  col);
        ui.g().drawFastHLine(xm, wy + wh - 3, x1 - xm, col);
        if (c) ui.g().drawFastVLine(x0, wy + 2, wh - 5, col);
    }

    char plbl[20];
    pinLabel(pin(0), plbl, sizeof(plbl));
    ui.textf(6, BODY_B - 9, C_FAINT, "on %s", plbl);
    ui.footer("[SPC] on/off [<>] freq [F] preset [^v] duty");
}

}  // namespace cg
