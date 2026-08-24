/**
 * ToolDigitalOut.h — drive up to eight outputs by hand or from a pattern.
 *
 * Manual toggling covers most bench work; the pattern generator covers the
 * rest (walking a chip-select line, clocking a counter, watching a bus
 * decoder) without needing a second device to produce the stimulus.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolDigitalOut : public Tool {
public:
    const char* id()    const override { return "dout"; }
    const char* name()  const override { return "Digital Out"; }
    const char* blurb() const override { return "8 outputs + pattern gen"; }
    Cat         cat()   const override { return Cat::Digital; }
    bool drivesOutputs() const override { return true; }

    const Role* roles()     const override;
    int         roleCount() const override { return 8; }

    void onEnter() override;
    void onExit()  override;
    void tick()    override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;

private:
    enum class Pat : uint8_t { Off, Walk, Bounce, Count, Alt, COUNT };

    Pat      _pat      = Pat::Off;
    uint32_t _stepMs   = 200;
    uint32_t _lastStep = 0;
    int      _phase    = 0;
    int      _dir      = 1;
    bool     _lvl[8]   = {};

    void applyAll();
    void stepPattern();
    static const char* patName(Pat p);
};

extern ToolDigitalOut toolDigitalOut;

}  // namespace cg
