/**
 * ToolServo.h — drive up to four hobby servos.
 *
 * Standard 50 Hz frame, pulse width 500-2500 us, mapped to 0-180 degrees.
 * At 50 Hz LEDC gives us the full 14 bits of duty resolution, which works out
 * to about 1.2 us per step -- finer than any hobby servo can resolve.
 *
 * Servos draw far more than a GPIO can source. Power them from the 5V rail on
 * the EXT header or an external supply, and tie the grounds together.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolServo : public Tool {
public:
    const char* id()    const override { return "servo"; }
    const char* name()  const override { return "Servo Driver"; }
    const char* blurb() const override { return "4ch, angle or microseconds"; }
    Cat         cat()   const override { return Cat::Signal; }
    bool drivesOutputs() const override { return true; }

    const Role* roles()     const override;
    int         roleCount() const override { return 4; }

    void onEnter() override;
    void onExit()  override;
    void tick()    override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;

private:
    static constexpr int CH       = 4;
    static constexpr int FRAME_HZ = 50;
    static constexpr int FRAME_US = 20000;

    int      _us[CH]     = { 1500, 1500, 1500, 1500 };
    bool     _live[CH]   = {};
    int      _sel        = 0;
    bool     _sweep      = false;
    int      _sweepDir   = 1;
    uint32_t _lastSweep  = 0;
    int      _minUs      = 500;
    int      _maxUs      = 2500;

    void applyChan(int i);
    void detachChan(int i);
    int  angleOf(int i) const;
};

extern ToolServo toolServo;

}  // namespace cg
