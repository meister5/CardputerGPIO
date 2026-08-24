/**
 * ToolDigitalIn.h — six-channel logic probe with edge counting.
 *
 * Edges are counted in an ISR rather than by polling. The v1 monitor sampled
 * inside the draw loop, so anything faster than the frame rate was invisible
 * and the "edge counter" mostly counted frames. An ISR keeps up into the tens
 * of kHz, which is where a bench probe actually needs to be.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolDigitalIn : public Tool {
public:
    const char* id()    const override { return "din"; }
    const char* name()  const override { return "Digital In"; }
    const char* blurb() const override { return "6ch probe, edge counts"; }
    Cat         cat()   const override { return Cat::Digital; }

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
    static constexpr int CH = 6;

    uint8_t  _pull = 1;            // 0 none, 1 up, 2 down
    bool     _lvl[CH] = {};
    uint32_t _shown[CH] = {};      // counts at last redraw
    float    _hz[CH] = {};
    uint32_t _lastRate = 0;
    uint32_t _rateBase[CH] = {};
    bool     _attached = false;

    void attach();
    void detach();
    void resetCounts();
    static const char* pullName(uint8_t p);
};

extern ToolDigitalIn toolDigitalIn;

}  // namespace cg
