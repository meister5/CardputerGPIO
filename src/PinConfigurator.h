/**
 * PinConfigurator.h  (fixed)
 *
 * Interactive Pin Assignment UI
 * ─────────────────────────────
 * A reusable full-screen widget that lets the user scroll through each
 * pin role and pick a GPIO from the safe-pin list.
 *
 * Screen layout (example: IC Control, role 0 of 3):
 * ┌──────────────────────────────────────┐
 * │  Configure: IC Control Mode          │  ← header
 * │  Role 1 / 3:  A0 (addr)             │  ← role name
 * │  → IC pin 11 (A0)                   │  ← hint
 * │                                      │
 * │  Available pins:                     │
 * │    36  37  38  39  40  41  42  43   │  ← safe pin strip
 * │        ^^^                           │  ← cursor under selected
 * │  [ GPIO 37 ]                         │  ← big selected value
 * │                                      │
 * │  [Fn+,/Fn+/] choose   [ENT] confirm │
 * │  [DEL] cancel                        │
 * └──────────────────────────────────────┘
 *
 * For ADC roles, only ADC-capable pins are offered.
 *
 * Keyboard sentinels (defined in MenuSystem.h or a shared header):
 *   KEY_UP    0x11  (Fn+;)
 *   KEY_DOWN  0x12  (Fn+.)
 *   KEY_LEFT  0x13  (Fn+,)
 *   KEY_RIGHT 0x14  (Fn+/)
 *
 * Usage
 * ─────
 *   PinConfigurator cfg;
 *   cfg.begin(&pinConfig, &pinManager, "IC Control Mode");
 *
 *   // In your update loop (after M5Cardputer.update()):
 *   cfg.update();
 *   cfg.onKey(key);          // pass key from MenuSystem::pollKey()
 *
 *   if (cfg.isDone())        { /* assignments saved *\/ }
 *   if (cfg.isCancelled())   { /* no changes *\/ }
 */

#pragma once
#include <M5Cardputer.h>   // FIX: was M5Unified.h
#include <vector>
#include "PinConfig.h"
#include "PinManager.h"

// ── Navigation sentinel bytes (must match MenuSystem::pollKey) ────────────
static constexpr char KEY_UP    = 0x11;
static constexpr char KEY_DOWN  = 0x12;
static constexpr char KEY_LEFT  = 0x13;
static constexpr char KEY_RIGHT = 0x14;

class PinConfigurator {
public:
    // Kick off a configuration session.
    // startRole: which role to start from (0 = first)
    void begin(PinConfig* cfg, const PinManager* pm,
               const char* profileName, int startRole = 0);

    void   update();
    void   onKey(char key);

    bool   isDone()      const { return _done;      }
    bool   isCancelled() const { return _cancelled; }

    // Reset state so begin() can be called again
    void   reset()             { _done = _cancelled = false; }

private:
    PinConfig*        _cfg         = nullptr;
    const PinManager* _pm          = nullptr;
    char              _profileName[32] = {};

    int               _currentRole = 0;
    int               _pinCursor   = 0;
    std::vector<int>  _candidates;

    bool              _done        = false;
    bool              _cancelled   = false;
    bool              _dirty       = true;

    void  buildCandidates();
    void  draw();
    void  confirmRole();

    // Display constants
    static constexpr int    SCR_W  = 240;
    static constexpr int    SCR_H  = 135;
    static constexpr uint32_t C_BG     = 0x000000u;
    static constexpr uint32_t C_HDR    = 0x1a3a5cu;
    static constexpr uint32_t C_TITLE  = 0xffd700u;
    static constexpr uint32_t C_TEXT   = 0xd0d0d0u;
    static constexpr uint32_t C_DIM    = 0x606060u;
    static constexpr uint32_t C_SEL    = 0x00ff88u;
    static constexpr uint32_t C_HINT   = 0x00bfffu;
    static constexpr uint32_t C_WARN   = 0xff8800u;
};
