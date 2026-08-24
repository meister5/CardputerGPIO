/**
 * ToolAnalogIn.h — four-channel analog input.
 *
 * Readings come from analogReadMilliVolts(), which applies the ADC
 * calibration burned into the chip's eFuse. The v1 tool computed
 * (raw / 4095) * 3.3, which ignores both the calibration and the fact that
 * the 12 dB attenuator is distinctly non-linear near the rails -- good for
 * a couple of hundred millivolts of error at the top of the range.
 *
 * Divider presets let you read past 3.3 V with an external resistor pair
 * without doing the arithmetic in your head.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolAnalogIn : public Tool {
public:
    const char* id()    const override { return "adc"; }
    const char* name()  const override { return "Analog In"; }
    const char* blurb() const override { return "4ch ADC, calibrated mV"; }
    Cat         cat()   const override { return Cat::Analog; }

    const Role* roles()     const override;
    int         roleCount() const override { return 4; }

    void onEnter() override;
    void onExit()  override;
    void tick()    override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;
    const char* logHeader() const override;
    bool logRow(char* out, size_t n) override;

private:
    static constexpr int CH = 4;

    struct Chan {
        uint32_t mv   = 0;
        uint32_t mn   = 0xFFFFFFFF;
        uint32_t mx   = 0;
        float    avg  = 0;
    };
    Chan     _c[CH];
    uint8_t  _osr    = 8;      // oversampling
    uint8_t  _scale  = 0;      // divider preset
    bool     _hold   = false;
    uint32_t _lastPoll = 0;

    static float  scaleMul(uint8_t s);
    static const char* scaleName(uint8_t s);
    void resetStats();
};

extern ToolAnalogIn toolAnalogIn;

}  // namespace cg
