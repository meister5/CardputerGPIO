/**
 * Profile.h  (v2 - fixed)
 */

#pragma once
#include <M5Cardputer.h>
#include "PinManager.h"
#include "PinConfig.h"

class Profile {
public:
    virtual ~Profile() = default;

    virtual const PinRole* roles()     const = 0;
    virtual int            roleCount() const = 0;

    virtual void onEnter(PinManager* pm, const PinConfig* cfg) = 0;
    virtual void onExit(PinManager* pm)  = 0;
    virtual void update(PinManager* pm)  = 0;
    virtual void onKey(PinManager* pm, char key) = 0;
    virtual const char* name() const = 0;

protected:
    static constexpr int SCR_W  = 240;
    static constexpr int SCR_H  = 135;
    static constexpr int HDR_H  = 18;
    static constexpr uint32_t C_BG     = 0x000000u;
    static constexpr uint32_t C_HDR    = 0x1a3a5cu;
    static constexpr uint32_t C_TITLE  = 0xffd700u;
    static constexpr uint32_t C_HIGH   = 0x00ff88u;
    static constexpr uint32_t C_LOW    = 0xff4444u;
    static constexpr uint32_t C_TEXT   = 0xd0d0d0u;
    static constexpr uint32_t C_DIM    = 0x606060u;
    static constexpr uint32_t C_BAR    = 0x00bfffu;
    static constexpr uint32_t C_WAVE_H = 0x00ff88u;
    static constexpr uint32_t C_WAVE_L = 0x303030u;

    // FIX: always reset font + size before drawing so text is never invisible.
    // M5GFX requires a font to be active or print() outputs nothing.
    void resetTextStyle() {
        M5Cardputer.Display.setTextSize(1);
    }

    void drawHeader(const char* title) {
        resetTextStyle();   // FIX: ensure font is set before any text
        M5Cardputer.Display.fillRect(0, 0, SCR_W, HDR_H, C_HDR);
        M5Cardputer.Display.setTextColor(C_TITLE);
        M5Cardputer.Display.setCursor(4, 4);
        M5Cardputer.Display.print(title);
    }

    void drawBarGraph(int x, int y, int w, int h, int pct, uint32_t color = C_BAR) {
        pct = constrain(pct, 0, 100);
        int filled = (int)((pct / 100.0f) * w);
        M5Cardputer.Display.fillRect(x,          y, filled,     h, color);
        M5Cardputer.Display.fillRect(x + filled, y, w - filled, h, C_DIM);
        M5Cardputer.Display.drawRect(x - 1, y - 1, w + 2, h + 2, C_TEXT);
    }

    void drawState(int x, int y, bool high) {
        uint32_t col    = high ? C_HIGH : C_LOW;
        const char* lbl = high ? "HIGH" : "LOW ";
        M5Cardputer.Display.fillRoundRect(x, y, 36, 12, 3, col);
        M5Cardputer.Display.setTextColor(0x000000u);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setCursor(x + 3, y + 2);
        M5Cardputer.Display.print(lbl);
    }
};
