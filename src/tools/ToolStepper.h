/**
 * ToolStepper.h — drive a stepper from either common wiring.
 *
 * STEP/DIR  A4988, DRV8825, TMC2208 and friends. STEP is pulsed, DIR sets
 *           direction, EN is active-low enable.
 * 4-WIRE    ULN2003 boards and the 28BYJ-48, driven with a half-step
 *           sequence for smoother low-speed motion.
 *
 * Position is tracked in steps so you can zero it, jog away and return.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolStepper : public Tool {
public:
    const char* id()    const override { return "step"; }
    const char* name()  const override { return "Stepper Driver"; }
    const char* blurb() const override { return "STEP/DIR or 4-wire"; }
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
    enum class Wiring : uint8_t { StepDir, FourWire };

    Wiring   _wiring  = Wiring::StepDir;
    bool     _run     = false;
    bool     _fwd     = true;
    bool     _enabled = true;
    uint32_t _rate    = 200;        // steps per second
    long     _pos     = 0;
    long     _target  = 0;
    bool     _seeking = false;
    uint32_t _lastStep = 0;
    uint8_t  _phase   = 0;

    void configure();
    void releaseAllPins();
    void doStep(bool forward);
    void setEnable(bool on);
    uint32_t stepIntervalUs() const;
};

extern ToolStepper toolStepper;

}  // namespace cg
