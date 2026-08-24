#include "ToolAnalogIn.h"

namespace cg {

ToolAnalogIn toolAnalogIn;

static const Role ROLES[] = {
    { "AIN 1", RoleDir::Adc, "-> analog source 1", -1 },
    { "AIN 2", RoleDir::Adc, "-> analog source 2", -1 },
    { "AIN 3", RoleDir::Adc, "-> analog source 3", -1 },
    { "AIN 4", RoleDir::Adc, "-> analog source 4", -1 },
};
const Role* ToolAnalogIn::roles() const { return ROLES; }

static const char* HELP[] = {
    "Calibrated millivolts on four channels.",
    "",
    "  R      clear min/max/average",
    "  H      hold / release the display",
    "  O      oversampling 1/4/8/16/32",
    "  S      input scale (divider preset)",
    "",
    "Scale presets assume a resistor divider",
    "on the input:  x2 = two equal resistors,",
    "x11 = 100k over 10k. The ADC itself only",
    "ever sees 0-3.3 V; never exceed that.",
    "",
    "ADC1 pins (G1-G6) are the accurate ones.",
};
const char* const* ToolAnalogIn::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

float ToolAnalogIn::scaleMul(uint8_t s) {
    switch (s) { case 1: return 2.0f; case 2: return 11.0f; default: return 1.0f; }
}
const char* ToolAnalogIn::scaleName(uint8_t s) {
    switch (s) { case 1: return "x2"; case 2: return "x11"; default: return "x1"; }
}

void ToolAnalogIn::resetStats() {
    for (int i = 0; i < CH; i++) _c[i] = Chan{};
}

void ToolAnalogIn::onEnter() {
    resetStats();
    for (int i = 0; i < CH; i++)
        if (pin(i) >= 0) pins.setAdc(pin(i));
}

void ToolAnalogIn::onExit() {
    for (int i = 0; i < CH; i++)
        if (pin(i) >= 0) pins.release(pin(i));
}

void ToolAnalogIn::tick() {
    if (_hold) return;
    uint32_t now = millis();
    if ((uint32_t)(now - _lastPoll) < 80) return;
    _lastPoll = now;

    for (int i = 0; i < CH; i++) {
        int g = pin(i);
        if (g < 0) continue;
        uint32_t mv = pins.adcMilliVoltsAvg(g, _osr);
        _c[i].mv = mv;
        if (mv < _c[i].mn) _c[i].mn = mv;
        if (mv > _c[i].mx) _c[i].mx = mv;
        _c[i].avg = (_c[i].avg == 0) ? mv : (_c[i].avg * 0.9f + mv * 0.1f);
    }
}

bool ToolAnalogIn::onKey(const KeyEvent& ev) {
    if (ev.ci('r')) { resetStats(); ui.notify("stats cleared"); return true; }
    if (ev.ci('h')) { _hold = !_hold; ui.notify(_hold ? "hold" : "live"); return true; }
    if (ev.ci('o')) {
        _osr = (_osr >= 32) ? 1 : (uint8_t)(_osr * 2);
        if (_osr == 2) _osr = 4;
        ui.notify("oversample x%u", _osr);
        return true;
    }
    if (ev.ci('s')) {
        _scale = (uint8_t)((_scale + 1) % 3);
        resetStats();
        ui.notify("scale %s", scaleName(_scale));
        return true;
    }
    return false;
}

void ToolAnalogIn::draw() {
    char right[16];
    snprintf(right, sizeof(right), "%s%s", scaleName(_scale), _hold ? " HOLD" : "");
    ui.header("Analog In", right, C_HDR);

    float mul = scaleMul(_scale);
    int y = BODY_Y + 2;

    for (int i = 0; i < CH; i++) {
        int g = pin(i);
        if (g < 0) {
            ui.textf(6, y + 6, C_WARN, "AIN %d  unassigned", i + 1);
            y += 25;
            continue;
        }

        float v    = (_c[i].mv * mul) / 1000.0f;
        float vmin = (_c[i].mn == 0xFFFFFFFF ? 0 : _c[i].mn * mul) / 1000.0f;
        float vmax = (_c[i].mx * mul) / 1000.0f;

        ui.textf(4, y, C_DIM, "G%-2d", g);
        ui.textBigf(24, y - 2, C_ROLE_ADC, 2, "%5.3f", v);
        ui.text(94, y + 4, C_DIM, "V");

        ui.textf(108, y - 1, C_FAINT, "min %5.3f", vmin);
        ui.textf(108, y + 7, C_FAINT, "max %5.3f", vmax);

        // Bar is always against the raw 0-3.3 V the pin actually sees.
        ui.hbar(180, y + 1, 54, 10, (_c[i].mv * 100.0f) / 3300.0f, C_ROLE_ADC);
        y += 25;
    }

    ui.footerf("[R] reset [H] hold [O] avg x%u [S] scale", _osr);
}


const char* ToolAnalogIn::logHeader() const { return "ch0_mV,ch1_mV,ch2_mV,ch3_mV"; }

bool ToolAnalogIn::logRow(char* out, size_t n) {
    // Unassigned channels log as an empty field rather than a zero, so a
    // spreadsheet does not plot a flat line that was never measured.
    int w = 0;
    for (int i = 0; i < CH; i++) {
        float mv = _c[i].mv * scaleMul(_scale);
        if (pin(i) >= 0) w += snprintf(out + w, n - w, "%s%.1f", i ? "," : "", mv);
        else             w += snprintf(out + w, n - w, "%s", i ? "," : "");
        if (w >= (int)n) return false;
    }
    return true;
}

}  // namespace cg
