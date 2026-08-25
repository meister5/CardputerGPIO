#include "PinPicker.h"
#include "../core/Settings.h"
#include <string.h>

namespace cg {

void PinPicker::begin(Tool* tool) {
    _tool   = tool;
    _result = PickerResult::Pending;
    _cursor = 0;
    _scroll = 0;
    for (int i = 0; i < tool->roleCount(); i++) _work[i] = (int8_t)tool->pin(i);
}

// Roles that need an ADC pin draw from the ADC pool; everything else from the
// general GPIO pool. Both pools already exclude the system I2C pins and, by
// default, the microSD pins.
static const int8_t* poolFor(RoleDir dir, int& n) {
    return (dir == RoleDir::Adc) ? poolAdc(n) : poolGpio(n);
}

void PinPicker::cyclePin(int role, int dir) {
    const Role& r = _tool->roles()[role];
    int n = 0;
    const int8_t* pool = poolFor(r.dir, n);
    if (n == 0) return;

    int idx = 0;
    for (int i = 0; i < n; i++)
        if (pool[i] == _work[role]) { idx = i; break; }

    idx = (idx + dir + n) % n;
    _work[role] = pool[idx];
}

bool PinPicker::duplicated(int role) const {
    for (int i = 0; i < _tool->roleCount(); i++)
        if (i != role && _work[i] == _work[role]) return true;
    return false;
}

const char* PinPicker::issue(int role) const {
    if (_work[role] < 0)      return "unassigned";
    if (duplicated(role))     return "same pin used twice";
    const char* w = pinWarn(_work[role]);
    return w;   // nullptr when clean
}

void PinPicker::resetDefaults() {
    for (int i = 0; i < _tool->roleCount(); i++) {
        const Role& r = _tool->roles()[i];
        int n = 0;
        const int8_t* pool = poolFor(r.dir, n);
        if (n == 0) { _work[i] = -1; continue; }
        if (r.pref >= 0 && pinGpioOk(r.pref) &&
            (r.dir != RoleDir::Adc || pinAdcOk(r.pref))) {
            _work[i] = r.pref;
        } else {
            _work[i] = pool[i % n];
        }
    }
    ui.notify("defaults restored");
}

bool PinPicker::onKey(const KeyEvent& ev) {
    int rc = _tool->roleCount();

    switch (ev.key) {
        case Key::Up:
            if (_cursor > 0) _cursor--;
            if (_cursor < _scroll) _scroll = _cursor;
            return true;
        case Key::Down:
            if (_cursor < rc - 1) _cursor++;
            if (_cursor >= _scroll + VISIBLE) _scroll = _cursor - VISIBLE + 1;
            return true;
        case Key::Left:
            cyclePin(_cursor, -1);
            return true;
        case Key::Right:
            cyclePin(_cursor, +1);
            return true;
        case Key::Enter:
            for (int i = 0; i < rc; i++) {
                _tool->setPin(i, _work[i]);
                settings.setPin(_tool->id(), i, _work[i]);
            }
            _result = PickerResult::Saved;
            ui.beep(2600, 20);
            return true;
        case Key::Back:
        case Key::Esc:
            _result = PickerResult::Cancelled;
            return true;
        case Key::Char:
            if (ev.ci('r')) { resetDefaults(); return true; }
            // Digit jumps straight to a role.
            if (ev.digit() >= 1 && ev.digit() <= rc) {
                _cursor = ev.digit() - 1;
                if (_cursor < _scroll) _scroll = _cursor;
                if (_cursor >= _scroll + VISIBLE) _scroll = _cursor - VISIBLE + 1;
                return true;
            }
            return true;
        default:
            return true;
    }
}

void PinPicker::draw() {
    int rc = _tool->roleCount();

    ui.clear();
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "Pins: %.18s", _tool->name());
    ui.header(hdr, nullptr, C_HDR);

    int y = BODY_Y + 1;
    int end = _scroll + VISIBLE;
    if (end > rc) end = rc;

    for (int i = _scroll; i < end; i++) {
        const Role& r = _tool->roles()[i];
        bool sel = (i == _cursor);
        ui.listRow(y, ROW_H, sel, roleColor(r.dir));

        // role number + label
        ui.textf(6, y + 4, sel ? C_WHITE : C_TEXT, "%d %-9.9s", i + 1, r.label);

        // direction badge
        ui.chip(76, y + 2, roleDirName(r.dir), C_BLACK, roleColor(r.dir));

        // assigned pin
        char lbl[20];
        if (_work[i] >= 0) pinLabel(_work[i], lbl, sizeof(lbl));
        else               snprintf(lbl, sizeof(lbl), "--");

        const char* bad = issue(i);
        ui.textf(120, y + 4, bad ? C_WARN : C_HIGH, "%c%-13.13s%c",
                 sel ? '<' : ' ', lbl, sel ? '>' : ' ');

        y += ROW_H;
    }

    ui.scrollbar(SCR_W - 4, BODY_Y + 1, VISIBLE * ROW_H, _scroll, VISIBLE, rc);

    // ── Issue line for the selected role ──────────────────────────────────
    int noteY = BODY_Y + VISIBLE * ROW_H + 3;
    const char* bad = issue(_cursor);
    if (bad) {
        ui.textf(6, noteY, C_WARN, "! %.36s", bad);
    } else {
        ui.textf(6, noteY, C_DIM, "%.38s", _tool->roles()[_cursor].hint);
    }

    ui.footer("[^v]role [<>]pin [R]def [ENT]ok");
    ui.drawNotification();
}

}  // namespace cg
