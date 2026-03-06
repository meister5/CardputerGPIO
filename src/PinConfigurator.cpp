/**
 * PinConfigurator.cpp  (fixed)
 * Interactive pin assignment UI implementation.
 *
 * Fixes:
 *  - Left/right navigation uses KEY_LEFT/KEY_RIGHT sentinels from pollKey()
 *    instead of raw ASCII 2/5 (which don't exist on Cardputer) or 'd'/'c'
 *    (which clash with letter input).
 *  - ESC/cancel uses DEL (key==8) since Cardputer has no physical ESC key.
 *  - All M5.Display → M5Cardputer.Display
 */

#include "PinConfigurator.h"

// ── begin ─────────────────────────────────────────────────────────────────
void PinConfigurator::begin(PinConfig* cfg, const PinManager* pm,
                            const char* profileName, int startRole)
{
    _cfg         = cfg;
    _pm          = pm;
    _done        = false;
    _cancelled   = false;
    _currentRole = startRole;
    strncpy(_profileName, profileName, sizeof(_profileName) - 1);

    buildCandidates();
    _dirty = true;
}

// ── buildCandidates ───────────────────────────────────────────────────────
void PinConfigurator::buildCandidates() {
    _candidates.clear();
    if (!_cfg || !_pm) return;

    PinDir dir = _cfg->role(_currentRole).dir;
    const std::vector<int>& pool =
        (dir == PinDir::ADC_ROLE) ? _pm->adcPins() : _pm->safePins();

    for (int p : pool) _candidates.push_back(p);

    // Position cursor on currently-assigned pin (if valid)
    int assigned = _cfg->pin(_currentRole);
    _pinCursor = 0;
    for (int i = 0; i < (int)_candidates.size(); i++) {
        if (_candidates[i] == assigned) { _pinCursor = i; break; }
    }
}

// ── update ────────────────────────────────────────────────────────────────
void PinConfigurator::update() {
    if (_done || _cancelled) return;
    if (_dirty) { draw(); _dirty = false; }
}

// ── onKey ─────────────────────────────────────────────────────────────────
void PinConfigurator::onKey(char key) {
    if (_done || _cancelled || !_cfg) return;

    // FIX: DEL (backspace) = cancel. Cardputer has no ESC key.
    if (key == 8) {
        _cancelled = true;
        return;
    }

    // FIX: Use KEY_LEFT / KEY_RIGHT sentinels (Fn+, / Fn+/) from pollKey()
    // Removed conflicting 'd'/'c' and bogus raw-byte codes 2/5.
    if (key == KEY_LEFT) {
        if (_pinCursor > 0) { _pinCursor--; _dirty = true; }
        return;
    }

    if (key == KEY_RIGHT) {
        if (_pinCursor < (int)_candidates.size() - 1) { _pinCursor++; _dirty = true; }
        return;
    }

    if (key == '\n' || key == '\r' || key == ' ') {
        confirmRole();
        return;
    }

    // Number shortcut: pressing a digit jumps to that index (0-indexed from '1')
    if (key >= '1' && key <= '9') {
        int idx = key - '1';
        if (idx < (int)_candidates.size()) { _pinCursor = idx; _dirty = true; }
        return;
    }
}

// ── confirmRole ───────────────────────────────────────────────────────────
void PinConfigurator::confirmRole() {
    if (_candidates.empty()) return;

    int chosen = _candidates[_pinCursor];
    _cfg->setPin(_currentRole, chosen);
    Serial.printf("[Configurator] Role %d '%s' → GPIO%d\n",
                  _currentRole, _cfg->role(_currentRole).label, chosen);

    _currentRole++;
    if (_currentRole >= _cfg->roleCount()) {
        _cfg->save(_cfg->profileId());
        _done = true;
        return;
    }

    buildCandidates();
    _dirty = true;
}

