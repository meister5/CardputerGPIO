/**
 * ToolI2C.h — scan both I2C buses, then actually talk to what you found.
 *
 * The ADV has two buses and the distinction matters:
 *
 *   SYSTEM (G8/G9)  the TCA8418 keyboard controller, the BMI270 IMU and the
 *                   ES8311 codec live here. It is brought out on EXT pins
 *                   8/10, so external devices can share it. A scan will
 *                   always show 0x34, 0x18 and 0x68 -- those are the board
 *                   itself, not your circuit.
 *   GROVE (G1/G2)   the HY2.0-4P port. Empty unless you plug something in.
 *
 * All traffic goes through M5Unified's own I2C driver rather than Wire, which
 * is the same path the keyboard uses -- so scanning the system bus cannot
 * fight the keyboard driver for the peripheral.
 *
 * Scanning is only half a debugging session, so this also does register
 * reads and writes: pick a device, dump its register space, poke a value in.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolI2C : public Tool {
public:
    const char* id()    const override { return "i2c"; }
    const char* name()  const override { return "I2C Explorer"; }
    const char* blurb() const override { return "2 buses, scan + registers"; }
    Cat         cat()   const override { return Cat::Bus; }

    void onEnter() override;
    void onExit()  override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;

private:
    enum class View : uint8_t { Scan, Regs, Write };

    View     _view   = View::Scan;
    bool     _grove  = true;
    uint32_t _freq   = 100000;

    uint8_t  _found[24] = {};
    int      _n         = 0;
    int      _sel       = 0;

    uint8_t  _regs[256] = {};
    bool     _regOk[256] = {};
    int      _regTop  = 0;
    int      _regSel  = 0;

    char     _entry[3] = {};
    int      _entryLen = 0;

    void doScan();
    void readRegs();
    void doWrite();
    void drawScan();
    void drawRegs();
    void drawWrite();
    const char* busName() const { return _grove ? "Grove" : "System"; }
};

extern ToolI2C toolI2C;

}  // namespace cg
