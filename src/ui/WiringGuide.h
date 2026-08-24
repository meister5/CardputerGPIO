/**
 * WiringGuide.h — what to plug in, before anything starts driving pins.
 *
 * Two views, toggled with TAB:
 *   LIST  role -> GPIO -> what to connect it to, plus any warning
 *   MAP   the real EXT 2.54-14P header and Grove port drawn to scale, with
 *         this tool's assigned pins highlighted in their role colour
 *
 * The map view is the one that saves you counting header pins with a
 * fingernail, which is the actual failure mode when wiring this board.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

enum class WiringResult { Pending, Start, Configure, Back };

class WiringGuide {
public:
    void begin(Tool* tool);
    void draw();
    bool onKey(const KeyEvent& ev);
    WiringResult result() const { return _result; }

private:
    Tool*        _tool   = nullptr;
    WiringResult _result = WiringResult::Pending;
    bool         _mapView = false;
    int          _scroll  = 0;

    void drawList();
    void drawMap();

    // Role index assigned to this GPIO, or -1.
    int  roleOf(int gpio) const;

    // Why this role's pin will not work, or nullptr when it is fine. The
    // answer changes with the radio and the SD opt-in, so it is worked out
    // fresh each frame rather than cached at assignment time.
    const char* roleProblem(int role) const;
};

}  // namespace cg
