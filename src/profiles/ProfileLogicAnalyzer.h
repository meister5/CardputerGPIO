/**
 * ProfileLogicAnalyzer.h  (v2)
 *
 * Profile 8 – Logic Analyzer
 * Up to 4 user-configured input channels.
 */

#pragma once
#include "../Profile.h"

class ProfileLogicAnalyzer : public Profile {
public:
    const char* name() const override { return "Logic Analyzer"; }

    static const PinRole ROLES[];
    static const int     ROLE_COUNT;
    const PinRole* roles()     const override { return ROLES; }
    int            roleCount() const override { return ROLE_COUNT; }

    void onEnter(PinManager* pm, const PinConfig* cfg) override;
    void onExit (PinManager* pm) override;
    void update (PinManager* pm) override;
    void onKey  (PinManager* pm, char key) override;

private:
    static constexpr int CHANNELS = 4;
    static constexpr int SAMPLES  = 200;

    int     _pins[CHANNELS]              = {};
    uint8_t _buf[CHANNELS][SAMPLES]      = {};
    int     _sampleIdx                   = 0;
    bool    _bufFull                     = false;

    unsigned long _lastSampleUs          = 0;
    int           _intervalUs            = 200;
    int           _intervalIdx           = 3;

    static constexpr int INTERVAL_PRESETS[] = { 50, 100, 200, 500, 1000, 2000, 5000 };
    static constexpr int INTERVAL_COUNT     = 7;

    bool _dirty = true;

    void captureSample(PinManager* pm);
    void drawWaveforms();
};
