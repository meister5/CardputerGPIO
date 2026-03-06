/**
 * ProfileDigitalIn.h  (v2)
 *
 * Profile 3 – Digital Input Monitor
 * Up to 6 user-configured input pins, live logic probe.
 */

#pragma once
#include "../Profile.h"

class ProfileDigitalIn : public Profile {
public:
    const char* name() const override { return "Digital Input Monitor"; }

    static const PinRole ROLES[];
    static const int     ROLE_COUNT;
    const PinRole* roles()     const override { return ROLES; }
    int            roleCount() const override { return ROLE_COUNT; }

    void onEnter(PinManager* pm, const PinConfig* cfg) override;
    void onExit (PinManager* pm) override;
    void update (PinManager* pm) override;
    void onKey  (PinManager* pm, char key) override;

private:
    static constexpr int MAX_PINS = 6;
    int           _pins[MAX_PINS]      = {};
    int           _pinCount            = 0;
    bool          _lastState[MAX_PINS] = {};
    int           _edgeCount[MAX_PINS] = {};
    unsigned long _lastDraw            = 0;

    void drawDisplay(PinManager* pm);
};
