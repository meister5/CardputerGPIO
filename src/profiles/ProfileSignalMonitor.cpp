/**
 * ProfileSignalMonitor.cpp  (v2 - fixed)
 */

#include "ProfileSignalMonitor.h"

const PinRole ProfileSignalMonitor::ROLES[] = {
    { "SIGNAL IN", PinDir::INPUT_ROLE, "-> digital signal source" },
};
const int ProfileSignalMonitor::ROLE_COUNT = 1;

void ProfileSignalMonitor::onEnter(PinManager* pm, const PinConfig* cfg) {
    _pin          = cfg->pin(0);
    _edgeCount    = 0;
    _periodUs     = 0;
    _lastRisingUs = 0;
    pm->configureInput(_pin, true);
    _lastState = pm->readPin(_pin);
    _lastDraw  = millis() - 200;  // force draw on first update()
}

void ProfileSignalMonitor::onExit(PinManager* pm) {
    if (_pin >= 0) pm->releasePin(_pin);
}

void ProfileSignalMonitor::onKey(PinManager* pm, char key) {
    if (key == 'r' || key == 'R') {
        _edgeCount    = 0;
        _periodUs     = 0;
        _lastRisingUs = micros();
    }
}

void ProfileSignalMonitor::update(PinManager* pm) {
    if (_pin < 0) return;

    bool cur = pm->readPin(_pin);
    if (cur != _lastState) {
        _edgeCount++;
        if (cur == true) {
            unsigned long now = micros();
            if (_lastRisingUs > 0) _periodUs = now - _lastRisingUs;
            _lastRisingUs = now;
        }
        _lastState = cur;
    }

    unsigned long now = millis();
    if (now - _lastDraw >= 100) { _lastDraw = now; drawDisplay(); }
}

void ProfileSignalMonitor::drawDisplay() {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(C_BG);
    drawHeader("Signal Monitor");

    M5Cardputer.Display.setTextColor(C_TEXT);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(4, 22);
    M5Cardputer.Display.printf("Monitoring: GPIO%d", _pin);

    drawState(150, 20, _lastState);

    M5Cardputer.Display.setTextColor(C_TEXT);
    M5Cardputer.Display.setCursor(4, 38);
    M5Cardputer.Display.printf("Edges:  %d", _edgeCount);

    M5Cardputer.Display.setCursor(4, 54);
    if (_periodUs > 0 && _periodUs < 2000000) {
        float hz = 1000000.0f / _periodUs;
        if (hz >= 1000.0f)
            M5Cardputer.Display.printf("Freq:   %.2f kHz", hz / 1000.0f);
        else
            M5Cardputer.Display.printf("Freq:   %.1f Hz", hz);
    } else {
        M5Cardputer.Display.print("Freq:   --- (no signal)");
    }

    M5Cardputer.Display.setCursor(4, 70);
    if (_periodUs > 0)
        M5Cardputer.Display.printf("Period: %lu us", _periodUs);
    else
        M5Cardputer.Display.print("Period: ---");

    static bool blink = false;
    if (_edgeCount % 2 == 0) blink = !blink;
    M5Cardputer.Display.fillCircle(SCR_W - 12, 30, 6, blink ? (uint32_t)C_HIGH : (uint32_t)C_DIM);

    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.setCursor(4, SCR_H - 10);
    M5Cardputer.Display.print("[R] reset counters");
}
