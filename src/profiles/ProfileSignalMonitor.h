/**
 * ProfileSignalMonitor.h  (v2)
 *
 * Profile 6 – Signal Monitor
 * One user-configured input pin; detects edges and estimates frequency.
 */

#pragma once
#include "../Profile.h"

class ProfileSignalMonitor : public Profile {
public:
    const char* name() const override { return "Signal Monitor"; }

    static const PinRole ROLES[];
    static const int     ROLE_COUNT;
    const PinRole* roles()     const override { return ROLES; }
    int            roleCount() const override { return ROLE_COUNT; }

    void onEnter(PinManager* pm, const PinConfig* cfg) override;
    void onExit (PinManager* pm) override;
    void update (PinManager* pm) override;
    void onKey  (PinManager* pm, char key) override;

private:
    int           _pin          = -1;
    bool          _lastState    = false;
    unsigned long _lastRisingUs = 0;
    unsigned long _periodUs     = 0;
    int           _edgeCount    = 0;
    unsigned long _lastDraw     = 0;

    void drawDisplay();
};
