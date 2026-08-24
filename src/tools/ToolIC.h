/**
 * ToolIC.h — drive the address lines of a decoder or mux and read the result.
 *
 * This is the tool for the moment you have a 74HC138, a 4051 or a shift-in
 * mux on the bench and want to know whether it works, without writing a
 * sketch for it. Set an address by hand, or let it walk every channel and
 * build a table of what each one reads.
 *
 * Presets only change how many address lines are used and whether the enable
 * is active low -- the wiring names stay the same, so a part that is not in
 * the list still works if you pick the preset with the right line count.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolIC : public Tool {
public:
    const char* id()    const override { return "ic"; }
    const char* name()  const override { return "Decoder / Mux"; }
    const char* blurb() const override { return "74138, 4051, 74151, 74154"; }
    Cat         cat()   const override { return Cat::Digital; }
    bool drivesOutputs() const override { return true; }

    const Role* roles()     const override;
    int         roleCount() const override { return 6; }

    void onEnter() override;
    void onExit()  override;
    void tick()    override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;
    const char* logHeader() const override;
    bool logRow(char* out, size_t n) override;

private:
    static constexpr int MAXCH = 16;

    enum class Part : uint8_t { HC138, HC4051, HC151, HC154, COUNT };
    enum class Read : uint8_t { Digital, Analog, None, COUNT };

    Part     _part    = Part::HC138;
    Read     _read    = Read::Digital;
    int      _addr    = 0;
    bool     _enabled = true;
    bool     _scan    = false;
    uint32_t _dwellMs = 120;
    uint32_t _lastStep = 0;

    int      _val[MAXCH] = {};      // mV in analog mode, 0/1 in digital
    bool     _seen[MAXCH] = {};

    // Roles: 0..3 address, 4 enable, 5 sense.
    static int  addrLines(Part p);
    static bool enableActiveLow(Part p);
    static const char* partName(Part p);
    static const char* readName(Read r);

    void applyAddress();
    void sample();
    int  channels() const { return 1 << addrLines(_part); }
};

extern ToolIC toolIC;

}  // namespace cg
