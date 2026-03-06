/**
 * PinConfig.h
 */

#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "PinManager.h"

enum class PinDir { OUTPUT_ROLE, INPUT_ROLE, ADC_ROLE, PWM_ROLE, EITHER };

struct PinRole {
    const char* label;
    PinDir      dir;
    const char* hint;
};

struct PinAssignment {
    int gpio = -1;
};

class PinConfig {
public:
    static constexpr int MAX_ROLES = 8;

    void load(const char* profileId, const PinRole* roles, int roleCount,
              const PinManager* pm);
    void save(const char* profileId) const;
    void clear(const char* profileId);

    bool isComplete() const;

    int         pin(int roleIndex) const;
    void        setPin(int roleIndex, int gpio);
    int         roleCount()            const { return _roleCount; }
    const PinRole& role(int i)         const { return _roles[i]; }
    const char* profileId()            const { return _profileId; }

private:
    char           _profileId[16] = {};
    const PinRole* _roles         = nullptr;
    int            _roleCount     = 0;
    PinAssignment  _assignments[MAX_ROLES];

    static constexpr const char* NVS_NS = "gpio_lab";
    void makeKey(int i, char* buf, int len) const;
};
