/**
 * ToolMeter.h — the pocket multimeter: volts, continuity, resistance, logic.
 *
 * Continuity uses the internal pull-up and the onboard speaker, so it behaves
 * like the beeper on a real DMM. Resistance is a divider measurement against
 * a reference resistor you supply -- the ESP32 has no current source, so
 * there is no way around adding one part, but the arithmetic is done here.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolMeter : public Tool {
public:
    const char* id()    const override { return "meter"; }
    const char* name()  const override { return "Multimeter"; }
    const char* blurb() const override { return "volts, continuity, ohms"; }
    Cat         cat()   const override { return Cat::Analog; }

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
    enum class Mode : uint8_t { Volts, Continuity, Ohms, Logic, COUNT };

    Mode     _mode   = Mode::Volts;
    uint8_t  _refIdx = 1;             // reference resistor for ohms mode
    uint32_t _mv     = 0;
    float    _ohms   = 0;
    bool     _cont   = false;
    bool     _lastCont = false;
    uint32_t _lastPoll = 0;
    uint32_t _mn = 0xFFFFFFFF, _mx = 0;

    void applyMode();
    static uint32_t refOhms(uint8_t i);
    static const char* modeName(Mode m);
};

extern ToolMeter toolMeter;

}  // namespace cg
