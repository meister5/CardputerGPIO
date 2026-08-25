#include "Keys.h"

namespace cg {

Keys keys;

// ── Logical key codes ─────────────────────────────────────────────────────
// High byte tags the family so a char and a special key can never collide.
static constexpr uint16_t LC_CHAR = 0x0100;   // | ascii
static constexpr uint16_t LC_SPEC = 0x0200;   // | Key enum
static constexpr uint16_t LC_FKEY = 0x0300;   // | 1..10

static inline uint16_t codeChar(char c)  { return LC_CHAR | (uint8_t)c; }
static inline uint16_t codeSpec(Key k)   { return LC_SPEC | (uint8_t)k; }
static inline uint16_t codeFkey(int n)   { return LC_FKEY | (uint8_t)n; }

// Map a shifted glyph back to its base-layer key. The Fn layer is matched on
// base keys, but the library resolves Shift before we ever see the character,
// so Fn+Shift+; arrives as ':' and would otherwise miss.
static char unshift(char c) {
    switch (c) {
        case '~': return '`';   case '!': return '1';   case '@': return '2';
        case '#': return '3';   case '$': return '4';   case '%': return '5';
        case '^': return '6';   case '&': return '7';   case '*': return '8';
        case '(': return '9';   case ')': return '0';   case '_': return '-';
        case '+': return '=';   case '{': return '[';   case '}': return ']';
        case '|': return '\\';  case ':': return ';';   case '"': return '\'';
        case '<': return ',';   case '>': return '.';   case '?': return '/';
        default:
            if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
            return c;
    }
}

void Keys::begin() {
    flush();
    _prevN = _curN = 0;
    _caps  = M5Cardputer.Keyboard.capslocked();
    _lastPhys = millis();
}

void Keys::inject(const KeyEvent& ev) { push(ev); }

void Keys::flush() {
    _qHead = _qTail = 0;
    _repArmed = false;
    _repCode  = 0;
}

void Keys::push(const KeyEvent& ev) {
    uint8_t nxt = (uint8_t)((_qTail + 1) % QMAX);
    if (nxt == _qHead) return;   // full: drop the newest rather than stall
    _q[_qTail] = ev;
    _qTail     = nxt;
}

bool Keys::next(KeyEvent& out) {
    if (_qHead == _qTail) return false;
    out    = _q[_qHead];
    _qHead = (uint8_t)((_qHead + 1) % QMAX);
    return true;
}

bool Keys::inPrev(uint16_t code) const {
    for (int i = 0; i < _prevN; i++)
        if (_prev[i] == code) return true;
    return false;
}

// Repeating Enter/Esc/Back would fire a screen transition several times from
// one press, so only navigation and printable keys repeat.
bool Keys::repeatable(uint16_t code) {
    if ((code & 0xFF00) == LC_CHAR) return true;
    if ((code & 0xFF00) == LC_SPEC) {
        Key k = (Key)(code & 0xFF);
        return k == Key::Up || k == Key::Down || k == Key::Left || k == Key::Right;
    }
    return false;
}

void Keys::emitCode(uint16_t code, bool repeat) {
    KeyEvent ev;
    ev.ctrl   = _mCtrl;
    ev.shift  = _mShift;
    ev.alt    = _mAlt;
    ev.opt    = _mOpt;
    ev.repeat = repeat;
    ev.phys   = true;      // emitCode() is only ever reached from update()

    switch (code & 0xFF00) {
        case LC_CHAR: {
            ev.key = Key::Char;
            char c = (char)(code & 0xFF);
            // Ctrl forces value_second in the library, so Ctrl+C reaches us as
            // 'C'. Normalise it back so tools can test lower case.
            if (_mCtrl) c = unshift(c);
            ev.ch = c;
            break;
        }
        case LC_SPEC:
            ev.key = (Key)(code & 0xFF);
            break;
        case LC_FKEY:
            ev.key = Key::Fkey;
            ev.num = (uint8_t)(code & 0xFF);
            break;
        default:
            return;
    }
    push(ev);
}

void Keys::update() {
    auto& kbd = M5Cardputer.Keyboard;
    Keyboard_Class::KeysState st = kbd.keysState();

    _mCtrl  = st.ctrl;
    _mShift = st.shift;
    _mAlt   = st.alt;
    _mOpt   = st.opt;

    // ── Caps Lock: Fn+Shift, edge-triggered ───────────────────────────────
    bool capsCombo = st.fn && st.shift;
    if (capsCombo && !_capsLatch) {
        _caps = !_caps;
        kbd.setCapsLocked(_caps);
    }
    _capsLatch = capsCombo;

    // ── Build this frame's set of logical keys ────────────────────────────
    _curN = 0;
    auto add = [&](uint16_t c) {
        if (_curN >= LMAX) return;
        for (int i = 0; i < _curN; i++) if (_cur[i] == c) return;
        _cur[_curN++] = c;
    };

    if (st.fn) {
        // Fn layer. Nothing here produces text, so the base characters the
        // library handed us are consumed rather than inserted.
        for (char raw : st.word) {
            char c = unshift(raw);
            switch (c) {
                case ';': add(codeSpec(Key::Up));    break;
                case '.': add(codeSpec(Key::Down));  break;
                case ',': add(codeSpec(Key::Left));  break;
                case '/': add(codeSpec(Key::Right)); break;
                case '`': add(codeSpec(Key::Esc));   break;
                default:
                    if (c >= '1' && c <= '9') add(codeFkey(c - '0'));
                    else if (c == '0')        add(codeFkey(10));
                    break;
            }
        }
        // Fn+Backspace is forward delete. The library still reports del=true
        // and the plain backspace HID code, so we have to disambiguate here.
        if (st.del) add(codeSpec(Key::FwdDel));
    } else {
        if (st.enter) add(codeSpec(Key::Enter));
        if (st.del)   add(codeSpec(Key::Back));
        if (st.tab)   add(codeSpec(Key::Tab));
        for (char c : st.word) {
            if (c == 0) continue;
            // The four keys that carry an arrow on the keycap navigate
            // without Fn, and the top-left key goes back.
            //
            // Shift is what opts out -- Shift+; is ':', and that is how those
            // five characters are typed on a screen that is not a text field.
            // Held Shift is also the only reliable way to tell the case
            // apart, because the library hands out value_second whenever
            // *any* of ctrl, shift or caps lock is set: with caps on, ';'
            // arrives as ':' with nothing else to distinguish it. Hence
            // unshift() here, and hence Ctrl being excluded outright -- a
            // Ctrl combination is a shortcut, not a cursor move.
            if (!_textInput && !_mShift && !_mCtrl) {
                switch (unshift(c)) {
                    case ';': add(codeSpec(Key::Up));    continue;
                    case '.': add(codeSpec(Key::Down));  continue;
                    case ',': add(codeSpec(Key::Left));  continue;
                    case '/': add(codeSpec(Key::Right)); continue;
                    case '`': add(codeSpec(Key::Back));  continue;
                    default:  break;
                }
            }
            add(codeChar(c));
        }
    }

    // ── Emit presses: anything present now that was not present last frame ─
    uint16_t newest = 0;
    for (int i = 0; i < _curN; i++) {
        if (!inPrev(_cur[i])) {
            emitCode(_cur[i], false);
            newest = _cur[i];
        }
    }

    uint32_t now = millis();

    // Anything held down is a sign someone is looking at the screen; the idle
    // timeout is measured from here. Held counts as well as pressed, so a key
    // leant on for a minute does not let the display go dark under the thumb.
    if (_curN > 0) _lastPhys = now;

    if (newest && repeatable(newest)) {
        _repCode  = newest;
        _repArmed = true;
        _repNext  = now + _repDelay;
    }

    // ── Auto-repeat: the held key must still be down ──────────────────────
    if (_repArmed) {
        bool stillDown = false;
        for (int i = 0; i < _curN; i++)
            if (_cur[i] == _repCode) { stillDown = true; break; }

        if (!stillDown) {
            _repArmed = false;
        } else if ((int32_t)(now - _repNext) >= 0) {
            emitCode(_repCode, true);
            _repNext = now + _repRate;
        }
    }

    // ── Roll the frame ────────────────────────────────────────────────────
    _prevN = _curN;
    for (int i = 0; i < _curN; i++) _prev[i] = _cur[i];
}

}  // namespace cg
