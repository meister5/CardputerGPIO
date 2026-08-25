#include "ToolServo.h"

namespace cg {

ToolServo toolServo;

static const Role ROLES[] = {
    { "SERVO 1", RoleDir::Pwm, "-> servo signal (orange)", -1 },
    { "SERVO 2", RoleDir::Pwm, "-> servo signal 2", -1 },
    { "SERVO 3", RoleDir::Pwm, "-> servo signal 3", -1 },
    { "SERVO 4", RoleDir::Pwm, "-> servo signal 4", -1 },
};
const Role* ToolServo::roles() const { return ROLES; }

static const char* HELP[] = {
    "1-4    select / attach a channel",
    "< >    move 1 degree      +/-  10 deg",
    "C      centre (1500 us)",
    "S      sweep the selected channel",
    "D      detach (stop pulsing)",
    "N / X  set this position as min / max",
    "",
    "Power servos from 5V on the EXT header,",
    "not from a GPIO, and tie grounds",
    "together. A stalled servo will brown out",
    "the board if it shares the 3.3 V rail.",
};
const char* const* ToolServo::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

int ToolServo::angleOf(int i) const {
    int span = _maxUs - _minUs;
    if (span <= 0) return 0;
    int a = ((_us[i] - _minUs) * 180) / span;
    return a < 0 ? 0 : (a > 180 ? 180 : a);
}

void ToolServo::applyChan(int i) {
    int g = pin(i);
    if (g < 0) return;
    if (!_live[i]) {
        if (pins.setPwm(g, FRAME_HZ, 0) == 0) {
            ui.notify("ch%d: LEDC busy", i + 1);
            return;
        }
        _live[i] = true;
    }
    float pct = (_us[i] * 100.0f) / FRAME_US;
    pins.pwmSetDuty(g, pct);
}

void ToolServo::detachChan(int i) {
    int g = pin(i);
    if (g >= 0) pins.release(g);
    _live[i] = false;
}

void ToolServo::onEnter() {
    for (int i = 0; i < CH; i++) { _us[i] = 1500; _live[i] = false; }
    _sel   = 0;
    _sweep = false;
    applyChan(0);
}

void ToolServo::onExit() {
    for (int i = 0; i < CH; i++) detachChan(i);
}

void ToolServo::tick() {
    if (!_sweep) return;
    uint32_t now = millis();
    if ((uint32_t)(now - _lastSweep) < 20) return;
    _lastSweep = now;

    _us[_sel] += _sweepDir * 15;
    if (_us[_sel] >= _maxUs) { _us[_sel] = _maxUs; _sweepDir = -1; }
    if (_us[_sel] <= _minUs) { _us[_sel] = _minUs; _sweepDir =  1; }
    applyChan(_sel);
}

bool ToolServo::onKey(const KeyEvent& ev) {
    int d = ev.digit();
    if (d >= 1 && d <= CH) {
        _sel   = d - 1;
        _sweep = false;
        if (pin(_sel) < 0) ui.notify("ch%d unassigned", d);
        else               applyChan(_sel);
        return true;
    }

    auto move = [&](int delta) {
        _sweep = false;
        _us[_sel] += delta;
        if (_us[_sel] < _minUs) _us[_sel] = _minUs;
        if (_us[_sel] > _maxUs) _us[_sel] = _maxUs;
        applyChan(_sel);
    };

    switch (ev.key) {
        case Key::Left:  move(-(_maxUs - _minUs) / 180);       return true;
        case Key::Right: move(+(_maxUs - _minUs) / 180);       return true;
        case Key::Up:    move(+(_maxUs - _minUs) / 18);        return true;
        case Key::Down:  move(-(_maxUs - _minUs) / 18);        return true;
        case Key::Char:
            if (ev.ch == '+' || ev.ch == '=') { move(+(_maxUs - _minUs) / 18); return true; }
            if (ev.ch == '-' || ev.ch == '_') { move(-(_maxUs - _minUs) / 18); return true; }
            if (ev.ci('c')) { _sweep = false; _us[_sel] = 1500; applyChan(_sel); return true; }
            if (ev.ci('s')) {
                _sweep = !_sweep;
                ui.notify(_sweep ? "sweeping ch%d" : "sweep off", _sel + 1);
                return true;
            }
            if (ev.ci('d')) { _sweep = false; detachChan(_sel); ui.notify("ch%d detached", _sel + 1); return true; }
            if (ev.ci('n')) { _minUs = _us[_sel]; ui.notify("min = %d us", _minUs); return true; }
            if (ev.ci('x')) { _maxUs = _us[_sel]; ui.notify("max = %d us", _maxUs); return true; }
            return false;
        default:
            return false;
    }
}

void ToolServo::draw() {
    char right[16];
    snprintf(right, sizeof(right), "ch%d%s", _sel + 1, _sweep ? " sweep" : "");
    ui.header("Servo Driver", right, C_HDR);

    // ── Big readout for the selected channel ──────────────────────────────
    ui.textBigf(6, BODY_Y + 4, C_ROLE_PWM, 3, "%3d", angleOf(_sel));
    ui.textBig(60, BODY_Y + 14, C_DIM, 1, "deg");
    ui.textBigf(92, BODY_Y + 8, C_TEXT, 2, "%4d us", _us[_sel]);

    // Dial: a 180 degree arc with a needle at the current angle.
    int cx = 200, cy = BODY_Y + 34, r = 26;
    for (int a = 0; a <= 180; a += 6) {
        float rad = (180 - a) * 3.14159f / 180.0f;
        int x = cx + (int)(cosf(rad) * r);
        int y = cy - (int)(sinf(rad) * r);
        ui.g().drawPixel(x, y, C_FAINT);
    }
    float nrad = (180 - angleOf(_sel)) * 3.14159f / 180.0f;
    ui.g().drawLine(cx, cy, cx + (int)(cosf(nrad) * r), cy - (int)(sinf(nrad) * r),
                    C_ROLE_PWM);
    ui.g().fillCircle(cx, cy, 2, C_ROLE_PWM);

    // ── Channel strip ─────────────────────────────────────────────────────
    int y = BODY_Y + 44;
    for (int i = 0; i < CH; i++) {
        bool sel = (i == _sel);
        uint16_t col = _live[i] ? C_HIGH : C_FAINT;
        ui.g().fillRect(4, y, 156, 12, sel ? C_SEL : C_BG);
        ui.textf(6, y + 2, sel ? C_WHITE : C_DIM, "%d", i + 1);

        char lbl[20];
        if (pin(i) >= 0) pinLabel(pin(i), lbl, sizeof(lbl));
        else             snprintf(lbl, sizeof(lbl), "unset");
        ui.textf(16, y + 2, pin(i) >= 0 ? C_TEXT : C_WARN, "%-11.11s", lbl);
        ui.textf(88, y + 2, col, "%4dus", _us[i]);
        ui.textf(130, y + 2, col, _live[i] ? "live" : "off");
        y += 13;
    }

    ui.footer("[1-4]ch [<>]1deg [^v]10 [S]swp");
}

}  // namespace cg
