/**
 * ProfileDigitalOut.h  (v2 - fixed)
 *
 * Profile 2 – Digital Output Panel
 * Up to 8 user-configurable output pins, toggled by number keys.
 */

#pragma once
#include "../Profile.h"

class ProfileDigitalOut : public Profile {
public:
    const char* name() const override { return "Digital Output Panel"; }

    static const PinRole ROLES[];
    static const int     ROLE_COUNT;
    const PinRole* roles()     const override { return ROLES; }
    int            roleCount() const override { return ROLE_COUNT; }

    void onEnter(PinManager* pm, const PinConfig* cfg) override;
    void onExit (PinManager* pm) override;
    void update (PinManager* pm) override;
    void onKey  (PinManager* pm, char key) override;

private:
    static constexpr int MAX_PINS = 8;
    int  _pins[MAX_PINS] = {};
    int  _pinCount       = 0;
    bool _dirty          = true;

    void drawDisplay(PinManager* pm);   // FIX: needs pm to read pin state
};
