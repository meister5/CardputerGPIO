/**
 * ProfileI2CScanner.cpp  (v2 - fixed)
 */

#include "ProfileI2CScanner.h"

const PinRole ProfileI2CScanner::ROLES[] = {
    { "SDA", PinDir::EITHER, "-> I2C SDA (Grove yellow)" },
    { "SCL", PinDir::EITHER, "-> I2C SCL (Grove white)"  },
};
const int ProfileI2CScanner::ROLE_COUNT = 2;

void ProfileI2CScanner::onEnter(PinManager* pm, const PinConfig* cfg) {
    _sdaPin     = cfg->pin(0);
    _sclPin     = cfg->pin(1);
    _foundCount = 0;
    _dirty      = true;
    Wire.begin(_sdaPin, _sclPin, 100000);
    doScan();
}

void ProfileI2CScanner::onExit(PinManager* /*pm*/) {
    Wire.end();
}

void ProfileI2CScanner::update(PinManager* /*pm*/) {
    if (_dirty) { drawDisplay(); _dirty = false; }
}

void ProfileI2CScanner::onKey(PinManager* /*pm*/, char key) {
    if (key == 's' || key == 'S' || key == '\n' || key == '\r') {
        doScan(); _dirty = true;
    }
}

void ProfileI2CScanner::doScan() {
    _foundCount = 0;
    Serial.printf("[I2C] Scanning SDA=%d SCL=%d...\n", _sdaPin, _sclPin);
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0 && _foundCount < MAX_DEVICES)
            _found[_foundCount++] = addr;
        delay(2);
    }
    Serial.printf("[I2C] Found %d device(s).\n", _foundCount);
}

void ProfileI2CScanner::drawDisplay() {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(C_BG);
    drawHeader("I2C Scanner");

    M5Cardputer.Display.setTextColor(C_TEXT);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(4, 22);
    M5Cardputer.Display.printf("SDA=GPIO%d  SCL=GPIO%d", _sdaPin, _sclPin);

    if (_foundCount == 0) {
        M5Cardputer.Display.setTextColor(C_LOW);
        M5Cardputer.Display.setCursor(4, 40);
        M5Cardputer.Display.print("No devices found.");
    } else {
        M5Cardputer.Display.setTextColor(C_HIGH);
        M5Cardputer.Display.setCursor(4, 40);
        M5Cardputer.Display.printf("%d device(s):", _foundCount);

        int col = 0, row = 0, cellW = 48, cellH = 16;
        int startX = 4, startY = 56;
        for (int i = 0; i < _foundCount; i++) {
            int x = startX + col * cellW;
            int y = startY + row * cellH;
            M5Cardputer.Display.fillRect(x, y, cellW - 2, cellH - 2, C_HDR);
            M5Cardputer.Display.drawRect(x, y, cellW - 2, cellH - 2, C_BAR);
            M5Cardputer.Display.setTextColor(C_BAR);
            M5Cardputer.Display.setCursor(x + 4, y + 4);
            M5Cardputer.Display.printf("0x%02X", _found[i]);
            if (++col >= 4) { col = 0; row++; }
        }
    }

    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.setCursor(4, SCR_H - 10);
    M5Cardputer.Display.print("[S/ENT] rescan");
}
