/**
 * ProfileLogicAnalyzer.cpp  (v2 - fixed)
 */

#include "ProfileLogicAnalyzer.h"

const PinRole ProfileLogicAnalyzer::ROLES[] = {
    { "CH 1", PinDir::INPUT_ROLE, "-> digital signal 1" },
    { "CH 2", PinDir::INPUT_ROLE, "-> digital signal 2" },
    { "CH 3", PinDir::INPUT_ROLE, "-> digital signal 3" },
    { "CH 4", PinDir::INPUT_ROLE, "-> digital signal 4" },
};
const int ProfileLogicAnalyzer::ROLE_COUNT = 4;
constexpr int ProfileLogicAnalyzer::INTERVAL_PRESETS[];

void ProfileLogicAnalyzer::onEnter(PinManager* pm, const PinConfig* cfg) {
    for (int i = 0; i < CHANNELS; i++) {
        _pins[i] = cfg->pin(i);
        if (_pins[i] >= 0) pm->configureInput(_pins[i], true);
    }
    memset(_buf, 0, sizeof(_buf));
    _sampleIdx    = 0;
    _bufFull      = false;
    _lastSampleUs = micros();
    _dirty        = true;
}

void ProfileLogicAnalyzer::onExit(PinManager* pm) {
    for (int i = 0; i < CHANNELS; i++)
        if (_pins[i] >= 0) pm->releasePin(_pins[i]);
}

void ProfileLogicAnalyzer::onKey(PinManager* /*pm*/, char key) {
    if (key == '+' && _intervalIdx > 0) {
        _intervalIdx--;
        _intervalUs = INTERVAL_PRESETS[_intervalIdx];
    } else if (key == '-' && _intervalIdx < INTERVAL_COUNT - 1) {
        _intervalIdx++;
        _intervalUs = INTERVAL_PRESETS[_intervalIdx];
    }
}

void ProfileLogicAnalyzer::update(PinManager* pm) {
    unsigned long now = micros();
    if ((long)(now - _lastSampleUs) >= _intervalUs) {
        _lastSampleUs = now;
        captureSample(pm);
    }
    if (_dirty) { drawWaveforms(); _dirty = false; }
}

void ProfileLogicAnalyzer::captureSample(PinManager* pm) {
    for (int ch = 0; ch < CHANNELS; ch++) {
        _buf[ch][_sampleIdx] = (_pins[ch] >= 0 && pm->readPin(_pins[ch])) ? 1 : 0;
    }
    if (++_sampleIdx >= SAMPLES) {
        _sampleIdx = 0;
        _bufFull   = true;
        _dirty     = true;
    } else if (_sampleIdx % 20 == 0) {
        _dirty = true;  // update progress bar every 20 samples
    }
}

void ProfileLogicAnalyzer::drawWaveforms() {
    if (!_bufFull) {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(C_BG);
        drawHeader("Logic Analyzer");
        M5Cardputer.Display.setTextColor(C_TEXT);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setCursor(4, 60);
        M5Cardputer.Display.print("Capturing...");
        int pct = (int)((_sampleIdx / (float)SAMPLES) * 100);
        drawBarGraph(4, 78, SCR_W - 8, 12, pct, C_BAR);
        return;
    }

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(C_BG);
    drawHeader("Logic Analyzer");

    static constexpr int LABEL_W = 44;
    static constexpr int WAVE_X  = LABEL_W + 2;
    static constexpr int WAVE_W  = SCR_W - WAVE_X - 2;
    static constexpr int CH_H    = 24;
    static constexpr int START_Y = 20;
    static constexpr int RISE    = 7;

    for (int ch = 0; ch < CHANNELS; ch++) {
        int midY = START_Y + ch * CH_H + CH_H / 2;

        M5Cardputer.Display.setTextColor(C_TEXT);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setCursor(2, midY - 4);
        if (_pins[ch] >= 0)
            M5Cardputer.Display.printf("G%2d", _pins[ch]);
        else
            M5Cardputer.Display.print("---");

        int prevX   = WAVE_X;
        int prevVal = _buf[ch][_sampleIdx];

        for (int s = 0; s < SAMPLES; s++) {
            int idx = (_sampleIdx + s) % SAMPLES;
            int val = _buf[ch][idx];
            int x   = WAVE_X + (int)((s / (float)SAMPLES) * WAVE_W);
            int yH  = midY - RISE;
            int yL  = midY + RISE;
            int y   = val ? yH : yL;

            if (s > 0) {
                int prevY = prevVal ? yH : yL;
                if (val != prevVal) {
                    M5Cardputer.Display.drawFastVLine(x, min(y, prevY),
                        abs(y - prevY) + 1, C_WAVE_H);
                }
                M5Cardputer.Display.drawFastHLine(prevX, y, x - prevX,
                    val ? (uint32_t)C_WAVE_H : (uint32_t)C_WAVE_L);
            }
            prevX   = x;
            prevVal = val;
        }
    }

    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(4, SCR_H - 10);
    int usPerDiv = (_intervalUs * SAMPLES) / 10;
    if (usPerDiv >= 1000)
        M5Cardputer.Display.printf("[+/-] %.1fms/div", usPerDiv / 1000.0f);
    else
        M5Cardputer.Display.printf("[+/-] %dus/div", usPerDiv);
}
