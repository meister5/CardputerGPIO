/**
 * MenuSystem.h  (v2 - fixed)
 *
 * Menu & UI System
 * ────────────────
 * Owns the full profile launch flow:
 *
 *   MENU
 *     → [ENTER]
 *     → WIRING_GUIDE  (shows GPIO table; user wires hardware)
 *       → [ENT]  → RUNNING     (profile active)
 *       → [C]    → CONFIGURING (PinConfigurator, then back to WIRING_GUIDE)
 *       → [ESC]  → MENU
 *     → RUNNING
 *       → [ESC]  → MENU  (profile released)
 *
 * Profiles receive a populated PinConfig* in onEnter() so they never
 * have to guess which pins to use.
 */

#pragma once
#include <M5Cardputer.h>   // FIX: was M5Unified.h — Cardputer keyboard requires this
#include "PinManager.h"
#include "Profile.h"
#include "PinConfig.h"
#include "PinConfigurator.h"
#include "WiringGuide.h"

static constexpr int MAX_PROFILES = 16;

class MenuSystem {
public:
    void init(PinManager* pm);

    // Register a profile.
    //   p         – profile instance
    //   profileId – short unique ASCII id for NVS key prefix, e.g. "ic", "dout"
    void addProfile(Profile* p, const char* profileId);

    void showMainMenu();
    void update();

private:
    enum class State { MENU, WIRING_GUIDE, CONFIGURING, RUNNING };
    State       _state        = State::MENU;

    PinManager* _pm           = nullptr;
    Profile*    _profiles[MAX_PROFILES]    = {};
    PinConfig   _configs[MAX_PROFILES];
    char        _profileIds[MAX_PROFILES][16] = {};
    int         _profileCount = 0;

    int         _cursor    = 0;
    int         _scrollOff = 0;
    static constexpr int VISIBLE_ROWS = 6;

    int         _activeIdx = -1;

    PinConfigurator _configurator;
    WiringGuide     _wiringGuide;

    void renderMenu();
    void handleMenuKey(char key);
    void launchWiringGuide(int idx);
    void launchConfigurator(int idx);
    void startProfile(int idx);
    void returnToMenu();
    char pollKey();   // returns 0 if no key, else ASCII char

    static constexpr int    SCR_W  = 240;
    static constexpr int    SCR_H  = 135;
    static constexpr uint32_t C_BG      = 0x000000u;
    static constexpr uint32_t C_HDR     = 0x1a3a5cu;
    static constexpr uint32_t C_TITLE   = 0xffd700u;
    static constexpr uint32_t C_SEL     = 0x003366u;
    static constexpr uint32_t C_TEXT    = 0xd0d0d0u;
    static constexpr uint32_t C_CURSOR  = 0x00ff88u;
    static constexpr uint32_t C_HINT    = 0x808080u;
};
