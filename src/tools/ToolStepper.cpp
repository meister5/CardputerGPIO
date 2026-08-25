#include "ToolStepper.h"

namespace cg {

ToolStepper toolStepper;

static const Role ROLES[] = {
    { "STEP/IN1", RoleDir::Out, "STEP pin, or coil IN1", -1 },
    { "DIR/IN2",  RoleDir::Out, "DIR pin, or coil IN2",  -1 },
    { "EN/IN3",   RoleDir::Out, "EN (active low), IN3",  -1 },
    { "--/IN4",   RoleDir::Out, "4-wire only: coil IN4", -1 },
};
const Role* ToolStepper::roles() const { return ROLES; }

static const char* HELP[] = {
    "W      switch STEP/DIR <-> 4-wire",
    "SPACE  run / stop continuously",
    "< >    jog one step",
    "^ v    jog 10 steps",
    "D      reverse direction",
    "+ -    speed",
    "E      driver enable (active low EN)",
    "Z      zero the position counter",
    "H      seek back to position 0",
    "",
    "4-wire uses a half-step sequence: eight",
    "phases, so a 28BYJ-48 needs 4096 steps",
    "per output revolution.",
};
const char* const* ToolStepper::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

// Half-step sequence for a unipolar 4-wire motor.
static const uint8_t HALF[8] = {
    0b0001, 0b0011, 0b0010, 0b0110,
    0b0100, 0b1100, 0b1000, 0b1001,
};

void ToolStepper::configure() {
    releaseAllPins();
    int n = (_wiring == Wiring::FourWire) ? 4 : 3;
    for (int i = 0; i < n; i++)
        if (pin(i) >= 0) pins.setOutput(pin(i), false);
    setEnable(_enabled);
}

void ToolStepper::releaseAllPins() {
    for (int i = 0; i < 4; i++)
        if (pin(i) >= 0) { pins.write(pin(i), false); pins.release(pin(i)); }
}

void ToolStepper::setEnable(bool on) {
    _enabled = on;
    if (_wiring == Wiring::StepDir && pin(2) >= 0)
        pins.write(pin(2), !on);          // EN is active low
    if (_wiring == Wiring::FourWire && !on)
        for (int i = 0; i < 4; i++)
            if (pin(i) >= 0) pins.write(pin(i), false);
}

uint32_t ToolStepper::stepIntervalUs() const {
    uint32_t r = _rate ? _rate : 1;
    return 1000000UL / r;
}

void ToolStepper::doStep(bool forward) {
    if (!_enabled) return;

    if (_wiring == Wiring::StepDir) {
        if (pin(1) >= 0) pins.write(pin(1), forward);
        if (pin(0) >= 0) {
            pins.write(pin(0), true);
            delayMicroseconds(4);          // A4988 needs >=1 us; 4 is safe
            pins.write(pin(0), false);
        }
    } else {
        _phase = (uint8_t)((_phase + (forward ? 1 : 7)) & 7);
        uint8_t m = HALF[_phase];
        for (int i = 0; i < 4; i++)
            if (pin(i) >= 0) pins.write(pin(i), (m >> i) & 1);
    }
    _pos += forward ? 1 : -1;
}

void ToolStepper::onEnter() {
    _run = _seeking = false;
    _pos = _target = 0;
    _phase = 0;
    _enabled = true;
    configure();
}

void ToolStepper::onExit() { releaseAllPins(); }

void ToolStepper::tick() {
    if (!_run && !_seeking) return;

    uint32_t now = micros();
    if ((uint32_t)(now - _lastStep) < stepIntervalUs()) return;
    _lastStep = now;

    if (_seeking) {
        if (_pos == _target) { _seeking = false; ui.notify("at home"); return; }
        doStep(_target > _pos);
    } else {
        doStep(_fwd);
    }
}

bool ToolStepper::onKey(const KeyEvent& ev) {
    auto jog = [&](int n) {
        _run = _seeking = false;
        for (int i = 0; i < (n < 0 ? -n : n); i++) doStep(n > 0);
    };

    switch (ev.key) {
        case Key::Left:  jog(-1);  return true;
        case Key::Right: jog(+1);  return true;
        case Key::Up:    jog(+10); return true;
        case Key::Down:  jog(-10); return true;
        case Key::Char:
            if (ev.ch == ' ') {
                _run = !_run;
                _seeking = false;
                ui.notify(_run ? "running" : "stopped");
                return true;
            }
            if (ev.ci('w')) {
                _wiring = (_wiring == Wiring::StepDir) ? Wiring::FourWire
                                                       : Wiring::StepDir;
                _run = _seeking = false;
                configure();
                ui.notify(_wiring == Wiring::StepDir ? "STEP/DIR" : "4-wire");
                return true;
            }
            if (ev.ci('d')) { _fwd = !_fwd; ui.notify(_fwd ? "forward" : "reverse"); return true; }
            if (ev.ci('e')) { setEnable(!_enabled); ui.notify(_enabled ? "enabled" : "disabled"); return true; }
            if (ev.ci('z')) { _pos = 0; ui.notify("position zeroed"); return true; }
            if (ev.ci('h')) { _target = 0; _seeking = true; _run = false; ui.notify("seeking home"); return true; }
            if (ev.ch == '+' || ev.ch == '=') {
                _rate += (_rate < 100) ? 10 : (_rate < 1000 ? 50 : 250);
                if (_rate > 5000) _rate = 5000;
                return true;
            }
            if (ev.ch == '-' || ev.ch == '_') {
                uint32_t d = (_rate <= 100) ? 10 : (_rate <= 1000 ? 50 : 250);
                _rate = (_rate > d + 10) ? _rate - d : 10;
                return true;
            }
            return false;
        default:
            return false;
    }
}

void ToolStepper::draw() {
    const char* wname = (_wiring == Wiring::StepDir) ? "STEP/DIR" : "4-WIRE";
    ui.header("Stepper Driver", wname, C_HDR);

    ui.text(6, BODY_Y + 3, C_DIM, "position");
    ui.textBigf(6, BODY_Y + 13, C_ROLE_PWM, 3, "%+6ld", _pos);

    ui.textf(6, BODY_Y + 40, C_DIM, "%lu steps/s", (unsigned long)_rate);
    ui.textf(120, BODY_Y + 40, _fwd ? C_HIGH : C_WARN, _fwd ? "forward" : "reverse");

    // Status chips
    int cx = 6, cy = BODY_Y + 52;
    ui.chip(cx, cy, _run ? "RUN" : "STOP", C_BLACK, _run ? C_HIGH : C_FAINT);
    cx += ui.chipW(_run ? "RUN" : "STOP") + 4;
    ui.chip(cx, cy, _enabled ? "EN" : "DIS", C_BLACK, _enabled ? C_INFO : C_LOW);
    cx += ui.chipW(_enabled ? "EN" : "DIS") + 4;
    if (_seeking) ui.chip(cx, cy, "HOMING", C_BLACK, C_WARN);

    // Pin summary
    int py = BODY_Y + 3;
    int nPins = (_wiring == Wiring::FourWire) ? 4 : 3;
    for (int i = 0; i < nPins; i++) {
        char lbl[20];
        if (pin(i) >= 0) pinLabel(pin(i), lbl, sizeof(lbl));
        else             snprintf(lbl, sizeof(lbl), "unset");
        static const char* FOUR[] = { "IN1", "IN2", "IN3", "IN4" };
        static const char* SDIR[] = { "STEP", "DIR", "EN", "" };
        const char* nm = (_wiring == Wiring::FourWire) ? FOUR[i] : SDIR[i];
        ui.textf(150, py, C_DIM, "%-4.4s", nm);
        ui.textf(180, py, pin(i) >= 0 ? C_TEXT : C_WARN, "%-9.9s", lbl);
        py += 10;
    }

    ui.footer("[SPC]run [<>]jog [D]dir [+-]spd");
}

}  // namespace cg
