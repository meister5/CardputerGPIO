/**
 * ProfileICControl.h  (v2)
 *
 * Profile 1 – IC Control Mode
 */

#pragma once
#include "../Profile.h"

enum class ICType {
    MUX_1OF8,
    DECODER_3TO8,
    SHIFT_REG,
    COUNTER,
    IC_TYPE_COUNT
};

class ProfileICControl : public Profile {
public:
    const char* name() const override { return "IC Control Mode"; }

    static const PinRole ROLES[];
    static const int     ROLE_COUNT;
    const PinRole* roles()     const override { return ROLES; }
    int            roleCount() const override { return ROLE_COUNT; }

    void onEnter(PinManager* pm, const PinConfig* cfg) override;
    void onExit (PinManager* pm) override;
    void update (PinManager* pm) override;
    void onKey  (PinManager* pm, char key) override;

private:
    ICType  _icType      = ICType::MUX_1OF8;
    int     _bits        = 3;
    bool    _pinState[3] = {};
    int     _usedPins[3] = {};
    bool    _dirty       = true;

    void    applyToHardware(PinManager* pm);
    void    drawDisplay();
    const char* icTypeName() const;
    int     binaryValue() const;
};
