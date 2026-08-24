#include "Settings.h"
#include "Board.h"
#include <M5Cardputer.h>
#include <Preferences.h>
#include <stdio.h>

namespace cg {

Settings settings;

static constexpr const char* NS_CFG  = "cgpio";
static constexpr const char* NS_PINS = "cgpins";

void Settings::begin() {
    Preferences p;
    if (p.begin(NS_CFG, true)) {
        _brightness = p.getUChar("bright",  110);
        _beep       = p.getUChar("beep",      1) != 0;
        _allowSd    = p.getUChar("sdpins",    0) != 0;
        _armOutputs = p.getUChar("armout",    1) != 0;
        _wifiMode   = p.getUChar("wifimode",  NET_OFF);
        _wifiAuto   = p.getUChar("wifiauto",  0) != 0;
        p.getString("wifissid", _ssid, sizeof(_ssid));
        p.getString("wifipass", _pass, sizeof(_pass));
        p.getString("appass",   _apPass, sizeof(_apPass));
        p.end();
    }
    if (_wifiMode > NET_STA) _wifiMode = NET_OFF;
    if (_brightness < 10) _brightness = 10;
    M5Cardputer.Display.setBrightness(_brightness);
}

void Settings::putU8(const char* key, uint8_t v) {
    Preferences p;
    if (!p.begin(NS_CFG, false)) return;
    p.putUChar(key, v);
    p.end();
}

void Settings::setBrightness(uint8_t v) {
    if (v < 10)  v = 10;
    if (v > 255) v = 255;
    _brightness = v;
    M5Cardputer.Display.setBrightness(v);
    putU8("bright", v);
}

void Settings::setBeep(bool v)       { _beep = v;       putU8("beep",   v ? 1 : 0); }
void Settings::setArmOutputs(bool v) { _armOutputs = v; putU8("armout", v ? 1 : 0); }

void Settings::setWifiMode(uint8_t v) {
    if (v > NET_STA) v = NET_OFF;
    _wifiMode = v;
    putU8("wifimode", v);
}

void Settings::setWifiAuto(bool v) { _wifiAuto = v; putU8("wifiauto", v ? 1 : 0); }

void Settings::setWifiCreds(const char* ssid, const char* pass) {
    snprintf(_ssid, sizeof(_ssid), "%s", ssid ? ssid : "");
    snprintf(_pass, sizeof(_pass), "%s", pass ? pass : "");
    Preferences p;
    if (!p.begin(NS_CFG, false)) return;
    p.putString("wifissid", _ssid);
    p.putString("wifipass", _pass);
    p.end();
}

void Settings::setApPass(const char* pass) {
    snprintf(_apPass, sizeof(_apPass), "%s", pass ? pass : "");
    Preferences p;
    if (!p.begin(NS_CFG, false)) return;
    p.putString("appass", _apPass);
    p.end();
}

void Settings::setAllowSdPins(bool v) {
    _allowSd = v;
    putU8("sdpins", v ? 1 : 0);
    poolsRebuild();   // the usable-pin pools depend on this
}

void Settings::keyFor(const char* toolId, int role, char* out, int n) {
    snprintf(out, n, "%.8s_%d", toolId, role);
}

int Settings::pin(const char* toolId, int role, int fallback) const {
    char key[16];
    keyFor(toolId, role, key, sizeof(key));

    Preferences p;
    if (!p.begin(NS_PINS, true)) return fallback;
    int v = p.isKey(key) ? p.getInt(key, fallback) : fallback;
    p.end();

    // A stored pin can become invalid when the SD opt-in is turned back off,
    // or when a config predates a firmware that changed the pin map.
    if (!pinGpioOk(v)) return fallback;
    return v;
}

void Settings::setPin(const char* toolId, int role, int gpio) {
    char key[16];
    keyFor(toolId, role, key, sizeof(key));

    Preferences p;
    if (!p.begin(NS_PINS, false)) return;
    p.putInt(key, gpio);
    p.end();
}

void Settings::clearPins(const char* toolId, int roleCount) {
    Preferences p;
    if (!p.begin(NS_PINS, false)) return;
    for (int i = 0; i < roleCount; i++) {
        char key[16];
        keyFor(toolId, i, key, sizeof(key));
        p.remove(key);
    }
    p.end();
}

void Settings::factoryReset() {
    Preferences p;
    if (p.begin(NS_PINS, false)) { p.clear(); p.end(); }
    if (p.begin(NS_CFG,  false)) { p.clear(); p.end(); }
    _brightness = 110;
    _beep       = true;
    _allowSd    = false;
    _armOutputs = true;
    M5Cardputer.Display.setBrightness(_brightness);
    poolsRebuild();
}

}  // namespace cg
