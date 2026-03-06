/**
 * ProfileAnalogReader.h  (v2)
 *
 * Profile 4 – Analog Sensor Reader
 * Up to 4 user-configured ADC pins.
 */

#pragma once
#include "../Profile.h"

class ProfileAnalogReader : public Profile {
public:
    const char* name() const override { return "Analog Sensor Reader"; }

    static const PinRole ROLES[];
    static const int     ROLE_COUNT;
    const PinRole* roles()     const override { return ROLES; }
    int            roleCount() const override { return ROLE_COUNT; }

    void onEnter(PinManager* pm, const PinConfig* cfg) override;
    void onExit (PinManager* pm) override;
    void update (PinManager* pm) override;
    void onKey  (PinManager* pm, char key) override;

private:
    static constexpr int MAX_PINS = 4;
    int           _pins[MAX_PINS] = {};
    int           _pinCount       = 0;
    unsigned long _lastDraw       = 0;

    void drawDisplay(PinManager* pm);
    void drawChannel(int x, int y, int pin, int raw, float volts);
};
