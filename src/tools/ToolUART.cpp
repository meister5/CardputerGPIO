#include "ToolUART.h"
#include <string.h>

namespace cg {

ToolUART toolUART;

static const Role ROLES[] = {
    { "TX", RoleDir::Out, "-> RX on your device", 13 },
    { "RX", RoleDir::In,  "<- TX on your device", 15 },
};
const Role* ToolUART::roles() const { return ROLES; }

static const char* HELP[] = {
    "Type, then ENTER to send the line.",
    "",
    "  B      baud rate",
    "  H      hex view on / off",
    "  W      swap TX and RX",
    "  U      USB bridge on / off",
    "  L      send CRLF or LF",
    "  Fn+`   clear the screen",
    "",
    "Levels are 3.3 V. Do not wire this to an",
    "RS-232 port without a transceiver -- RS-232",
    "swings +/-12 V and will destroy the pin.",
    "",
    "Bridge mode copies bytes both ways",
    "between USB serial and the header.",
};
const char* const* ToolUART::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

static const uint32_t BAUDS[] = {
    1200, 2400, 4800, 9600, 19200, 115200, 230400, 460800, 921600
};
static constexpr int BAUD_N = (int)(sizeof(BAUDS) / sizeof(BAUDS[0]));
uint32_t ToolUART::baud(uint8_t i) { return BAUDS[i % BAUD_N]; }

static HardwareSerial Port(1);

void ToolUART::clearScreen() {
    memset(_scr, 0, sizeof(_scr));
    _row = _col = 0;
}

void ToolUART::openPort() {
    closePort();
    int tx = _swap ? pin(1) : pin(0);
    int rx = _swap ? pin(0) : pin(1);
    if (tx < 0 || rx < 0) { ui.notify("assign TX and RX"); return; }

    pins.claimBus(tx);
    pins.claimBus(rx);
    Port.begin(baud(_baudIdx), SERIAL_8N1, rx, tx);
    _open = true;
}

void ToolUART::closePort() {
    if (_open) Port.end();
    _open = false;
}

void ToolUART::onEnter() {
    clearScreen();
    _rxCount = _txCount = 0;
    _inLen   = 0;
    _input[0] = 0;
    openPort();
}

void ToolUART::onExit() {
    closePort();
    if (pin(0) >= 0) pins.release(pin(0));
    if (pin(1) >= 0) pins.release(pin(1));
}

void ToolUART::newline() {
    _col = 0;
    _row = (_row + 1) % ROWS;
    memset(_scr[_row], 0, COLS + 1);
}

void ToolUART::putChar(char c) {
    if (c == '\n') { newline(); return; }
    if (c == '\r') return;
    if (c < 32 || c > 126) c = '.';
    if (_col >= COLS) newline();
    _scr[_row][_col++] = c;
}

void ToolUART::putByte(uint8_t b) {
    if (!_hex) { putChar((char)b); return; }
    char t[4];
    snprintf(t, sizeof(t), "%02X ", b);
    for (int i = 0; i < 3; i++) {
        if (_col >= COLS) newline();
        _scr[_row][_col++] = t[i];
    }
}

void ToolUART::tick() {
    if (!_open) return;

    int guard = 0;
    while (Port.available() && guard++ < 256) {
        uint8_t b = (uint8_t)Port.read();
        _rxCount++;
        putByte(b);
        if (_bridge) Serial.write(b);
    }

    if (_bridge) {
        guard = 0;
        while (Serial.available() && guard++ < 256) {
            uint8_t b = (uint8_t)Serial.read();
            Port.write(b);
            _txCount++;
        }
    }
}

void ToolUART::sendLine() {
    if (!_open) { ui.notify("port not open"); return; }
    Port.write((const uint8_t*)_input, _inLen);
    if (_crlf) Port.write((const uint8_t*)"\r\n", 2);
    else       Port.write('\n');
    _txCount += _inLen + (_crlf ? 2 : 1);

    // Echo what we sent so the log reads as a conversation.
    for (int i = 0; i < _inLen; i++) putChar(_input[i]);
    newline();

    _inLen = 0;
    _input[0] = 0;
}

bool ToolUART::onKey(const KeyEvent& ev) {
    switch (ev.key) {
        case Key::Enter: sendLine(); return true;
        case Key::Back:
            if (_inLen > 0) { _input[--_inLen] = 0; return true; }
            return false;                       // empty line: let the shell exit
        case Key::Esc:   clearScreen(); return true;
        case Key::Char:
            // Control shortcuts first, so they are not typed into the buffer.
            if (ev.ctrl) {
                if (ev.ci('b')) { _baudIdx = (uint8_t)((_baudIdx + 1) % BAUD_N); openPort();
                                  ui.notify("%lu baud", (unsigned long)baud(_baudIdx)); return true; }
                return true;
            }
            if (ev.alt) {
                if (ev.ci('h')) { _hex = !_hex; ui.notify(_hex ? "hex" : "ascii"); return true; }
                if (ev.ci('w')) { _swap = !_swap; openPort(); ui.notify(_swap ? "TX/RX swapped" : "TX/RX normal"); return true; }
                if (ev.ci('u')) { _bridge = !_bridge; ui.notify(_bridge ? "USB bridge on" : "bridge off"); return true; }
                if (ev.ci('l')) { _crlf = !_crlf; ui.notify(_crlf ? "CRLF" : "LF"); return true; }
                if (ev.ci('b')) { _baudIdx = (uint8_t)((_baudIdx + 1) % BAUD_N); openPort();
                                  ui.notify("%lu baud", (unsigned long)baud(_baudIdx)); return true; }
                return true;
            }
            if (ev.ch >= ' ' && ev.ch <= '~' && _inLen < (int)sizeof(_input) - 1) {
                _input[_inLen++] = ev.ch;
                _input[_inLen]   = 0;
            }
            return true;
        case Key::Fkey:
            switch (ev.num) {
                case 2: _hex    = !_hex;    ui.notify(_hex ? "hex" : "ascii"); return true;
                case 3: _swap   = !_swap;   openPort(); ui.notify(_swap ? "swapped" : "normal"); return true;
                case 4: _bridge = !_bridge; ui.notify(_bridge ? "bridge on" : "bridge off"); return true;
                case 5: _baudIdx = (uint8_t)((_baudIdx + 1) % BAUD_N); openPort();
                        ui.notify("%lu baud", (unsigned long)baud(_baudIdx)); return true;
                default: return true;
            }
        default:
            return false;
    }
}

void ToolUART::draw() {
    char right[24];
    snprintf(right, sizeof(right), "%lu%s%s", (unsigned long)baud(_baudIdx),
             _hex ? " hex" : "", _bridge ? " USB" : "");
    ui.header("UART Terminal", right, _open ? C_HDR : C_FTR);

    // Oldest line first, wrapping around the ring.
    int y = BODY_Y + 1;
    for (int i = 1; i <= ROWS; i++) {
        int r = (_row + i) % ROWS;
        ui.textf(3, y, C_TEXT, "%.39s", _scr[r]);
        y += 9;
    }

    // Input line, pinned above the footer.
    int iy = BODY_B - 10;
    ui.g().fillRect(0, iy - 2, SCR_W, 11, C_PANEL);
    ui.textf(3, iy, C_HIGH, "> %.34s_", _input);

    if (!_open) ui.textRight(SCR_W - 4, BODY_Y + 1, C_LOW, "CLOSED");

    ui.footerf("F2 hex F3 swap F4 usb F5 baud  rx%lu tx%lu",
               (unsigned long)_rxCount, (unsigned long)_txCount);
}

}  // namespace cg
