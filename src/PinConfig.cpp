/**
 * PinConfig.cpp
 */

#include "PinConfig.h"

void PinConfig::load(const char* profileId, const PinRole* roles, int roleCount,
                     const PinManager* pm)
{
    strncpy(_profileId, profileId, sizeof(_profileId) - 1);
    _roles     = roles;
    _roleCount = min(roleCount, MAX_ROLES);

    Preferences prefs;
    prefs.begin(NVS_NS, true);

    const auto& safePins = pm->safePins();
    const auto& adcPins  = pm->adcPins();

    for (int i = 0; i < _roleCount; i++) {
        char key[20];
        makeKey(i, key, sizeof(key));
        if (prefs.isKey(key)) {
            _assignments[i].gpio = prefs.getInt(key, -1);
        } else {
            const auto& pool = (roles[i].dir == PinDir::ADC_ROLE) ? adcPins : safePins;
            _assignments[i].gpio = (i < (int)pool.size()) ? pool[i] : -1;
        }
    }
    prefs.end();
}

void PinConfig::save(const char* profileId) const {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    for (int i = 0; i < _roleCount; i++) {
        char key[20];
        makeKey(i, key, sizeof(key));
        prefs.putInt(key, _assignments[i].gpio);
    }
    prefs.end();
    Serial.printf("[PinConfig] Saved %d assignments for '%s'\n", _roleCount, profileId);
}

void PinConfig::clear(const char* profileId) {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    for (int i = 0; i < _roleCount; i++) {
        char key[20];
        makeKey(i, key, sizeof(key));
        prefs.remove(key);
        _assignments[i].gpio = -1;
    }
    prefs.end();
    Serial.printf("[PinConfig] Cleared assignments for '%s'\n", profileId);
}

bool PinConfig::isComplete() const {
    for (int i = 0; i < _roleCount; i++)
        if (_assignments[i].gpio < 0) return false;
    return true;
}

int PinConfig::pin(int i) const {
    if (i < 0 || i >= _roleCount) return -1;
    return _assignments[i].gpio;
}

void PinConfig::setPin(int i, int gpio) {
    if (i >= 0 && i < _roleCount) _assignments[i].gpio = gpio;
}

void PinConfig::makeKey(int i, char* buf, int len) const {
    snprintf(buf, len, "%s_%d", _profileId, i);
}
