/**
 * ToolIR.h — transmit infrared using the emitter built into the ADV.
 *
 * The ADV has an IR LED on G44, so this needs no wiring at all: point the
 * top edge of the Cardputer at the device and press a key.
 *
 * Timing comes from the RMT peripheral with a 1 us tick and a 38 kHz carrier
 * generated in hardware, which is the only way to get IR timing right on a
 * chip that is also running WiFi and a display refresh.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolIR : public Tool {
public:
    const char* id()    const override { return "ir"; }
    const char* name()  const override { return "IR Transmitter"; }
    const char* blurb() const override { return "onboard emitter, NEC/Sony/RC5"; }
    Cat         cat()   const override { return Cat::Bus; }

    void onEnter() override;
    void onExit()  override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;

private:
    enum class Proto : uint8_t { NEC, Sony12, RC5, COUNT };
    enum class Field : uint8_t { Addr, Cmd, COUNT };

    Proto    _proto = Proto::NEC;
    Field    _field = Field::Addr;
    uint16_t _addr  = 0x00;
    uint16_t _cmd   = 0x00;
    uint32_t _sent  = 0;
    bool     _ready = false;
    uint32_t _lastSend = 0;

    bool  initRmt();
    void  send();
    int   buildNEC(rmt_data_t* out);
    int   buildSony(rmt_data_t* out);
    int   buildRC5(rmt_data_t* out);
    static const char* protoName(Proto p);
    void  adjust(int delta);
};

extern ToolIR toolIR;

}  // namespace cg
