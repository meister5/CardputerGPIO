/**
 * ToolPWM.h — square wave generator, 1 Hz to 10 MHz.
 *
 * Two engines behind one screen:
 *
 *   LEDC      5 Hz and up, in hardware. Duty resolution is derived from the
 *             requested frequency (see Pins::pwmBestBits). The v1 tool pinned
 *             resolution at 10 bits, which quietly capped it at 78 kHz while
 *             the UI still advertised 1 MHz.
 *
 *   Software  below 5 Hz, where LEDC's divider cannot reach. The pin is
 *             toggled from tick() against millis(), which is exact enough at
 *             those periods and keeps the low end usable for relay and
 *             indicator work.
 *
 * The frequency LEDC actually produced is displayed next to the one you
 * asked for, because the divider rarely lands exactly on a round number.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolPWM : public Tool {
public:
    const char* id()    const override { return "pwm"; }
    const char* name()  const override { return "PWM Generator"; }
    const char* blurb() const override { return "1Hz-10MHz, live duty"; }
    Cat         cat()   const override { return Cat::Signal; }
    bool drivesOutputs() const override { return true; }

    const Role* roles()     const override;
    int         roleCount() const override { return 1; }

    void onEnter() override;
    void onExit()  override;
    void tick()    override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;

private:
    uint32_t _want    = 1000;
    uint32_t _actual  = 0;
    float    _duty    = 50.0f;
    bool     _on      = false;
    bool     _soft    = false;
    uint8_t  _presetIdx = 4;

    // software low-frequency engine
    uint32_t _swNext  = 0;
    bool     _swLevel = false;

    void apply();
    void stop();
    void nudgeFreq(int dir, bool coarse);
};

extern ToolPWM toolPWM;

}  // namespace cg
