/**
 * MenuSystem.cpp  (v2 - fixed)
 * Full launch-flow state machine: MENU → WIRING_GUIDE → CONFIGURING → RUNNING
 *
 * Fixes:
 *  - pollKey() now uses M5Cardputer.Keyboard (not M5.Keyboard)
 *  - M5Cardputer.update() called at the top of update() so key state refreshes
 *  - Arrow key detection via keysState() fn/opt flags instead of bogus ASCII codes
 *  - ESC detected via status.fn (Fn key) since Cardputer has no dedicated ESC key
 */

#include "MenuSystem.h"

// ── init ──────────────────────────────────────────────────────────────────
void MenuSystem::init(PinManager* pm) {
    _pm = pm;
}

void MenuSystem::addProfile(Profile* p, const char* profileId) {
    if (_profileCount >= MAX_PROFILES) return;
    int idx = _profileCount++;
    _profiles[idx] = p;
    strncpy(_profileIds[idx], profileId, 15);

    // Load persisted pin assignments (or defaults) for this profile
    _configs[idx].load(profileId, p->roles(), p->roleCount(), _pm);
}

// ── showMainMenu ──────────────────────────────────────────────────────────
void MenuSystem::showMainMenu() {
    _state     = State::MENU;
    _activeIdx = -1;
    _cursor    = 0;
    _scrollOff = 0;
    renderMenu();
}

// ── update ────────────────────────────────────────────────────────────────
void MenuSystem::update() {
    // FIX: must call M5Cardputer.update() every loop tick so keyboard state refreshes
    M5Cardputer.update();

    char key = pollKey();

    switch (_state) {

    // ── Main menu ─────────────────────────────────────────────────────
    case State::MENU:
        if (key) handleMenuKey(key);
        break;

    // ── Wiring guide ──────────────────────────────────────────────────
    case State::WIRING_GUIDE:
        _wiringGuide.update();
        if (key) _wiringGuide.onKey(key);
        switch (_wiringGuide.result()) {
            case WiringGuideResult::START:
                startProfile(_activeIdx);
                break;
            case WiringGuideResult::CONFIGURE:
                launchConfigurator(_activeIdx);
                break;
            case WiringGuideResult::BACK:
                returnToMenu();
                break;
            default: break;
        }
        break;

    // ── Pin configurator ──────────────────────────────────────────────
    case State::CONFIGURING:
        _configurator.update();
        if (key) _configurator.onKey(key);
        if (_configurator.isDone()) {
            // Reload config (assignments were just saved)
            _configs[_activeIdx].load(
                _profileIds[_activeIdx],
                _profiles[_activeIdx]->roles(),
                _profiles[_activeIdx]->roleCount(),
                _pm);
            launchWiringGuide(_activeIdx);   // go back to wiring guide with new assignments
        } else if (_configurator.isCancelled()) {
            launchWiringGuide(_activeIdx);   // cancelled — back to wiring guide unchanged
        }
        break;

    // ── Running profile ───────────────────────────────────────────────
    case State::RUNNING:
        if (key == 8 || key == 27) {   // Backspace / ESC-substitute → exit profile
            returnToMenu();
            return;
        }
        if (key) _profiles[_activeIdx]->onKey(_pm, key);
        _profiles[_activeIdx]->update(_pm);
        break;
    }
}

// ── handleMenuKey ─────────────────────────────────────────────────────────
void MenuSystem::handleMenuKey(char key) {
    bool dirty = false;

    // UP — 'k' or ';' (Fn+, mapped below as UP_KEY sentinel)
    if (key == 'k' || key == KEY_UP) {
        if (_cursor > 0) _cursor--;
        if (_cursor < _scrollOff) _scrollOff = _cursor;
        dirty = true;
    }
    // DOWN — 'j' or KEY_DOWN sentinel
    else if (key == 'j' || key == KEY_DOWN) {
        if (_cursor < _profileCount - 1) _cursor++;
        if (_cursor >= _scrollOff + VISIBLE_ROWS) _scrollOff = _cursor - VISIBLE_ROWS + 1;
        dirty = true;
    }
    // ENTER / SPACE → select
    else if (key == '\n' || key == '\r' || key == ' ') {
        launchWiringGuide(_cursor);
        return;
    }
    // Number shortcut 1-8
    else if (key >= '1' && key <= '8') {
        int idx = key - '1';
        if (idx < _profileCount) launchWiringGuide(idx);
        return;
    }

    if (dirty) renderMenu();
}

// ── launchWiringGuide ─────────────────────────────────────────────────────
void MenuSystem::launchWiringGuide(int idx) {
    _activeIdx = idx;
    _state     = State::WIRING_GUIDE;
    _wiringGuide.reset();
    _wiringGuide.begin(&_configs[idx], _profiles[idx]->name());
}

