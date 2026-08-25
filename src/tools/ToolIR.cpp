#include "ToolIR.h"

namespace cg {

ToolIR toolIR;

static const char* HELP[] = {
    "The emitter is on G44, inside the case.",
    "Aim the top edge at the target.",
    "",
    "  P      protocol",
    "  TAB    switch address / command field",
    "  < >    value -1 / +1",
    "  ^ v    value -16 / +16",
    "  0-9 a-f  type a hex digit",
    "  SPACE  transmit",
    "",
    "NEC sends address, ~address, command,",
    "~command -- the inverted bytes are the",
    "receiver's error check, added for you.",
    "",
    "Range is a couple of metres: the onboard",
    "LED is driven for signalling, not power.",
};
const char* const* ToolIR::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

const char* ToolIR::protoName(Proto p) {
    switch (p) {
        case Proto::NEC:    return "NEC";
        case Proto::Sony12: return "Sony 12";
        case Proto::RC5:    return "RC5";
        default:            return "?";
    }
}

static inline void mark(rmt_data_t& d, uint16_t on, uint16_t off) {
    d.level0    = 1;  d.duration0 = on;
    d.level1    = 0;  d.duration1 = off;
}

bool ToolIR::initRmt() {
    // 1 MHz tick: one unit per microsecond, which is how IR timings are
    // specified everywhere.
    if (!rmtInit(PIN_IR_TX, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_2, 1000000)) return false;
    rmtSetEOT(PIN_IR_TX, LOW);
    return true;
}

int ToolIR::buildNEC(rmt_data_t* out) {
    int n = 0;
    out[n].level0 = 1; out[n].duration0 = 9000;
    out[n].level1 = 0; out[n].duration1 = 4500;
    n++;

    uint8_t bytes[4] = {
        (uint8_t)(_addr & 0xFF), (uint8_t)(~_addr & 0xFF),
        (uint8_t)(_cmd  & 0xFF), (uint8_t)(~_cmd  & 0xFF)
    };
    for (int b = 0; b < 4; b++)
        for (int i = 0; i < 8; i++) {          // LSB first
            bool one = (bytes[b] >> i) & 1;
            mark(out[n++], 560, one ? 1690 : 560);
        }

    mark(out[n++], 560, 0);                    // stop bit
    return n;
}

int ToolIR::buildSony(rmt_data_t* out) {
    int n = 0;
    out[n].level0 = 1; out[n].duration0 = 2400;
    out[n].level1 = 0; out[n].duration1 = 600;
    n++;

    for (int i = 0; i < 7; i++) {              // 7 command bits, LSB first
        bool one = (_cmd >> i) & 1;
        mark(out[n++], one ? 1200 : 600, 600);
    }
    for (int i = 0; i < 5; i++) {              // 5 address bits
        bool one = (_addr >> i) & 1;
        mark(out[n++], one ? 1200 : 600, 600);
    }
    return n;
}

int ToolIR::buildRC5(rmt_data_t* out) {
    // Manchester: a 1 is space-then-mark, a 0 is mark-then-space.
    static bool toggle = false;
    toggle = !toggle;

    uint16_t frame = (uint16_t)((1 << 13) | ((toggle ? 1 : 0) << 11) |
                                ((_addr & 0x1F) << 6) | (_cmd & 0x3F));
    int n = 0;
    for (int i = 13; i >= 0; i--) {
        bool one = (frame >> i) & 1;
        if (one) { out[n].level0 = 0; out[n].duration0 = 889;
                   out[n].level1 = 1; out[n].duration1 = 889; }
        else     { out[n].level0 = 1; out[n].duration0 = 889;
                   out[n].level1 = 0; out[n].duration1 = 889; }
        n++;
    }
    return n;
}

void ToolIR::send() {
    if (!_ready) { ui.notify("RMT unavailable"); return; }

    rmt_data_t sym[80];
    int n = 0;
    switch (_proto) {
        case Proto::NEC:    n = buildNEC(sym);  break;
        case Proto::Sony12: n = buildSony(sym); break;
        case Proto::RC5:    n = buildRC5(sym);  break;
        default: return;
    }

    // The carrier is only applied to high levels, so the gaps stay dark.
    float duty = (_proto == Proto::Sony12) ? 33.0f : 33.0f;
    uint32_t carrier = (_proto == Proto::RC5) ? 36000 : 38000;
    rmtSetCarrier(PIN_IR_TX, true, false, carrier, duty);

    rmtWrite(PIN_IR_TX, sym, n, 200);
    _sent++;
    _lastSend = millis();
    ui.beep(3200, 12);
}

void ToolIR::onEnter() {
    _ready = initRmt();
    _sent  = 0;
    if (!_ready) ui.notify("could not claim RMT");
}

void ToolIR::onExit() {
    rmtDeinit(PIN_IR_TX);
    _ready = false;
}

void ToolIR::adjust(int delta) {
    uint16_t& v = (_field == Field::Addr) ? _addr : _cmd;
    int nv = (int)v + delta;
    int maxV = (_proto == Proto::NEC) ? 0xFF
             : (_field == Field::Addr ? 0x1F : 0x3F);
    if (nv < 0)     nv = maxV;
    if (nv > maxV)  nv = 0;
    v = (uint16_t)nv;
}

bool ToolIR::onKey(const KeyEvent& ev) {
    switch (ev.key) {
        case Key::Left:  adjust(-1);  return true;
        case Key::Right: adjust(+1);  return true;
        case Key::Up:    adjust(+16); return true;
        case Key::Down:  adjust(-16); return true;
        case Key::Tab:
            _field = (Field)(((int)_field + 1) % (int)Field::COUNT);
            return true;
        case Key::Enter: send(); return true;
        case Key::Char: {
            if (ev.ch == ' ') { send(); return true; }
            if (ev.ci('p')) {
                _proto = (Proto)(((int)_proto + 1) % (int)Proto::COUNT);
                ui.notify("%s", protoName(_proto));
                return true;
            }
            // Hex digits shift into the active field.
            char c = ev.ch;
            if (c >= 'A' && c <= 'F') c = (char)(c - 'A' + 'a');
            int d = -1;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            if (d >= 0) {
                uint16_t& v = (_field == Field::Addr) ? _addr : _cmd;
                v = (uint16_t)(((v << 4) | d) & 0xFF);
                return true;
            }
            return false;
        }
        default:
            return false;
    }
}

void ToolIR::draw() {
    ui.header("IR Transmitter", protoName(_proto), C_HDR);

    ui.text(6, BODY_Y + 2, C_DIM, "on-board emitter G44");

    bool addrSel = (_field == Field::Addr);

    ui.text(14, BODY_Y + 16, addrSel ? C_TITLE : C_DIM, "address");
    ui.textBigf(14, BODY_Y + 26, addrSel ? C_HIGH : C_TEXT, 3, "%02X", _addr);

    ui.text(130, BODY_Y + 16, !addrSel ? C_TITLE : C_DIM, "command");
    ui.textBigf(130, BODY_Y + 26, !addrSel ? C_HIGH : C_TEXT, 3, "%02X", _cmd);

    // Underline whichever field the keys will change.
    int ux = addrSel ? 14 : 130;
    ui.g().fillRect(ux, BODY_Y + 50, 36, 2, C_TITLE);

    if (millis() - _lastSend < 350) {
        ui.g().fillRoundRect(180, BODY_Y + 2, 52, 12, 3, C_HIGH);
        ui.text(186, BODY_Y + 4, C_BLACK, "SENT");
    }

    ui.textf(6, BODY_B - 9, C_FAINT, "%lu frame(s) sent  %s",
             (unsigned long)_sent, _ready ? "" : "RMT FAILED");

    ui.footer("[SPC]send [TAB]field [<>]value");
}

}  // namespace cg
