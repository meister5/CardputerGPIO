/**
 * ToolNeoPixel.h — drive a WS2812 / SK6812 strip from one data pin.
 *
 * The protocol is a 800 kHz self-clocked bitstream where a 0 and a 1 differ
 * only in the width of the high pulse, so it cannot be bit-banged reliably
 * while a display refresh or WiFi interrupt might land mid-byte. This uses
 * RMT at a 0.1 us tick, which clocks the whole frame out of hardware.
 *
 * A strip is a 5 V device fed from a 3.3 V pin. Most take it; if the first
 * LED is dark or the colours are wrong, that level mismatch is why.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolNeoPixel : public Tool {
public:
    const char* id()    const override { return "npx"; }
    const char* name()  const override { return "NeoPixel"; }
    const char* blurb() const override { return "WS2812 strip driver"; }
    Cat         cat()   const override { return Cat::Signal; }
    bool drivesOutputs() const override { return true; }

    const Role* roles()     const override;
    int         roleCount() const override { return 1; }

    void onEnter() override;
    void onExit()  override;
    void tick()    override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;

private:
    static constexpr int MAXPIX = 64;

    enum class Fx : uint8_t { Solid, Rainbow, Chase, Breathe, Sparkle, Off, COUNT };
    enum class Sel : uint8_t { Effect, Count, Bright, Speed, Hue, COUNT };

    Fx   _fx    = Fx::Solid;
    Sel  _sel   = Sel::Effect;
    int  _count = 8;
    int  _bright = 40;      // percent — a strip at 100 % can outrun USB power
    int  _speed  = 5;       // 1..10
    int  _hue    = 0;       // 0..255
    bool _grb    = true;    // SK6812 RGBW strips and clones differ

    uint8_t  _px[MAXPIX][3] = {};
    uint32_t _lastStep = 0;
    uint32_t _phase    = 0;
    bool     _ready    = false;

    bool initRmt(int gpio);
    void render();
    void show();
    void adjust(int delta);
    static void hsv(uint8_t h, uint8_t s, uint8_t v, uint8_t* rgb);
    static const char* fxName(Fx f);
};

extern ToolNeoPixel toolNeoPixel;

}  // namespace cg
