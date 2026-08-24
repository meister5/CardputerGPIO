/**
 * ToolSPI.h — send bytes on SPI and see exactly what came back.
 *
 * SPI has no addressing and no acknowledgement, so a device that is silent
 * looks identical to one that is not there. The only way to tell is to send a
 * known command and look at MISO, which is what this does: type the bytes,
 * press Enter, read the response beside them.
 *
 * Note the default pins are the microSD bus (G40/G14/G39). They are only
 * offered once "allow microSD pins" is enabled in Settings, since driving
 * them fights the card reader.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolSPI : public Tool {
public:
    const char* id()    const override { return "spi"; }
    const char* name()  const override { return "SPI Probe"; }
    const char* blurb() const override { return "clock bytes, read MISO"; }
    Cat         cat()   const override { return Cat::Bus; }
    bool drivesOutputs() const override { return true; }

    const Role* roles()     const override;
    int         roleCount() const override { return 4; }

    void onEnter() override;
    void onExit()  override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;

private:
    static constexpr int MAXB = 12;

    uint8_t  _tx[MAXB] = {};
    uint8_t  _rx[MAXB] = {};
    int      _len      = 0;
    bool     _done     = false;
    uint8_t  _mode     = 0;
    uint8_t  _speedIdx = 2;
    char     _entry[3] = {};
    int      _entryLen = 0;
    bool     _ok       = false;

    static uint32_t speed(uint8_t i);
    void transact();
};

extern ToolSPI toolSPI;

}  // namespace cg
