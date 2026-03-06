/**
 * ProfileDigitalIn.cpp  (v2 - fixed)
 */

#include "ProfileDigitalIn.h"

const PinRole ProfileDigitalIn::ROLES[] = {
    { "IN 1", PinDir::INPUT_ROLE, "-> signal source 1" },
    { "IN 2", PinDir::INPUT_ROLE, "-> signal source 2" },
    { "IN 3", PinDir::INPUT_ROLE, "-> signal source 3" },
    { "IN 4", PinDir::INPUT_ROLE, "-> signal source 4" },
    { "IN 5", PinDir::INPUT_ROLE, "-> signal source 5" },
    { "IN 6", PinDir::INPUT_ROLE, "-> signal source 6" },
};
const int ProfileDigitalIn::ROLE_COUNT = 6;

void ProfileDigitalIn::onEnter(PinManager* pm, const PinConfig* cfg) {
    _pinCount = cfg->roleCount();
    for (int i = 0; i < _pinCount; i++) {
        _pins[i]      = cfg->pin(i);
        _edgeCount[i] = 0;
        if (_pins[i] < 0) continue;
        pm->configureInput(_pins[i], true);
        _lastState[i] = pm->readPin(_pins[i]);
    }
    _lastDraw = millis() - 200;  // force draw on first update()
}

void ProfileDigitalIn::onExit(PinManager* pm) {
    for (int i = 0; i < _pinCount; i++) pm->releasePin(_pins[i]);
}

void ProfileDigitalIn::onKey(PinManager* pm, char key) {
    if (key == 'r' || key == 'R') memset(_edgeCount, 0, sizeof(_edgeCount));
}

void ProfileDigitalIn::update(PinManager* pm) {
    for (int i = 0; i < _pinCount; i++) {
        bool cur = pm->readPin(_pins[i]);
        if (cur != _lastState[i]) { _edgeCount[i]++; _lastState[i] = cur; }
    }
    unsigned long now = millis();
    if (now - _lastDraw >= 50) { _lastDraw = now; drawDisplay(pm); }
}

void ProfileDigitalIn::drawDisplay(PinManager* pm) {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(C_BG);
    drawHeader("Digital Input Monitor");

    int rowH = 17, startY = 22;
    for (int i = 0; i < _pinCount; i++) {
        int y   = startY + i * rowH;
        const PinState* ps = pm->stateOf(_pins[i]);
        bool hi = ps ? ps->digital : false;

        M5Cardputer.Display.setTextColor(C_TEXT);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setCursor(4, y + 3);
        M5Cardputer.Display.printf("GPIO%2d", _pins[i]);

        drawState(52, y + 2, hi);

        M5Cardputer.Display.setTextColor(C_DIM);
        M5Cardputer.Display.setCursor(96, y + 3);
        M5Cardputer.Display.printf("edges:%4d", _edgeCount[i]);

        M5Cardputer.Display.fillCircle(224, y + 8, 5, hi ? (uint32_t)C_HIGH : (uint32_t)C_LOW);
    }

    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.setCursor(4, SCR_H - 10);
    M5Cardputer.Display.print("[R] reset edge counters");
}
