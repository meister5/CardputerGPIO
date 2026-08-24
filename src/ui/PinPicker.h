/**
 * PinPicker.h — assign GPIOs to a tool's roles.
 *
 * The v1 configurator was a forced linear wizard: to change the third of
 * three roles you stepped through the first two, and there was no way to see
 * that you had just assigned the same pin twice. This shows every role at
 * once, edits in place, and flags the two mistakes that actually happen --
 * assigning one pin to two roles, and picking a pin that shares the microSD
 * bus.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

enum class PickerResult { Pending, Saved, Cancelled };

class PinPicker {
public:
    void begin(Tool* tool);
    void draw();
    bool onKey(const KeyEvent& ev);
    PickerResult result() const { return _result; }

private:
    Tool*        _tool   = nullptr;
    PickerResult _result = PickerResult::Pending;
    int          _cursor = 0;
    int          _scroll = 0;
    int8_t       _work[Tool::MAX_ROLES] = {};

    static constexpr int ROW_H   = 15;
    static constexpr int VISIBLE = 5;

    void  cyclePin(int role, int dir);
    bool  duplicated(int role) const;
    const char* issue(int role) const;   // nullptr when the role is fine
    void  resetDefaults();
};

}  // namespace cg
