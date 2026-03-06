/**
 * ProfileICControl.cpp  (v2 - fixed)
 */

#include "ProfileICControl.h"

const PinRole ProfileICControl::ROLES[] = {
    { "A0 (addr bit 0)", PinDir::OUTPUT_ROLE, "-> IC pin A0" },
    { "A1 (addr bit 1)", PinDir::OUTPUT_ROLE, "-> IC pin A1" },
    { "A2 (addr bit 2)", PinDir::OUTPUT_ROLE, "-> IC pin A2" },
};
const int ProfileICControl::ROLE_COUNT = 3;

void ProfileICControl::onEnter(PinManager* pm, const PinConfig* cfg) {
    _bits = ROLE_COUNT;
    memset(_pinState, 0, sizeof(_pinState));

    for (int i = 0; i < _bits; i++) {
        _usedPins[i] = cfg->pin(i);
        pm->configureOutput(_usedPins[i]);
    }
    applyToHardware(pm);
    _dirty = true;
}

void ProfileICControl::onExit(PinManager* pm) {
    for (int i = 0; i < _bits; i++) pm->releasePin(_usedPins[i]);
}

void ProfileICControl::applyToHardware(PinManager* pm) {
    for (int i = 0; i < _bits; i++) pm->writePin(_usedPins[i], _pinState[i]);
}

void ProfileICControl::update(PinManager* pm) {
    if (_dirty) { drawDisplay(); _dirty = false; }
}

void ProfileICControl::onKey(PinManager* pm, char key) {
    if (key >= '0' && key < ('0' + _bits)) {
        int idx = key - '0';
        _pinState[idx] = !_pinState[idx];
        applyToHardware(pm);
        _dirty = true;
    } else if (key == '+') {
        int v = (binaryValue() + 1) & ((1 << _bits) - 1);
        for (int i = 0; i < _bits; i++) _pinState[i] = (v >> i) & 1;
        applyToHardware(pm);
        _dirty = true;
    } else if (key == '-') {
        int v = (binaryValue() - 1) & ((1 << _bits) - 1);
        for (int i = 0; i < _bits; i++) _pinState[i] = (v >> i) & 1;
        applyToHardware(pm);
        _dirty = true;
    } else if (key == '\n' || key == '\r') {
        _icType = (ICType)(((int)_icType + 1) % (int)ICType::IC_TYPE_COUNT);
        _dirty  = true;
    }
}

void ProfileICControl::drawDisplay() {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(C_BG);
    drawHeader("IC Control Mode");

    int val     = binaryValue();
    int outputs = 1 << _bits;

    M5Cardputer.Display.setTextColor(C_TITLE);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(4, 22);
    M5Cardputer.Display.printf("IC: %s", icTypeName());

    M5Cardputer.Display.setCursor(4, 36);
    for (int i = _bits - 1; i >= 0; i--) {
        M5Cardputer.Display.setTextColor(_pinState[i] ? (uint32_t)C_HIGH : (uint32_t)C_LOW);
        M5Cardputer.Display.printf("A%d(G%d):%s ", i, _usedPins[i], _pinState[i] ? "1" : "0");
    }

    M5Cardputer.Display.setTextColor(C_TITLE);
    M5Cardputer.Display.setCursor(4, 52);
    M5Cardputer.Display.printf("Address: %d  (0b", val);
    for (int i = _bits - 1; i >= 0; i--) M5Cardputer.Display.print(_pinState[i] ? '1' : '0');
    M5Cardputer.Display.print(")");

    M5Cardputer.Display.setTextColor(C_TEXT);
    M5Cardputer.Display.setCursor(4, 66);
    M5Cardputer.Display.print("Active output: ");
    M5Cardputer.Display.setTextColor(C_HIGH);
    M5Cardputer.Display.printf("Y%d", val);

    int gridX = 4, gridY = 82, cellW = 22, cellH = 14;
    for (int i = 0; i < min(outputs, 8); i++) {
        int cx = gridX + (i % 8) * cellW;
        int cy = gridY;
        bool active = (i == val);
        M5Cardputer.Display.fillRect(cx, cy, cellW - 2, cellH, active ? (uint32_t)C_HIGH : (uint32_t)C_DIM);
        M5Cardputer.Display.setTextColor(active ? 0x000000u : (uint32_t)C_TEXT);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setCursor(cx + 3, cy + 3);
        M5Cardputer.Display.printf("Y%d", i);
    }

    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.setCursor(4, SCR_H - 10);
    M5Cardputer.Display.print("[0-2] toggle  [+/-] inc/dec  [ENT] IC type");
}

int ProfileICControl::binaryValue() const {
    int v = 0;
    for (int i = 0; i < _bits; i++) if (_pinState[i]) v |= (1 << i);
    return v;
}

const char* ProfileICControl::icTypeName() const {
    switch (_icType) {
        case ICType::MUX_1OF8:      return "1-of-8 MUX (74151)";
        case ICType::DECODER_3TO8:  return "3-to-8 Decoder (74138)";
        case ICType::SHIFT_REG:     return "Shift Reg (74595)";
        case ICType::COUNTER:       return "Binary Counter";
        default:                    return "Unknown";
    }
}
