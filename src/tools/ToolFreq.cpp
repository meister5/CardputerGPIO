#include "ToolFreq.h"

namespace cg {

ToolFreq toolFreq;

static const Role ROLES[] = {
    { "SIGNAL", RoleDir::In, "-> clock / pulse source", -1 },
};
const Role* ToolFreq::roles() const { return ROLES; }

static const char* HELP[] = {
    "G      gate time 100ms / 1s / 10s",
    "R      reset the total count",
    "D      duty measurement on / off",
    "",
    "A longer gate gives more resolution on",
    "slow signals; a shorter one updates",
    "faster. Resolution is 1 / gate: with a",
    "1 s gate you can see 1 Hz steps.",
    "",
    "Duty uses pulseIn() and is blocking, so",
    "it is only trustworthy below ~100 kHz.",
    "Above that it is shown as --.",
    "",
    "Input has a 1 us glitch filter.",
};
const char* const* ToolFreq::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

static const uint32_t GATES[] = { 100, 1000, 10000 };
uint32_t ToolFreq::gateMs(uint8_t i) { return GATES[i % 3]; }

void ToolFreq::onEnter() {
    int g = pin(0);
    _ok = (g >= 0) && _pc.begin(g, 1000);
    if (!_ok) { ui.notify("PCNT unavailable"); return; }
    _pc.startGate(gateMs(_gateIdx));
    _total = 0;
}

void ToolFreq::onExit() {
    _pc.end();
    if (pin(0) >= 0) pins.release(pin(0));
}

void ToolFreq::measureDuty() {
    int g = pin(0);
    if (g < 0) return;

    // pulseIn needs the pin as a plain input; PCNT keeps counting regardless.
    pinMode(g, INPUT);
    uint32_t hi = pulseIn(g, HIGH, 20000);
    uint32_t lo = pulseIn(g, LOW,  20000);
    if (hi == 0 || lo == 0) { _duty = -1; return; }
    _highUs = hi;
    _lowUs  = lo;
    _duty   = (hi * 100.0f) / (hi + lo);
}

void ToolFreq::tick() {
    if (!_ok) return;

    if (_pc.ready()) {
        _hz     = _pc.hz();
        _total += (int64_t)(_hz * gateMs(_gateIdx) / 1000.0f);
        _pc.startGate(gateMs(_gateIdx));
    }

    if (_dutyOn && _hz > 0 && _hz < 100000.0f) {
        uint32_t now = millis();
        if ((uint32_t)(now - _lastDuty) > 500) {
            _lastDuty = now;
            measureDuty();
        }
    } else if (_hz >= 100000.0f) {
        _duty = -1;
    }
}

bool ToolFreq::onKey(const KeyEvent& ev) {
    if (ev.ci('g')) {
        _gateIdx = (uint8_t)((_gateIdx + 1) % 3);
        _pc.startGate(gateMs(_gateIdx));
        ui.notify("gate %lu ms", (unsigned long)gateMs(_gateIdx));
        return true;
    }
    if (ev.ci('r')) { _total = 0; _pc.clear(); ui.notify("count cleared"); return true; }
    if (ev.ci('d')) { _dutyOn = !_dutyOn; ui.notify(_dutyOn ? "duty on" : "duty off"); return true; }
    return false;
}

void ToolFreq::draw() {
    char right[16];
    snprintf(right, sizeof(right), "gate %lums", (unsigned long)gateMs(_gateIdx));
    ui.header("Frequency Counter", right, C_HDR);

    if (!_ok) {
        ui.text(8, BODY_Y + 30, C_LOW, "Could not claim a PCNT unit.");
        ui.text(8, BODY_Y + 44, C_DIM, "Close another counting tool first.");
        ui.footer("[DEL] back");
        return;
    }

    // ── Frequency, in whatever unit reads best ────────────────────────────
    char buf[24];
    if      (_hz >= 1000000.0f) snprintf(buf, sizeof(buf), "%.4f", _hz / 1000000.0f);
    else if (_hz >= 1000.0f)    snprintf(buf, sizeof(buf), "%.3f", _hz / 1000.0f);
    else                        snprintf(buf, sizeof(buf), "%.1f", _hz);
    const char* unit = (_hz >= 1000000.0f) ? "MHz" : (_hz >= 1000.0f ? "kHz" : "Hz");

    ui.textBig(6, BODY_Y + 4, _hz > 0 ? C_HIGH : C_DIM, 3, buf);
    ui.textBig(6 + (int)strlen(buf) * 18 + 6, BODY_Y + 14, C_DIM, 1, unit);

    // ── Period ────────────────────────────────────────────────────────────
    ui.text(6, BODY_Y + 32, C_DIM, "period");
    if (_hz > 0) {
        float us = 1000000.0f / _hz;
        if      (us >= 1000.0f) ui.textf(56, BODY_Y + 32, C_TEXT, "%.3f ms", us / 1000.0f);
        else                    ui.textf(56, BODY_Y + 32, C_TEXT, "%.2f us", us);
    } else {
        ui.text(56, BODY_Y + 32, C_FAINT, "--");
    }

    // ── Duty ──────────────────────────────────────────────────────────────
    ui.text(6, BODY_Y + 44, C_DIM, "duty");
    if (!_dutyOn) {
        ui.text(56, BODY_Y + 44, C_FAINT, "off");
    } else if (_duty < 0) {
        ui.text(56, BODY_Y + 44, C_FAINT, "-- (too fast)");
    } else {
        ui.textf(56, BODY_Y + 44, C_TEXT, "%.1f%%", _duty);
        ui.hbar(120, BODY_Y + 44, 110, 9, _duty, C_ROLE_PWM);
        ui.textf(6, BODY_Y + 56, C_FAINT, "hi %luus  lo %luus",
                 (unsigned long)_highUs, (unsigned long)_lowUs);
    }

    ui.textf(6, BODY_B - 9, C_FAINT, "total %lld edges", (long long)_total);
    ui.footer("[G] gate  [R] reset  [D] duty");
}


const char* ToolFreq::logHeader() const { return "hz,duty_pct,total_edges"; }

bool ToolFreq::logRow(char* out, size_t n) {
    if (!_ok) return false;
    snprintf(out, n, "%.3f,%.2f,%lld", _hz, _duty, (long long)_total);
    return true;
}

}  // namespace cg
