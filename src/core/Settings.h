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

    // Seconds of no keyboard activity before the display powers down; 0 keeps
    // it on forever. Only the keys on the case count as activity -- driving
    // the board from a browser is precisely when the panel is wasted power.
    uint16_t screenOff() const { return _screenOff; }
    void     setScreenOff(uint16_t secs);

    // microSD shares G14/G39/G40 with the EXT header. Off by default so a
    // stray assignment cannot fight the card reader.
    bool allowSdPins() const { return _allowSd; }
    void setAllowSdPins(bool v);

    // Confirm before a tool drives outputs. On by default: this thing is
    // wired to real hardware.
    bool armOutputs() const { return _armOutputs; }
    void setArmOutputs(bool v);

    // ── WiFi / web interface ──────────────────────────────────────────────
    // 0 = off, 1 = access point, 2 = join a network. Named NET_* rather than
    // WIFI_* because the core's WiFiType.h defines those as macros.
    enum : uint8_t { NET_OFF = 0, NET_AP = 1, NET_STA = 2 };

    uint8_t wifiMode() const { return _wifiMode; }
    void    setWifiMode(uint8_t v);

    // Credentials for joining a network.
    const char* wifiSsid() const { return _ssid; }
    const char* wifiPass() const { return _pass; }
    void setWifiCreds(const char* ssid, const char* pass);

    // The access point's own password. Generated on first use rather than
    // derived from the MAC, which anyone in range can read off the SSID.
    const char* apPass() const { return _apPass; }
    void setApPass(const char* pass);

    // Optional password for the web interface. Empty means no login, which
    // is the right default on the device's own access point: that radio is
    // already WPA2. It matters on a shared network, where anything that can
    // route to the board would otherwise be able to drive its pins.
    const char* webPass() const { return _webPass; }
    void setWebPass(const char* pass);

    // Start the portal at boot rather than waiting to be asked.
    bool wifiAuto() const { return _wifiAuto; }
    void setWifiAuto(bool v);

    // ── Per-tool pin assignments ──────────────────────────────────────────
    int  pin(const char* toolId, int role, int fallback) const;
    void setPin(const char* toolId, int role, int gpio);
    void clearPins(const char* toolId, int roleCount);

    void factoryReset();

private:
    uint8_t  _brightness = 110;
    uint16_t _screenOff  = 30;
    bool    _beep       = true;
    bool    _allowSd    = false;
    bool    _armOutputs = true;
    uint8_t _wifiMode   = NET_OFF;
    bool    _wifiAuto   = false;
    char    _ssid[33]   = {};
    char    _pass[65]   = {};
    char    _apPass[17] = {};
    char    _webPass[33] = {};

    void putU8(const char* key, uint8_t v);
    void putU16(const char* key, uint16_t v);
    static void keyFor(const char* toolId, int role, char* out, int n);
};

extern Settings settings;

}  // namespace cg
