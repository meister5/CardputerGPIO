/**
 * WiringGuide.h  (fixed)
 *
 * Wiring Guide Screen
 * ───────────────────
 * Shown every time a profile is about to start, after pin assignment
 * is confirmed. Gives the user a clear "plug this in before pressing
 * ENTER" reference so they know exactly which physical pins to use.
 *
 * Screen layout (example: IC Control, 3 roles):
 * ┌──────────────────────────────────────┐
 * │  IC Control Mode — Wiring Guide      │
 * ├──────────────────────────────────────┤
 * │                                      │
 * │  A0 (addr)   GPIO 36   → IC pin 11  │
 * │  A1 (addr)   GPIO 37   → IC pin 10  │
 * │  A2 (addr)   GPIO 38   → IC pin 9   │
 * │                                      │
 * │  [ENT] Start   [C] Configure   [DEL] Back │
 * └──────────────────────────────────────┘
 *
 * Returns one of three outcomes:
 *   START      – user pressed ENTER → profile should run
 *   CONFIGURE  – user pressed C     → open PinConfigurator
 *   BACK       – user pressed DEL   → return to main menu
 */

#pragma once
#include <M5Cardputer.h>   // FIX: was M5Unified.h
#include "PinConfig.h"

enum class WiringGuideResult { PENDING, START, CONFIGURE, BACK };

class WiringGuide {
public:
    void begin(const PinConfig* cfg, const char* profileName);
    void update();
    void onKey(char key);

    WiringGuideResult result() const { return _result; }
    void reset() { _result = WiringGuideResult::PENDING; _drawn = false; }

private:
    const PinConfig* _cfg         = nullptr;
    char             _profileName[32] = {};
    WiringGuideResult _result     = WiringGuideResult::PENDING;
    bool             _drawn       = false;

    void draw();

    static constexpr int    SCR_W  = 240;
    static constexpr int    SCR_H  = 135;
    static constexpr uint32_t C_BG     = 0x000000u;
    static constexpr uint32_t C_HDR    = 0x1a3a5cu;
    static constexpr uint32_t C_TITLE  = 0xffd700u;
    static constexpr uint32_t C_TEXT   = 0xd0d0d0u;
    static constexpr uint32_t C_DIM    = 0x606060u;
    static constexpr uint32_t C_GPIO   = 0x00ff88u;
    static constexpr uint32_t C_HINT   = 0x00bfffu;
    static constexpr uint32_t C_OUT    = 0x00aa44u;
    static constexpr uint32_t C_IN     = 0x0077ffu;
    static constexpr uint32_t C_ADC    = 0xffaa00u;
    static constexpr uint32_t C_PWM    = 0xaa00ffu;
};
