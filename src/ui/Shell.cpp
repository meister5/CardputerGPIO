#include "Shell.h"
#include "../core/Settings.h"
#include "../core/Logger.h"
#include <string.h>
#include <ctype.h>

namespace cg {

Shell shell;

static constexpr int   ROW_H   = 17;
static constexpr int   VISIBLE = 6;
static constexpr uint32_t FRAME_MS = 33;   // ~30 fps

// ── Global key reference, shown on F1 from anywhere ───────────────────────
static const char* HELP_LINES[] = {
    "This keyboard has no arrow or ESC keys.",
    "They live on the Fn layer, as printed",
    "on the keycaps:",
    "",
    "  Fn + ;   up          Fn + ,   left",
    "  Fn + .   down        Fn + /   right",
    "  Fn + `   ESC         Fn + 1-0 F1-F10",
    "  Fn + Aa  caps lock   DEL      back",
    "",
    "F1 opens help. F2 starts/stops CSV",
    "logging in tools that measure.",
    "Held keys auto-repeat.",
    "",
    "In the menu: type to search, Fn+,/ to",
    "filter by category, digits to jump.",
};
static constexpr int HELP_N = (int)(sizeof(HELP_LINES) / sizeof(HELP_LINES[0]));

void Shell::begin() {
    _state       = State::Splash;
    _splashUntil = millis() + 900;
    rebuildView();
}

void Shell::add(Tool* t) {
    if (_count >= MAX_TOOLS || !t) return;
    _tools[_count++] = t;

    // Resolve saved pin assignments now, so the menu can show what is ready.
    for (int i = 0; i < t->roleCount(); i++) {
        const Role& r = t->roles()[i];
        int n = 0;
        const int8_t* pool = (r.dir == RoleDir::Adc) ? poolAdc(n) : poolGpio(n);

        int fallback = -1;
        if (r.pref >= 0 && pinGpioOk(r.pref) &&
            (r.dir != RoleDir::Adc || pinAdcOk(r.pref))) {
            fallback = r.pref;
        } else if (n > 0) {
            fallback = pool[i % n];
        }
        t->setPin(i, settings.pin(t->id(), i, fallback));
    }
    rebuildView();
}

// ── Filtering ─────────────────────────────────────────────────────────────
static bool ciContains(const char* hay, const char* needle) {
    if (!*needle) return true;
    for (const char* h = hay; *h; h++) {
        const char *a = h, *b = needle;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
        if (!*b) return true;
    }
    return false;
}

bool Shell::matches(int i) const {
    Tool* t = _tools[i];
    if (_catFilter >= 0 && (int)t->cat() != _catFilter) return false;
    if (_searchLen == 0) return true;
    return ciContains(t->name(), _search) || ciContains(t->blurb(), _search);
}

void Shell::rebuildView() {
    _viewN = 0;
    for (int i = 0; i < _count; i++)
        if (matches(i)) _view[_viewN++] = i;

    if (_cursor >= _viewN) _cursor = _viewN > 0 ? _viewN - 1 : 0;
    if (_cursor < _scroll) _scroll = _cursor;
    if (_cursor >= _scroll + VISIBLE) _scroll = _cursor - VISIBLE + 1;
    if (_scroll < 0) _scroll = 0;
}

// ── Frame ─────────────────────────────────────────────────────────────────
void Shell::run() {
    keys.update();

    // Splash is the only screen that advances on its own.
    if (_state == State::Splash) {
        drawSplash();
        if ((int32_t)(millis() - _splashUntil) >= 0) {
            _state = State::Menu;
            keys.flush();
        }
        return;
    }

    KeyEvent ev;
    while (keys.next(ev)) {
        // F1 is help everywhere except inside help itself.
        if (ev.key == Key::Fkey && ev.num == 1 && _state != State::Help) {
            _helpReturn = _state;
            _state      = State::Help;
            continue;
        }

        switch (_state) {
            case State::Help:
                _state = _helpReturn;
                break;

            case State::Menu:
                onMenuKey(ev);
                break;

            case State::Wiring:
                _wiring.onKey(ev);
                switch (_wiring.result()) {
                    case WiringResult::Start:
                        if (_active->drivesOutputs() && settings.armOutputs())
                            _state = State::Arm;
                        else
                            startTool();
                        break;
                    case WiringResult::Configure:
                        _picker.begin(_active);
                        _state = State::Picker;
                        break;
                    case WiringResult::Back:
                        toMenu();
                        break;
                    default: break;
                }
                break;

            case State::Picker:
                _picker.onKey(ev);
                if (_picker.result() != PickerResult::Pending) {
                    _wiring.begin(_active);
                    _state = State::Wiring;
                }
                break;

            case State::Arm:
                onArmKey(ev);
                break;

            case State::Running:
                if (ev.key == Key::Fkey && ev.num == 2) { toggleLogging(); break; }
                if (!_active->onKey(ev)) {
                    if (ev.key == Key::Back || ev.key == Key::Esc) {
                        stopTool();
                    }
                }
                break;

            default: break;
        }
        if (_state == State::Splash) break;
    }

    // ── Render, capped so a fast tool cannot starve the keyboard ──────────
    if (_state == State::Running && _active) {
        _active->tick();
        if (logger.active()) {
            char csv[128];
            if (_active->logRow(csv, sizeof(csv))) logger.row(csv);
        }
    }

    uint32_t now = millis();
    if ((uint32_t)(now - _lastFrame) < FRAME_MS) return;
    _lastFrame = now;

    switch (_state) {
        case State::Menu:    drawMenu();      break;
        case State::Wiring:  _wiring.draw();  break;
        case State::Picker:  _picker.draw();  break;
        case State::Arm:     drawArm();       break;
        case State::Help:    drawHelp();      break;
        case State::Running:
            ui.clear();
            _active->draw();
            drawLogBadge();
            ui.drawNotification();
            ui.push();
            break;
        default: break;
    }
}

// ── Menu ──────────────────────────────────────────────────────────────────
void Shell::onMenuKey(const KeyEvent& ev) {
    switch (ev.key) {
        case Key::Up:
            if (_cursor > 0) _cursor--;
            if (_cursor < _scroll) _scroll = _cursor;
            return;
        case Key::Down:
            if (_cursor < _viewN - 1) _cursor++;
            if (_cursor >= _scroll + VISIBLE) _scroll = _cursor - VISIBLE + 1;
            return;
        case Key::Left:
            _catFilter = (_catFilter <= -1) ? (int)Cat::COUNT - 1 : _catFilter - 1;
            rebuildView();
            return;
        case Key::Right:
            _catFilter = (_catFilter >= (int)Cat::COUNT - 1) ? -1 : _catFilter + 1;
            rebuildView();
            return;
        case Key::Enter:
            if (_viewN > 0) openTool(_view[_cursor]);
            return;
        case Key::Back:
            if (_searchLen > 0) { _search[--_searchLen] = 0; rebuildView(); }
            return;
        case Key::Esc:
            _searchLen = 0;
            _search[0] = 0;
            _catFilter = -1;
            rebuildView();
            return;
        case Key::Char: {
            // Digits jump to a visible row; letters extend the search.
            int d = ev.digit();
            if (d >= 1 && d <= 9 && _searchLen == 0) {
                int idx = d - 1;
                if (idx < _viewN) {
                    _cursor = idx;
                    if (_cursor < _scroll) _scroll = _cursor;
                    if (_cursor >= _scroll + VISIBLE) _scroll = _cursor - VISIBLE + 1;
                    openTool(_view[_cursor]);
                }
                return;
            }
            if (ev.ch >= ' ' && ev.ch <= '~' && _searchLen < (int)sizeof(_search) - 1) {
                _search[_searchLen++] = ev.ch;
                _search[_searchLen]   = 0;
                _cursor = 0;
                _scroll = 0;
                rebuildView();
            }
            return;
        }
        default: return;
    }
}

void Shell::drawMenu() {
    ui.clear();

    char right[16];
    int batt = M5.Power.getBatteryLevel();
    if (batt >= 0) snprintf(right, sizeof(right), "%d%%", batt);
    else           snprintf(right, sizeof(right), "ADV");
    ui.header("GPIO Toolbox", right, C_HDR);

    if (_viewN == 0) {
        ui.text(8, BODY_Y + 34, C_WARN, "Nothing matches that filter.");
        ui.text(8, BODY_Y + 48, C_DIM, "[Fn+`] clear");
    }

    int y   = BODY_Y + 1;
    int end = _scroll + VISIBLE;
    if (end > _viewN) end = _viewN;

    for (int i = _scroll; i < end; i++) {
        Tool* t   = _tools[_view[i]];
        bool  sel = (i == _cursor);
        uint16_t cc = catColor(t->cat());

        ui.listRow(y, ROW_H, sel, cc);
        ui.g().fillRect(4, y + 4, 3, ROW_H - 8, cc);

        // Row number is only a shortcut while no search is active.
        if (_searchLen == 0 && i < 9)
            ui.textf(10, y + 2, C_FAINT, "%d", i + 1);

        ui.textf(20, y + 1, sel ? C_WHITE : C_TEXT, "%-22.22s", t->name());
        ui.textf(20, y + 9, sel ? C_TEXT : C_DIM,   "%-26.26s", t->blurb());

        ui.textRight(SCR_W - 7, y + 5, sel ? cc : C_FAINT, catName(t->cat()));
        y += ROW_H;
    }

    ui.scrollbar(SCR_W - 4, BODY_Y + 1, VISIBLE * ROW_H, _scroll, VISIBLE, _viewN);

    if (_searchLen > 0)      ui.footerf("search: %s_   [DEL] erase  [Fn+`] clear", _search);
    else if (_catFilter >= 0) ui.footerf("cat: %s   [<>] change  [F1] help",
                                         catName((Cat)_catFilter));
    else                      ui.footer("[^v] pick  [ENT] open  type to search  [F1] help");

    ui.drawNotification();
    ui.push();
}

// ── Splash ────────────────────────────────────────────────────────────────
void Shell::drawSplash() {
    ui.clear(C_BG);
    ui.g().fillRect(0, 40, SCR_W, 3, C_TITLE);
    ui.textBig(14, 14, C_TITLE, 2, "GPIO Toolbox");
    ui.text(16, 50, C_TEXT, boardName());

    if (!boardIsAdv()) {
        ui.text(16, 66, C_LOW, "Unsupported board detected.");
        ui.text(16, 78, C_DIM, "Built for Cardputer ADV pin map.");
    } else {
        int n = 0; poolGpio(n);
        ui.textf(16, 66, C_DIM, "%d usable GPIO  -  F1 for keys", n);
    }
    ui.textRight(SCR_W - 8, SCR_H - 12, C_FAINT, "v2.0");
    ui.push();
}

// ── Arm confirmation ──────────────────────────────────────────────────────
void Shell::drawArm() {
    ui.clear();
    ui.header("Arm outputs?", nullptr, C_WARN);

    ui.textf(8, BODY_Y + 4, C_TEXT, "%.30s will drive:", _active->name());

    int y = BODY_Y + 18;
    int shown = 0;
    for (int i = 0; i < _active->roleCount() && shown < 4; i++) {
        const Role& r = _active->roles()[i];
        if (r.dir != RoleDir::Out && r.dir != RoleDir::Pwm) continue;
        char lbl[20];
        pinLabel(_active->pin(i), lbl, sizeof(lbl));
        ui.textf(14, y, C_WARN, "%-8.8s %s", r.label, lbl);
        y += 11;
        shown++;
    }
    if (shown == 0) ui.text(14, y, C_WARN, "(pins assigned at run time)");

    ui.text(8, BODY_B - 12, C_DIM, "Check your wiring before arming.");
    ui.footer("[ENT] arm   [DEL] cancel   [O] disable prompt");
    ui.push();
}

void Shell::onArmKey(const KeyEvent& ev) {
    if (ev.key == Key::Enter) {
        startTool();
    } else if (ev.key == Key::Back || ev.key == Key::Esc) {
        _wiring.begin(_active);
        _state = State::Wiring;
    } else if (ev.ci('o')) {
        settings.setArmOutputs(false);
        ui.notify("arm prompt off");
        startTool();
    }
}

// ── Help ──────────────────────────────────────────────────────────────────
void Shell::drawHelp() {
    ui.clear();

    int   n     = 0;
    const char* const* lines = nullptr;
    const char* title = "Keys";

    // A running tool gets to show its own help page instead.
    if (_helpReturn == State::Running && _active) {
        lines = _active->help(n);
        if (n > 0) title = _active->name();
    }
    if (n == 0) { lines = HELP_LINES; n = HELP_N; title = "Keyboard"; }

    ui.header(title, "help", C_INFO);
    int y = BODY_Y + 2;
    for (int i = 0; i < n && y < BODY_B - 6; i++) {
        ui.textf(6, y, lines[i][0] ? C_TEXT : C_DIM, "%.39s", lines[i]);
        y += 9;
    }
    ui.footer("any key to close");
    ui.push();
}

// ── Transitions ───────────────────────────────────────────────────────────
void Shell::openTool(int toolIdx) {
    _active = _tools[toolIdx];
    keys.flush();
    if (_active->roleCount() > 0) {
        _wiring.begin(_active);
        _state = State::Wiring;
    } else if (_active->drivesOutputs() && settings.armOutputs()) {
        _state = State::Arm;
    } else {
        startTool();
    }
}

void Shell::startTool() {
    keys.flush();
    _active->onEnter();
    _state = State::Running;
}

void Shell::stopTool() {
    if (_active) _active->onExit();
    pins.releaseAll();
    toMenu();
}

void Shell::toMenu() {
    _active = nullptr;
    _state  = State::Menu;
    keys.flush();
    rebuildView();
}


// ── CSV logging ───────────────────────────────────────────────────────────
void Shell::toggleLogging() {
    if (!_active) return;

    if (logger.active()) {
        uint32_t rows = logger.rows();
        char file[32];
        snprintf(file, sizeof(file), "%s", logger.fileName());
        bool toSd = ((uint8_t)logger.sink() & (uint8_t)LogSink::Sd) != 0;
        logger.stop();
        if (toSd) ui.notify("saved %s (%lu rows)", file, (unsigned long)rows);
        else      ui.notify("logged %lu rows to serial", (unsigned long)rows);
        return;
    }

    if (!_active->canLog()) {
        ui.notify("this tool has nothing to log");
        return;
    }

    // Prefer the card, but never let a missing card stop a capture: serial is
    // always there and a bench session usually has the USB lead plugged in
    // anyway.
    if (!logger.start(_active->id(), _active->logHeader(), LogSink::Both)) {
        ui.notify("log failed: %s", logger.lastError());
        return;
    }
    if (((uint8_t)logger.sink() & (uint8_t)LogSink::Sd) == 0)
        ui.notify("serial only: %s", logger.lastError()[0] ? logger.lastError() : "no card");
    else
        ui.notify("logging to %s", logger.fileName());
}

void Shell::drawLogBadge() {
    if (!logger.active()) return;

    // Blinks, so it is obvious at a glance that a capture is still running.
    bool on = ((millis() / 500) & 1) != 0;
    int  w  = 46;
    int  x  = SCR_W - w - 3;
    int  y  = SCR_H - FTR_H - 13;
    ui.g().fillRoundRect(x, y, w, 12, 3, on ? C_LOW : C_PANEL2);
    ui.g().fillCircle(x + 7, y + 6, 3, C_WHITE);
    ui.textf(x + 13, y + 2, C_WHITE, "%lus", (unsigned long)logger.seconds());
}

}  // namespace cg
