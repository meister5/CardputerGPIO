/**
 * WiringGuide.cpp  (fixed)
 *
 * Fixes:
 *  - ESC (key==27) replaced with DEL (key==8) — Cardputer has no ESC key,
 *    DEL/backspace is the standard "back" action.
 *  - All M5.Display → M5Cardputer.Display
 *  - Footer updated to reflect actual Cardputer key bindings
 */

#include "WiringGuide.h"

void WiringGuide::begin(const PinConfig* cfg, const char* profileName) {
    _cfg    = cfg;
    strncpy(_profileName, profileName, sizeof(_profileName) - 1);
    _result = WiringGuideResult::PENDING;
    _drawn  = false;
}

void WiringGuide::update() {
    if (!_drawn) { draw(); _drawn = true; }
}

void WiringGuide::onKey(char key) {
    if (key == '\n' || key == '\r' || key == ' ') {
        _result = WiringGuideResult::START;
    } else if (key == 'c' || key == 'C') {
        _result = WiringGuideResult::CONFIGURE;
    } else if (key == 8) {   // FIX: DEL/backspace = back (was ESC==27, not on Cardputer)
        _result = WiringGuideResult::BACK;
    }
}

void WiringGuide::draw() {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(C_BG);

    // ── Header ────────────────────────────────────────────────────────
    M5Cardputer.Display.fillRect(0, 0, SCR_W, 18, C_HDR);
    M5Cardputer.Display.setTextColor(C_TITLE);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(4, 4);
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "%.20s — Wiring", _profileName);
    M5Cardputer.Display.print(hdr);

    // ── Pin table ─────────────────────────────────────────────────────
    int roleCount = _cfg->roleCount();
    int rowH      = 16;
    int startY    = 22;
    int maxRows   = (SCR_H - startY - 16) / rowH;

    for (int i = 0; i < min(roleCount, maxRows); i++) {
        const PinRole& role = _cfg->role(i);
        int            gpio = _cfg->pin(i);
        int            y    = startY + i * rowH;

        // Direction colour dot
        uint32_t dCol = C_DIM;
        switch (role.dir) {
            case PinDir::OUTPUT_ROLE: dCol = C_OUT;  break;
            case PinDir::INPUT_ROLE:  dCol = C_IN;   break;
            case PinDir::ADC_ROLE:    dCol = C_ADC;  break;
            case PinDir::PWM_ROLE:    dCol = C_PWM;  break;
            default: break;
        }
        M5Cardputer.Display.fillCircle(4, y + 6, 3, dCol);

        // Role label
        M5Cardputer.Display.setTextColor(C_TEXT);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setCursor(12, y + 2);
        char roleLabel[13];
        snprintf(roleLabel, sizeof(roleLabel), "%-12s", role.label);
        M5Cardputer.Display.print(roleLabel);

        // GPIO number (highlighted)
        M5Cardputer.Display.setTextColor(C_GPIO);
        M5Cardputer.Display.setCursor(100, y + 2);
        if (gpio >= 0)
            M5Cardputer.Display.printf("GPIO %2d", gpio);
        else
            M5Cardputer.Display.print("GPIO ???");

        // Connection hint
        M5Cardputer.Display.setTextColor(C_HINT);
        M5Cardputer.Display.setCursor(148, y + 2);
        M5Cardputer.Display.print(role.hint);
    }

    // If there are more roles than fit, show ellipsis
    if (roleCount > maxRows) {
        M5Cardputer.Display.setTextColor(C_DIM);
        M5Cardputer.Display.setCursor(4, startY + maxRows * rowH);
        M5Cardputer.Display.printf("  (+%d more — see config)", roleCount - maxRows);
    }

    // ── Footer ────────────────────────────────────────────────────────
    M5Cardputer.Display.fillRect(0, SCR_H - 14, SCR_W, 14, 0x0a0a0au);
    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(4, SCR_H - 10);
    // FIX: updated footer — DEL = Back (not ESC which doesn't exist on Cardputer)
    M5Cardputer.Display.print("[ENT] Start   [C] Reconfigure   [DEL] Back");
}
