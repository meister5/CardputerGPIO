/**
 * ToolFreq.h — frequency, period, duty and total count on one pin.
 *
 * Counting happens in the ESP32's PCNT peripheral, not in an interrupt, so
 * the ceiling is set by the hardware rather than by how fast we can service
 * an ISR. That is the difference between topping out around 50 kHz and
 * measuring a several-MHz clock.
 *
 * Duty cycle is measured separately with pulseIn(), which is a blocking
 * timing loop and only meaningful below roughly 100 kHz -- it is shown as
 * unavailable above that rather than printing a number that is wrong.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolFreq : public Tool {
public:
    const char* id()    const override { return "freq"; }
    const char* name()  const override { return "Frequency Counter"; }
    const char* blurb() const override { return "hardware counter + duty"; }
    Cat         cat()   const override { return Cat::Signal; }

    const Role* roles()     const override;
    int         roleCount() const override { return 1; }

    void onEnter() override;
    void onExit()  override;
    void tick()    override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;
    const char* logHeader() const override;
    bool logRow(char* out, size_t n) override;

private:
    PulseCounter _pc;
    uint8_t  _gateIdx = 1;
    float    _hz      = 0;
    int64_t  _total   = 0;
    bool     _dutyOn  = true;
    float    _duty    = 0;
    uint32_t _highUs  = 0, _lowUs = 0;
    uint32_t _lastDuty = 0;
    bool     _ok      = false;

    static uint32_t gateMs(uint8_t i);
    void measureDuty();
};

extern ToolFreq toolFreq;

}  // namespace cg
