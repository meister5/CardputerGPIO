/**
 * Keys.h — keyboard event layer for the Cardputer ADV.
 *
 * M5Cardputer 1.1.1 gives you a resolved snapshot of which keys are down and
 * nothing else. Everything an application actually needs it does not do:
 *
 *   - There is no ESC key on this keyboard. The top-left key is backtick;
 *     ESC is Fn+`.
 *   - Arrows are Fn + ; , . /  (as printed on the keycaps). The library only
 *     tells you Fn is held and hands you the *base* character.
 *   - Fn+Shift+; hands you ':' rather than ';', so the Fn layer has to
 *     un-shift before matching.
 *   - Ctrl forces the shifted glyph into `word`, so Ctrl+C arrives as 'C' and
 *     a naive `ch == 'c'` test never fires.
 *   - Caps Lock is not a toggle; Aa is momentary and the app owns the state.
 *   - There is no auto-repeat, so holding an arrow moves the cursor once.
 *
 * This class turns all of that into a queue of KeyEvents with a single
 * meaning each, so tools never touch KeysState.
 *
 * Note: newer (unreleased) M5Cardputer revisions resolve the Fn layer in the
 * library via a third keymap column. We deliberately do not depend on that --
 * this file targets 1.1.1, which is what Library Manager installs.
 */

#pragma once
#include <M5Cardputer.h>
#include <stdint.h>

namespace cg {

enum class Key : uint8_t {
    None = 0,
    Char,       // printable; see KeyEvent::ch
    Up, Down, Left, Right,
    Enter,
    Back,       // Backspace / DEL — the universal "go back"
    Esc,        // Fn + `
    FwdDel,     // Fn + Backspace
    Tab,
    Fkey,       // F1..F10; see KeyEvent::num
};

struct KeyEvent {
    Key     key    = Key::None;
    char    ch     = 0;      // Key::Char — Ctrl-combos are normalised to lower case
    uint8_t num    = 0;      // Key::Fkey — 1..10
    bool    ctrl   = false;
    bool    shift  = false;
    bool    alt    = false;
    bool    opt    = false;
    bool    repeat = false;  // synthesised by auto-repeat rather than a fresh press

    explicit operator bool() const { return key != Key::None; }

    bool is(Key k)  const { return key == k; }
    bool is(char c) const { return key == Key::Char && ch == c; }

    // Case-insensitive character test — the common case for tool shortcuts.
    bool ci(char c) const {
        if (key != Key::Char) return false;
        char a = ch,  b = c;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        return a == b;
    }

    // Digit shortcut helper: returns 0..9, or -1 when this is not a digit.
    int digit() const {
        return (key == Key::Char && ch >= '0' && ch <= '9') ? (ch - '0') : -1;
    }
};

class Keys {
public:
    void begin();

    // Call once per loop, after M5Cardputer.update().
    void update();

    // Pop one event. Returns false when the queue is empty.
    bool next(KeyEvent& out);

    // Push an event from somewhere that is not the physical keyboard -- the
    // web interface uses this, so a browser drives exactly the same code
    // path as the keys on the case.
    void inject(const KeyEvent& ev);

    // Drop anything queued — used when switching screens so a held key does
    // not leak its repeats into the screen you just opened.
    void flush();

    bool capsLock() const { return _caps; }

    // Auto-repeat tuning (milliseconds).
    void setRepeat(uint16_t delayMs, uint16_t rateMs) {
        _repDelay = delayMs;
        _repRate  = rateMs;
    }

private:
    static constexpr int QMAX = 12;

    KeyEvent _q[QMAX];
    uint8_t  _qHead = 0, _qTail = 0;

    // A "logical key" is the thing the user pressed after the Fn layer has
    // been applied: Fn+; is one logical key (Up), not two.
    static constexpr int LMAX = 10;
    uint16_t _prev[LMAX];  int _prevN = 0;
    uint16_t _cur [LMAX];  int _curN  = 0;

    uint16_t _repCode  = 0;
    uint32_t _repNext  = 0;
    bool     _repArmed = false;
    uint16_t _repDelay = 380;
    uint16_t _repRate  = 55;

    bool _caps      = false;
    bool _capsLatch = false;   // edge-detect for the Fn+Shift toggle

    void push(const KeyEvent& ev);
    void emitCode(uint16_t code, bool repeat);
    bool inPrev(uint16_t code) const;
    static bool repeatable(uint16_t code);

    // Current modifier snapshot, refreshed each update().
    bool _mCtrl = false, _mShift = false, _mAlt = false, _mOpt = false;
};

extern Keys keys;

}  // namespace cg
