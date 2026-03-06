/**
 * ProfileDigitalOut.cpp  (v2 - fixed)
 */

#include "ProfileDigitalOut.h"

const PinRole ProfileDigitalOut::ROLES[] = {
    { "OUT 1", PinDir::OUTPUT_ROLE, "-> circuit input 1" },
    { "OUT 2", PinDir::OUTPUT_ROLE, "-> circuit input 2" },
    { "OUT 3", PinDir::OUTPUT_ROLE, "-> circuit input 3" },
    { "OUT 4", PinDir::OUTPUT_ROLE, "-> circuit input 4" },
    { "OUT 5", PinDir::OUTPUT_ROLE, "-> circuit input 5" },
    { "OUT 6", PinDir::OUTPUT_ROLE, "-> circuit input 6" },
    { "OUT 7", PinDir::OUTPUT_ROLE, "-> circuit input 7" },
    { "OUT 8", PinDir::OUTPUT_ROLE, "-> circuit input 8" },
};
const int ProfileDigitalOut::ROLE_COUNT = 8;

void ProfileDigitalOut::onEnter(PinManager* pm, const PinConfig* cfg) {
    _pinCount = cfg->roleCount();
    for (int i = 0; i < _pinCount; i++) {
        _pins[i] = cfg->pin(i);
        if (_pins[i] < 0) continue;
        pm->configureOutput(_pins[i]);
        pm->writePin(_pins[i], false);
    }
    _dirty = true;
}

void ProfileDigitalOut::onExit(PinManager* pm) {
    for (int i = 0; i < _pinCount; i++) pm->releasePin(_pins[i]);
}

void ProfileDigitalOut::update(PinManager* pm) {
    if (_dirty) { drawDisplay(pm); _dirty = false; }
}

void ProfileDigitalOut::onKey(PinManager* pm, char key) {
    if (key >= '1' && key <= '8') {
        int idx = key - '1';
        if (idx < _pinCount) { pm->togglePin(_pins[idx]); _dirty = true; }
    } else if (key == 'a' || key == 'A') {
        for (int i = 0; i < _pinCount; i++) pm->writePin(_pins[i], true);
        _dirty = true;
    } else if (key == 'z' || key == 'Z') {
        for (int i = 0; i < _pinCount; i++) pm->writePin(_pins[i], false);
        _dirty = true;
    }
}

void ProfileDigitalOut::drawDisplay(PinManager* pm) {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(C_BG);
    drawHeader("Digital Output Panel");

    int rowH = 14, startY = 22;
    for (int i = 0; i < _pinCount; i++) {
        int y   = startY + i * rowH;
        const PinState* ps = pm->stateOf(_pins[i]);
        bool hi = ps ? ps->digital : false;

        M5Cardputer.Display.setTextColor(C_TEXT);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setCursor(4, y + 2);
        M5Cardputer.Display.printf("[%d] GPIO%2d", i + 1, _pins[i]);

        drawState(88, y, hi);

        M5Cardputer.Display.fillRect(132, y + 2, 100, 10,
            hi ? 0x00aa44u : 0x330000u);
    }

    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.setCursor(4, SCR_H - 10);
    M5Cardputer.Display.print("[1-8] toggle  [A] all HIGH  [Z] all LOW");
}