// ── draw ──────────────────────────────────────────────────────────────────
void PinConfigurator::draw() {
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(C_BG);

    // ── Header ────────────────────────────────────────────────────────
    M5Cardputer.Display.fillRect(0, 0, SCR_W, 18, C_HDR);
    M5Cardputer.Display.setTextColor(C_TITLE);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(4, 4);
    M5Cardputer.Display.print("Configure Pins");

    // Profile name (right side of header)
    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.setCursor(SCR_W - 80, 4);
    char pname[16];
    strncpy(pname, _profileName, 15); pname[15] = '\0';
    M5Cardputer.Display.print(pname);

    // ── Role info ─────────────────────────────────────────────────────
    const PinRole& role = _cfg->role(_currentRole);

    M5Cardputer.Display.setTextColor(C_TEXT);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(4, 22);
    M5Cardputer.Display.printf("Step %d of %d", _currentRole + 1, _cfg->roleCount());

    // Role label (large)
    M5Cardputer.Display.setTextColor(C_SEL);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setCursor(4, 34);
    M5Cardputer.Display.print(role.label);

    // Direction badge
    M5Cardputer.Display.setTextSize(1);
    const char* dirStr = "---";
    uint32_t dirCol = C_DIM;
    switch (role.dir) {
        case PinDir::OUTPUT_ROLE: dirStr = " OUT "; dirCol = 0x00aa44u; break;
        case PinDir::INPUT_ROLE:  dirStr = " IN  "; dirCol = 0x0055ffu; break;
        case PinDir::ADC_ROLE:    dirStr = " ADC "; dirCol = 0xffaa00u; break;
        case PinDir::PWM_ROLE:    dirStr = " PWM "; dirCol = 0xaa00ffu; break;
        case PinDir::EITHER:      dirStr = " I/O "; dirCol = C_DIM;     break;
    }
    M5Cardputer.Display.fillRoundRect(SCR_W - 44, 34, 40, 14, 3, dirCol);
    M5Cardputer.Display.setTextColor(0x000000u);
    M5Cardputer.Display.setCursor(SCR_W - 41, 38);
    M5Cardputer.Display.print(dirStr);

    // Hint text
    M5Cardputer.Display.setTextColor(C_HINT);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(4, 54);
    M5Cardputer.Display.print(role.hint);

    // ── Pin strip ─────────────────────────────────────────────────────
    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.setCursor(4, 68);
    M5Cardputer.Display.print("GPIO:");

    int stripX = 40;
    int stripY = 66;
    int cellW  = 22;
    int cellH  = 14;
    int maxVisible = (SCR_W - stripX - 4) / cellW;

    // Scroll window so cursor is always visible
    int windowStart = max(0, _pinCursor - maxVisible / 2);
    windowStart = min(windowStart, max(0, (int)_candidates.size() - maxVisible));

    for (int i = 0; i < maxVisible && (windowStart + i) < (int)_candidates.size(); i++) {
        int idx  = windowStart + i;
        int pin  = _candidates[idx];
        int cx   = stripX + i * cellW;
        bool sel = (idx == _pinCursor);

        M5Cardputer.Display.fillRect(cx, stripY, cellW - 2, cellH,
            sel ? (uint32_t)C_SEL : (uint32_t)0x1a1a2eu);
        M5Cardputer.Display.setTextColor(sel ? 0x000000u : (uint32_t)C_TEXT);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setCursor(cx + 2, stripY + 3);
        M5Cardputer.Display.printf("%2d", pin);
    }

    // Scroll arrows
    if (windowStart > 0) {
        M5Cardputer.Display.setTextColor(C_WARN);
        M5Cardputer.Display.setCursor(stripX - 8, stripY + 3);
        M5Cardputer.Display.print("<");
    }
    if (windowStart + maxVisible < (int)_candidates.size()) {
        M5Cardputer.Display.setTextColor(C_WARN);
        M5Cardputer.Display.setCursor(SCR_W - 8, stripY + 3);
        M5Cardputer.Display.print(">");
    }

    // ── Large selected GPIO display ───────────────────────────────────
    if (!_candidates.empty()) {
        int selPin = _candidates[_pinCursor];
        M5Cardputer.Display.setTextSize(2);
        M5Cardputer.Display.setTextColor(C_SEL);
        M5Cardputer.Display.setCursor(4, 86);
        M5Cardputer.Display.printf("[ GPIO %2d ]", selPin);
    }

    // ── Footer hint ───────────────────────────────────────────────────
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(C_DIM);
    M5Cardputer.Display.setCursor(4, SCR_H - 10);
    M5Cardputer.Display.print("[Fn+,/>] pick  [1-9] jump  [ENT] ok  [DEL] cancel");
}
