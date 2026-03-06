/**
 * ProfileAnalogReader.cpp  (v2 - fixed)
 */

#include "ProfileAnalogReader.h"

const PinRole ProfileAnalogReader::ROLES[] = {
    { "ADC 1", PinDir::ADC_ROLE, "-> sensor output 1" },
    { "ADC 2", PinDir::ADC_ROLE, "-> sensor output 2" },
    { "ADC 3", PinDir::ADC_ROLE, "-> sensor output 3" },
    { "ADC 4", PinDir::ADC_ROLE, "-> sensor output 4" },
};
const int ProfileAnalogReader::ROLE_COUNT = 4;

void ProfileAnalogReader::onEnter(PinManager* pm, const PinConfig* cfg) {
    _pinCount = cfg->roleCount();
    for (int i = 0; i < _pinCount; i++) {
        _pins[i] = cfg->pin(i);
        if (_pins[i] < 0) continue;
        pm->configureADC(_pins[i]);
    }
    _lastDraw = millis() - 200;  // force draw on first update()
}

void ProfileAnalogReader::onExit(PinManager* pm) {
    for (int i = 0; i < _pinCount; i++) pm->releasePin(_pins[i]);
}

void ProfileAnalogReader::onKey(PinManager* /*pm*/, char /*key*/) {}

void ProfileAnalogReader::update(PinManager* pm) {
    for (int i = 0; i < _pinCount; i++) pm->readADC(_pins[i]);
    unsigned long now = millis();
    if (now - _lastDraw >= 100) { _lastDraw = now; drawDisplay(pm); }
}

void ProfileAnalogReader::drawDisplay(PinManager* pm) {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(C_BG);
    drawHeader("Analog Sensor Reader");

    int rowH = 25, startY = 22;
    for (int i = 0; i < _pinCount; i++) {
        const PinState* s = pm->stateOf(_pins[i]);
        if (!s) continue;
        drawChannel(4, startY + i * rowH, _pins[i], s->adcRaw, s->voltage);
    }

    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.setCursor(4, SCR_H - 10);
    M5Cardputer.Display.print("ADC 12-bit  |  3.3V ref  |  4x oversampling");
}

void ProfileAnalogReader::drawChannel(int x, int y, int pin, int raw, float volts) {
    M5Cardputer.Display.setTextColor(C_TITLE);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(x, y);
    M5Cardputer.Display.printf("GPIO%2d", pin);

    M5Cardputer.Display.setTextColor(C_HIGH);
    M5Cardputer.Display.setCursor(x + 44, y);
    M5Cardputer.Display.printf("%.2fV", volts);

    M5Cardputer.Display.setTextColor(C_TEXT);
    M5Cardputer.Display.setCursor(x + 86, y);
    M5Cardputer.Display.printf("raw:%4d", raw);

    int pct = (int)((raw / 4095.0f) * 100);
    uint32_t col = (volts < 1.0f) ? 0x00aaaau : (volts < 2.5f) ? 0x00ff88u : 0xffaa00u;
    drawBarGraph(x, y + 12, SCR_W - 8, 9, pct, col);
}
