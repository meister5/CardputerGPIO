/**
 * ProfilePWMGen.h  (v2)
 *
 * Profile 5 – PWM Generator
 * User configures which GPIO to output PWM on.
 */

#pragma once
#include "../Profile.h"

class ProfilePWMGen : public Profile {
public:
    const char* name() const override { return "PWM Generator"; }

    static const PinRole ROLES[];
    static const int     ROLE_COUNT;
    const PinRole* roles()     const override { return ROLES; }
    int            roleCount() const override { return ROLE_COUNT; }

    void onEnter(PinManager* pm, const PinConfig* cfg) override;
    void onExit (PinManager* pm) override;
    void update (PinManager* pm) override;
    void onKey  (PinManager* pm, char key) override;

private:
    int  _pin      = -1;
    int  _freq     = 1000;
    int  _duty     = 50;
    bool _active   = false;
    bool _dirty    = true;

    static constexpr int FREQ_STEPS[] = {
        1, 10, 50, 100, 500, 1000, 2000, 5000,
        10000, 20000, 50000, 100000, 500000, 1000000
    };
    static constexpr int FREQ_STEP_COUNT = 14;
    int  _freqIdx = 5;

    void applyPWM(PinManager* pm);
    void drawDisplay();
    void drawWaveform(int x, int y, int w, int h, int dutyPct);
    const char* formatFreq(int hz, char* buf, int len);
};