// ── launchConfigurator ────────────────────────────────────────────────────
void MenuSystem::launchConfigurator(int idx) {
    _state = State::CONFIGURING;
    _configurator.reset();
    _configurator.begin(&_configs[idx], _pm, _profiles[idx]->name());
}

// ── startProfile ──────────────────────────────────────────────────────────
void MenuSystem::startProfile(int idx) {
    _state = State::RUNNING;
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(0x000000);
    _profiles[idx]->onEnter(_pm, &_configs[idx]);
    _profiles[idx]->update(_pm);   // draw immediately — no blank frame
}

// ── returnToMenu ──────────────────────────────────────────────────────────
void MenuSystem::returnToMenu() {
    if (_state == State::RUNNING && _activeIdx >= 0) {
        _profiles[_activeIdx]->onExit(_pm);
    }
    _activeIdx = -1;
    _state     = State::MENU;
    renderMenu();
}

// ── renderMenu ────────────────────────────────────────────────────────────
void MenuSystem::renderMenu() {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(C_BG);

    M5Cardputer.Display.fillRect(0, 0, SCR_W, 18, C_HDR);
    M5Cardputer.Display.setTextColor(C_TITLE);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(4, 4);
    M5Cardputer.Display.print("Cardputer GPIO Lab");

    int rowH   = 18;
    int startY = 22;
    int end    = min(_scrollOff + VISIBLE_ROWS, _profileCount);

    for (int i = _scrollOff; i < end; i++) {
        int y = startY + (i - _scrollOff) * rowH;

        if (i == _cursor) {
            M5Cardputer.Display.fillRect(0, y, SCR_W, rowH - 2, C_SEL);
            M5Cardputer.Display.drawRect(0, y, SCR_W, rowH - 2, C_CURSOR);
            M5Cardputer.Display.setTextColor(C_CURSOR);
        } else {
            M5Cardputer.Display.setTextColor(C_TEXT);
        }

        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setCursor(10, y + 4);
        M5Cardputer.Display.printf("%d. %s", i + 1, _profiles[i]->name());

        // Small "configured" tick if all pins are assigned
        if (_configs[i].isComplete()) {
            M5Cardputer.Display.setTextColor(0x00aa44u);
            M5Cardputer.Display.setCursor(SCR_W - 12, y + 4);
            M5Cardputer.Display.print("*");
        }
    }

    if (_scrollOff > 0) {
        M5Cardputer.Display.setTextColor(C_HINT);
        M5Cardputer.Display.setCursor(SCR_W - 12, 20);
        M5Cardputer.Display.print("^");
    }
    if (_scrollOff + VISIBLE_ROWS < _profileCount) {
        M5Cardputer.Display.setTextColor(C_HINT);
        M5Cardputer.Display.setCursor(SCR_W - 12, SCR_H - 12);
        M5Cardputer.Display.print("v");
    }

    M5Cardputer.Display.setTextColor(C_HINT);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(4, SCR_H - 10);
    M5Cardputer.Display.print("[j/k] Navigate  [ENT] Select  * = configured");
}

// ── pollKey ───────────────────────────────────────────────────────────────
// FIX: Use M5Cardputer.Keyboard (not M5.Keyboard).
// NOTE: M5Cardputer.update() must be called before this each loop.
//
// Special key mapping (Cardputer has no dedicated arrow/ESC keys):
//   Fn + ;   → KEY_UP   (sentinel 0x11)
//   Fn + .   → KEY_DOWN (sentinel 0x12)
//   Fn + ,   → KEY_LEFT (sentinel 0x13)
//   Fn + /   → KEY_RIGHT(sentinel 0x14)
//   DEL key  → 0x08 (backspace / "ESC" substitute)
//
char MenuSystem::pollKey() {
    auto& kbd = M5Cardputer.Keyboard;

    // isChange() fires on press AND release — only act on press
    if (!kbd.isChange() || !kbd.isPressed()) return 0;

    Keyboard_Class::KeysState status = kbd.keysState();

    // ── Special keys ─────────────────────────────────────────────────
    if (status.enter) return '\n';
    if (status.del)   return 8;   // Backspace key — no Fn needed on Cardputer

    // ── Fn + nav key → direction sentinels ───────────────────────────
    if (status.fn) {
        for (char ch : status.word) {
            switch (ch) {
                case ';': return KEY_UP;
                case '.': return KEY_DOWN;
                case ',': return KEY_LEFT;
                case '/': return KEY_RIGHT;
                default: break;
            }
        }
        // Fn alone or unrecognised combo — ignore
        return 0;
    }

    // ── Regular printable key ─────────────────────────────────────────
    for (char ch : status.word) {
        if (ch != 0) return ch;
    }

    return 0;
}
