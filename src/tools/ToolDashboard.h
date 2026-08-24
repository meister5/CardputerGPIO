/**
 * ToolDashboard.h — live view of every usable pin, with inline control.
 *
 * The screen you leave open while probing: every exposed GPIO, what mode it
 * is in, what it is reading, and -- for pins you have switched to output --
 * a one-key toggle. Modes set here persist while the tool is open, so it
 * doubles as a scratch bench without configuring roles first.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolDashboard : public Tool {
public:
    const char* id()    const override { return "dash"; }
    const char* name()  const override { return "Pin Dashboard"; }
    const char* blurb() const override { return "all pins, live, one screen"; }
    Cat         cat()   const override { return Cat::Digital; }

    void onEnter() override;
    void onExit()  override;
    void tick()    override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;

    const char* hints() const override { return "[M] mode [SPC] toggle [DEL] back"; }
    const char* const* help(int& n) const override;

private:
    static constexpr int VISIBLE = 6;
    static constexpr int ROW_H   = 16;

    int  _cursor = 0;
    int  _scroll = 0;
    int  _n      = 0;
    const int8_t* _pool = nullptr;

    uint32_t _lastPoll = 0;
    bool     _lvl[16]  = {};
    uint32_t _mv[16]   = {};

    void cycleMode(int idx);
};

extern ToolDashboard toolDashboard;

}  // namespace cg
