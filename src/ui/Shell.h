/**
 * Shell.h — application state machine and tool launcher.
 *
 * Owns the loop, the menu, and the four screens every tool passes through:
 *
 *   MENU --ENTER--> WIRING --ENTER--> [ARM] --> RUNNING
 *                     |  C --> PICKER --+
 *                     |  DEL -----------+--> MENU
 *
 * Tools with no roles skip WIRING entirely. Tools that drive outputs get an
 * ARM confirmation first, so a pin set saved months ago cannot start sourcing
 * current into someone's circuit the instant they press Enter.
 */

#pragma once
#include "../core/Tool.h"
#include "PinPicker.h"
#include "WiringGuide.h"

namespace cg {

class Shell {
public:
    static constexpr int MAX_TOOLS = 24;

    void begin();
    void add(Tool* t);
    void run();          // one frame; call from loop()

    // ── Remote control ────────────────────────────────────────────────────
    // The web interface drives these rather than duplicating the state
    // machine, so a browser and the keyboard can never disagree about what
    // the device is doing.
    Tool*       activeTool() const { return _active; }
    const char* stateName()  const;
    bool        openToolById(const char* id);   // as if it were picked in the menu
    void        backToMenu();                   // as if DEL were pressed
    bool        startActiveTool();              // as if ENTER were pressed in wiring

    // The registry, so the setup manager can snapshot every tool's pins.
    int   toolCount() const { return _count; }
    Tool* toolAt(int i) const { return (i >= 0 && i < _count) ? _tools[i] : nullptr; }

private:
    enum class State : uint8_t { Splash, Menu, Wiring, Picker, Arm, Running, Help };

    Tool*  _tools[MAX_TOOLS] = {};
    int    _count  = 0;
    State  _state  = State::Splash;
    Tool*  _active = nullptr;

    // ── Menu ──────────────────────────────────────────────────────────────
    int   _cursor = 0;
    int   _scroll = 0;
    int   _catFilter = -1;              // -1 = all
    char  _search[10] = {};
    int   _searchLen = 0;
    int   _view[MAX_TOOLS] = {};        // indices passing the current filter
    int   _viewN = 0;

    uint32_t _splashUntil = 0;
    uint32_t _lastFrame   = 0;
    State    _helpReturn  = State::Menu;

    PinPicker   _picker;
    WiringGuide _wiring;

    void rebuildView();
    bool matches(int toolIdx) const;

    void drawSplash();
    void drawMenu();
    void drawArm();
    void drawLogBadge();
    void toggleLogging();
    void drawHelp();

    // The footer's right-hand corner: the key that means the same thing on
    // every screen, or nothing at all where it would not be true.
    const char* globalHint() const;

    // A browser is polling the framebuffer, so frames are worth composing
    // even with the panel dark.
    bool mirrorWatched() const;

    void onMenuKey(const KeyEvent& ev);
    void onArmKey(const KeyEvent& ev);

    void openTool(int toolIdx);
    void startTool();
    void stopTool();
    void toMenu();
};

extern Shell shell;

}  // namespace cg
