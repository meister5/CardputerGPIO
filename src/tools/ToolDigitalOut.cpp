#include "ToolDigitalOut.h"

namespace cg {

ToolDigitalOut toolDigitalOut;

static const Role ROLES[] = {
    { "OUT 1", RoleDir::Out, "-> load / input 1", -1 },
    { "OUT 2", RoleDir::Out, "-> load / input 2", -1 },
    { "OUT 3", RoleDir::Out, "-> load / input 3", -1 },
    { "OUT 4", RoleDir::Out, "-> load / input 4", -1 },
    { "OUT 5", RoleDir::Out, "-> load / input 5", -1 },
    { "OUT 6", RoleDir::Out, "-> load / input 6", -1 },
    { "OUT 7", RoleDir::Out, "-> load / input 7", -1 },
    { "OUT 8", RoleDir::Out, "-> load / input 8", -1 },
};
const Role* ToolDigitalOut::roles() const { return ROLES; }

static const char* HELP[] = {
    "1-8    toggle that output",
    "A / Z  all high / all low",
    "T      invert every output",
    "P      next pattern",
    "SPACE  run / pause the pattern",
    "+ / -  pattern speed",
    "",
    "Patterns: Walk (one bit moving up),",
    "Bounce (ping-pong), Count (binary),",
    "Alt (alternating halves).",
    "",
    "Outputs are 3.3 V and rated ~20 mA each.",
    "Use a driver for anything bigger.",
};
const char* const* ToolDigitalOut::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

const char* ToolDigitalOut::patName(Pat p) {
    switch (p) {
        case Pat::Off:    return "manual";
        case Pat::Walk:   return "walk";
        case Pat::Bounce: return "bounce";
        case Pat::Count:  return "count";
        case Pat::Alt:    return "alt";
        default:          return "?";
    }
}

void ToolDigitalOut::onEnter() {
    for (int i = 0; i < 8; i++) {
        _lvl[i] = false;
        if (pin(i) >= 0) pins.setOutput(pin(i), false);
    }
    _pat   = Pat::Off;
    _phase = 0;
    _dir   = 1;
}

void ToolDigitalOut::onExit() {
    for (int i = 0; i < 8; i++)
        if (pin(i) >= 0) { pins.write(pin(i), false); pins.release(pin(i)); }
}

void ToolDigitalOut::applyAll() {
    for (int i = 0; i < 8; i++)
        if (pin(i) >= 0) pins.write(pin(i), _lvl[i]);
}

void ToolDigitalOut::stepPattern() {
    switch (_pat) {
        case Pat::Walk:
            for (int i = 0; i < 8; i++) _lvl[i] = (i == _phase);
            _phase = (_phase + 1) % 8;
            break;
        case Pat::Bounce:
            for (int i = 0; i < 8; i++) _lvl[i] = (i == _phase);
            _phase += _dir;
            if (_phase >= 7) { _phase = 7; _dir = -1; }
            if (_phase <= 0) { _phase = 0; _dir =  1; }
            break;
        case Pat::Count:
            for (int i = 0; i < 8; i++) _lvl[i] = (_phase >> i) & 1;
            _phase = (_phase + 1) & 0xFF;
            break;
        case Pat::Alt:
            for (int i = 0; i < 8; i++) _lvl[i] = ((i / 4) == (_phase & 1));
            _phase++;
            break;
        default:
            return;
    }
    applyAll();
}

void ToolDigitalOut::tick() {
    if (_pat == Pat::Off) return;
    uint32_t now = millis();
    if ((uint32_t)(now - _lastStep) < _stepMs) return;
    _lastStep = now;
    stepPattern();
}

bool ToolDigitalOut::onKey(const KeyEvent& ev) {
    int d = ev.digit();
    if (d >= 1 && d <= 8) {
        int i = d - 1;
        if (pin(i) < 0) { ui.notify("out %d unassigned", d); return true; }
        _pat    = Pat::Off;
        _lvl[i] = !_lvl[i];
        pins.write(pin(i), _lvl[i]);
        ui.beep(_lvl[i] ? 2800 : 1900, 12);
        return true;
    }

    if (ev.ci('a') || ev.ci('z')) {
        bool hi = ev.ci('a');
        _pat = Pat::Off;
        for (int i = 0; i < 8; i++) _lvl[i] = hi;
        applyAll();
        return true;
    }
    if (ev.ci('t')) {
        _pat = Pat::Off;
        for (int i = 0; i < 8; i++) _lvl[i] = !_lvl[i];
        applyAll();
        return true;
    }
    if (ev.ci('p')) {
        _pat   = (Pat)(((int)_pat + 1) % (int)Pat::COUNT);
        _phase = 0;
        _dir   = 1;
        ui.notify("pattern: %s", patName(_pat));
        return true;
    }
    if (ev.ch == ' ') {
        if (_pat == Pat::Off) { _pat = Pat::Walk; ui.notify("pattern: walk"); }
        else                  { _pat = Pat::Off;  ui.notify("paused"); }
        return true;
    }
    if (ev.ch == '+' || ev.ch == '=') {
        if (_stepMs > 20) _stepMs -= (_stepMs > 200 ? 50 : 10);
        return true;
    }
    if (ev.ch == '-' || ev.ch == '_') {
        if (_stepMs < 2000) _stepMs += (_stepMs >= 200 ? 50 : 10);
        return true;
    }
    return false;
}

void ToolDigitalOut::draw() {
    char right[16];
    snprintf(right, sizeof(right), "%s", patName(_pat));
    ui.header("Digital Out", right, C_HDR);

    // Two columns of four channels.
    for (int i = 0; i < 8; i++) {
        int col = i / 4, row = i % 4;
        int x = 4 + col * 118;
        int y = BODY_Y + 3 + row * 20;

        bool on = _lvl[i];
        bool ok = pin(i) >= 0;

        ui.g().fillRoundRect(x, y, 112, 18, 3, on ? C_PANEL2 : C_PANEL);
        ui.g().drawRoundRect(x, y, 112, 18, 3, on ? C_HIGH : C_LINE);

        ui.textf(x + 4, y + 6, on ? C_HIGH : C_DIM, "%d", i + 1);

        char lbl[20];
        if (ok) pinLabel(pin(i), lbl, sizeof(lbl));
        else    snprintf(lbl, sizeof(lbl), "unset");
        ui.textf(x + 14, y + 6, ok ? C_TEXT : C_WARN, "%-10.10s", lbl);

        if (ok) ui.state(x + 80, y + 4, on);
    }

    ui.footerf("[1-8] toggle [P] %s [SPC] run [+/-] %lums",
               patName(_pat), (unsigned long)_stepMs);
}

}  // namespace cg
