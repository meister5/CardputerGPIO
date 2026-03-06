/**
 * ProfilePWMGen.cpp  (v2 - fixed)
 */

#include "ProfilePWMGen.h"

const PinRole ProfilePWMGen::ROLES[] = {
    { "PWM OUT", PinDir::PWM_ROLE, "-> oscilloscope / circuit" },
};
const int ProfilePWMGen::ROLE_COUNT = 1;
constexpr int ProfilePWMGen::FREQ_STEPS[];

void ProfilePWMGen::onEnter(PinManager* pm, const PinConfig* cfg) {
    _pin     = cfg->pin(0);
    _freqIdx = 5;
    _freq    = FREQ_STEPS[_freqIdx];
    _duty    = 50;
    _active  = false;
    _dirty   = true;
}

void ProfilePWMGen::onExit(PinManager* pm) {
    if (_active && _pin >= 0) { pm->releasePin(_pin); _active = false; }
}

void ProfilePWMGen::update(PinManager* pm) {
    if (_dirty) { drawDisplay(); _dirty = false; }
}

void ProfilePWMGen::onKey(PinManager* pm, char key) {
    switch (key) {
        case 'F':
            if (_freqIdx < FREQ_STEP_COUNT - 1) {
                _freqIdx++; _freq = FREQ_STEPS[_freqIdx];
                if (_active) pm->setPWMFreq(_pin, _freq);
                _dirty = true;
            }
            break;
        case 'f':
            if (_freqIdx > 0) {
                _freqIdx--; _freq = FREQ_STEPS[_freqIdx];
                if (_active) pm->setPWMFreq(_pin, _freq);
                _dirty = true;
            }
            break;
        case 'D':
            _duty = min(95, _duty + 5);
            if (_active) pm->setPWMDuty(_pin, _duty);
            _dirty = true;
            break;
        case 'd':
            _duty = max(5, _duty - 5);
            if (_active) pm->setPWMDuty(_pin, _duty);
            _dirty = true;
            break;
        case ' ':
            if (_active) { pm->releasePin(_pin); _active = false; }
            else         { applyPWM(pm); }
            _dirty = true;
            break;
    }
}

void ProfilePWMGen::applyPWM(PinManager* pm) {
    if (_pin < 0) return;
    pm->configurePWM(_pin, _freq, _duty);
    _active = true;
}

void ProfilePWMGen::drawDisplay() {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(C_BG);
    drawHeader("PWM Generator");

    char fbuf[16];

    uint32_t stCol = _active ? (uint32_t)C_HIGH : (uint32_t)C_LOW;
    M5Cardputer.Display.fillRoundRect(SCR_W - 44, 2, 40, 14, 4, stCol);
    M5Cardputer.Display.setTextColor(0x000000u);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(SCR_W - 38, 5);
    M5Cardputer.Display.print(_active ? " ON" : "OFF");

    M5Cardputer.Display.setTextColor(C_TEXT);
    M5Cardputer.Display.setCursor(4, 22);
    M5Cardputer.Display.printf("Pin: GPIO%d", _pin);

    M5Cardputer.Display.setTextColor(C_TITLE);
    M5Cardputer.Display.setCursor(4, 36);
    M5Cardputer.Display.printf("Freq: %s", formatFreq(_freq, fbuf, sizeof(fbuf)));
    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.print("  [F/f]");

    M5Cardputer.Display.setTextColor(C_TITLE);
    M5Cardputer.Display.setCursor(4, 50);
    M5Cardputer.Display.printf("Duty: %3d%%", _duty);
    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.print("  [D/d]");

    drawBarGraph(4, 63, SCR_W - 8, 10, _duty, C_BAR);
    drawWaveform(4, 80, SCR_W - 8, 30, _duty);

    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.setCursor(4, SCR_H - 10);
    M5Cardputer.Display.print("[SPACE] start/stop");
}

void ProfilePWMGen::drawWaveform(int x, int y, int w, int h, int dutyPct) {
    int highW = (int)((dutyPct / 100.0f) * w);
    int midY  = y + h / 2;
    int top   = y + 2;

    M5Cardputer.Display.fillRect(x, y, w, h, C_BG);
    M5Cardputer.Display.drawFastVLine(x,          top,  midY - top, C_WAVE_H);
    M5Cardputer.Display.drawFastHLine(x,          top,  highW,      C_WAVE_H);
    M5Cardputer.Display.drawFastVLine(x + highW,  top,  midY - top, C_WAVE_H);
    M5Cardputer.Display.drawFastHLine(x + highW,  midY, w - highW,  C_WAVE_H);
    M5Cardputer.Display.drawFastVLine(x + w,      midY, midY - top, C_WAVE_H);

    char fbuf[12];
    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(x + 2, y + h - 10);
    M5Cardputer.Display.printf("1 cycle @ %s", formatFreq(_freq, fbuf, sizeof(fbuf)));
}

const char* ProfilePWMGen::formatFreq(int hz, char* buf, int len) {
    if (hz >= 1000000)   snprintf(buf, len, "%.1f MHz", hz / 1000000.0f);
    else if (hz >= 1000) snprintf(buf, len, "%.1f kHz", hz / 1000.0f);
    else                 snprintf(buf, len, "%d Hz",    hz);
    return buf;
}
