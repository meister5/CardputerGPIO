/**
 * Settings.h — everything that survives a power cycle.
 *
 * Two namespaces in NVS:
 *   "cgpio"  global preferences (brightness, beeper, SD-pin opt-in, ...)
 *   "cgpins" per-tool pin assignments, keyed "<toolId>_<role>"
 *
 * NVS keys are capped at 15 characters, which is why tool ids are limited to
 * 8. Tool ids are persistence keys: renaming one silently orphans the user's
 * saved pin assignments, so they never change once shipped.
 */

#pragma once
#include <stdint.h>

namespace cg {

class Settings {
public:
    void begin();

    uint8_t brightness() const { return _brightness; }
    void    setBrightness(uint8_t v);

    bool beep() const { return _beep; }
    void setBeep(bool v);

    // microSD shares G14/G39/G40 with the EXT header. Off by default so a
    // stray assignment cannot fight the card reader.
    bool allowSdPins() const { return _allowSd; }
    void setAllowSdPins(bool v);

    // Confirm before a tool drives outputs. On by default: this thing is
    // wired to real hardware.
    bool armOutputs() const { return _armOutputs; }
    void setArmOutputs(bool v);

    // ── Per-tool pin assignments ──────────────────────────────────────────
    int  pin(const char* toolId, int role, int fallback) const;
    void setPin(const char* toolId, int role, int gpio);
    void clearPins(const char* toolId, int roleCount);

    void factoryReset();

private:
    uint8_t _brightness = 110;
    bool    _beep       = true;
    bool    _allowSd    = false;
    bool    _armOutputs = true;

    void putU8(const char* key, uint8_t v);
    static void keyFor(const char* toolId, int role, char* out, int n);
};

extern Settings settings;

}  // namespace cg
