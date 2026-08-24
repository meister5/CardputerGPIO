/**
 * ToolScope.h — four-channel logic analyser with a real trigger.
 *
 * The v1 "logic analyzer" sampled one point per draw call, so its effective
 * rate was the frame rate and its buffer filled over several seconds. This is
 * a burst capture: arm, wait for the trigger edge, then fill the buffer in a
 * tight cycle-counted loop and stop. That is how you actually catch a start
 * bit or a chip-select pulse.
 *
 * Sampling reads the GPIO input registers directly rather than calling
 * digitalRead four times, which is what makes the megahertz rates reachable.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolScope : public Tool {
public:
    const char* id()    const override { return "scope"; }
    const char* name()  const override { return "Logic Analyzer"; }
    const char* blurb() const override { return "4ch burst capture + trigger"; }
    Cat         cat()   const override { return Cat::Signal; }

    const Role* roles()     const override;
    int         roleCount() const override { return 4; }

    void onEnter() override;
    void onExit()  override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;

private:
    enum class Trig : uint8_t { None, Rise, Fall, COUNT };
    enum class Phase : uint8_t { Idle, Armed, Done };

    static constexpr int CH      = 4;
    static constexpr int SAMPLES = 1024;

    uint8_t  _buf[SAMPLES] = {};      // one nibble per sample, ch0 in bit 0
    Phase    _phase   = Phase::Idle;
    Trig     _trig    = Trig::Rise;
    int      _trigCh  = 0;
    uint8_t  _rateIdx = 4;
    int      _zoom    = 1;            // samples per pixel column
    int      _pan     = 0;
    int      _cursor  = -1;
    bool     _triggered = false;

    static uint32_t rateHz(uint8_t i);
    static const char* trigName(Trig t);

    void capture();
    void drawWaves();
};

extern ToolScope toolScope;

}  // namespace cg
