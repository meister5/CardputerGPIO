/**
 * ProfileI2CScanner.h  (v2)
 *
 * Profile 7 – I2C Scanner
 * User configures SDA and SCL pins (defaults to Grove port).
 */

#pragma once
#include "../Profile.h"
#include <Wire.h>

class ProfileI2CScanner : public Profile {
public:
    const char* name() const override { return "I2C Scanner"; }

    static const PinRole ROLES[];
    static const int     ROLE_COUNT;
    const PinRole* roles()     const override { return ROLES; }
    int            roleCount() const override { return ROLE_COUNT; }

    void onEnter(PinManager* pm, const PinConfig* cfg) override;
    void onExit (PinManager* pm) override;
    void update (PinManager* pm) override;
    void onKey  (PinManager* pm, char key) override;

private:
    static constexpr int MAX_DEVICES = 32;
    uint8_t _found[MAX_DEVICES] = {};
    int     _foundCount  = 0;
    int     _sdaPin      = GROVE_SDA;
    int     _sclPin      = GROVE_SCL;
    bool    _dirty       = true;

    void doScan();
    void drawDisplay();
};
