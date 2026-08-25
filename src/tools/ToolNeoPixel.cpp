#include "ToolNeoPixel.h"

namespace cg {

ToolNeoPixel toolNeoPixel;

static const Role ROLES[] = {
    { "DATA", RoleDir::Out, "strip DIN (first pixel)", -1 },
};
const Role* ToolNeoPixel::roles() const { return ROLES; }

static const char* HELP[] = {
    "Wire: strip GND -> GND, strip 5V -> 5V,",
    "strip DIN -> the DATA pin. Use the pad",
    "marked IN, not OUT.",
    "",
    "  ^ v    choose a setting",
    "  < >    change it",
    "  G      GRB / RGB byte order",
    "  SPACE  blackout",
    "",
    "Brightness is capped in software because",
    "60 pixels at full white is about 3.6 A --",
    "far past what the Cardputer's 5V pin can",
    "supply. Above ~20 pixels, power the strip",
    "separately and join the grounds.",
    "",
    "Colours wrong? Try RGB order with G.",
};
const char* const* ToolNeoPixel::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

const char* ToolNeoPixel::fxName(Fx f) {
    switch (f) {
        case Fx::Solid:   return "solid";
        case Fx::Rainbow: return "rainbow";
        case Fx::Chase:   return "chase";
        case Fx::Breathe: return "breathe";
        case Fx::Sparkle: return "sparkle";
        case Fx::Off:     return "off";
        default:          return "?";
    }
}

void ToolNeoPixel::hsv(uint8_t h, uint8_t s, uint8_t v, uint8_t* rgb) {
    uint8_t region = h / 43;
    uint8_t rem    = (uint8_t)((h - region * 43) * 6);
    uint8_t p = (uint8_t)((v * (255 - s)) >> 8);
    uint8_t q = (uint8_t)((v * (255 - ((s * rem) >> 8))) >> 8);
    uint8_t t = (uint8_t)((v * (255 - ((s * (255 - rem)) >> 8))) >> 8);
    switch (region) {
        case 0:  rgb[0]=v; rgb[1]=t; rgb[2]=p; break;
        case 1:  rgb[0]=q; rgb[1]=v; rgb[2]=p; break;
        case 2:  rgb[0]=p; rgb[1]=v; rgb[2]=t; break;
        case 3:  rgb[0]=p; rgb[1]=q; rgb[2]=v; break;
        case 4:  rgb[0]=t; rgb[1]=p; rgb[2]=v; break;
        default: rgb[0]=v; rgb[1]=p; rgb[2]=q; break;
    }
}

bool ToolNeoPixel::initRmt(int gpio) {
    // 10 MHz -> 0.1 us per tick, which resolves the 0.4/0.8 us pulses the
    // WS2812 datasheet asks for.
    if (!rmtInit(gpio, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_2, 10000000)) return false;
    rmtSetEOT(gpio, LOW);
    return true;
}

void ToolNeoPixel::onEnter() {
    int g = pin(0);
    _ready = false;
    if (g < 0 || !pinGpioOk(g)) { ui.notify("pick a data pin"); return; }
    pins.claimBus(g);
    _ready = initRmt(g);
    if (!_ready) ui.notify("RMT unavailable");
    _phase = 0;
    render();
    show();
}

void ToolNeoPixel::onExit() {
    int g = pin(0);
    if (_ready && g >= 0) {
        for (int i = 0; i < MAXPIX; i++) _px[i][0] = _px[i][1] = _px[i][2] = 0;
        show();                       // leave the strip dark, not stuck lit
        rmtDeinit(g);
    }
    if (g >= 0) pins.release(g);
    _ready = false;
}

void ToolNeoPixel::render() {
    uint8_t v = (uint8_t)((255 * _bright) / 100);
    uint8_t c[3];

    for (int i = 0; i < _count && i < MAXPIX; i++) {
        switch (_fx) {
            case Fx::Solid:
                hsv((uint8_t)_hue, 255, v, c);
                break;
            case Fx::Rainbow:
                hsv((uint8_t)((_hue + (i * 256) / (_count ? _count : 1)) & 0xFF), 255, v, c);
                break;
            case Fx::Chase: {
                int head = (int)(_phase % (uint32_t)(_count ? _count : 1));
                int d = (i - head + _count) % _count;
                uint8_t fall = (d < 4) ? (uint8_t)(v >> d) : 0;
                hsv((uint8_t)_hue, 255, fall, c);
                break;
            }
            case Fx::Breathe: {
                uint8_t p = (uint8_t)(_phase & 0xFF);
                uint8_t tri = (p < 128) ? (uint8_t)(p * 2) : (uint8_t)((255 - p) * 2);
                hsv((uint8_t)_hue, 255, (uint8_t)((v * tri) / 255), c);
                break;
            }
            case Fx::Sparkle: {
                // Deterministic hash of pixel+phase: no rand(), so the
                // pattern is reproducible between runs.
                uint32_t h = (uint32_t)(i * 2654435761u) ^ (_phase * 40503u);
                bool on = ((h >> 13) & 7) == 0;
                hsv((uint8_t)_hue, on ? 0 : 255, on ? v : (uint8_t)(v / 8), c);
                break;
            }
            default:
                c[0] = c[1] = c[2] = 0;
                break;
        }
        _px[i][0] = c[0]; _px[i][1] = c[1]; _px[i][2] = c[2];
    }
    for (int i = _count; i < MAXPIX; i++)
        _px[i][0] = _px[i][1] = _px[i][2] = 0;
}

void ToolNeoPixel::show() {
    if (!_ready) return;
    int g = pin(0);
    if (g < 0) return;

    static rmt_data_t buf[MAXPIX * 24];
    int n = 0;
    int cnt = _count < MAXPIX ? _count : MAXPIX;

    for (int i = 0; i < cnt; i++) {
        uint8_t ord[3];
        if (_grb) { ord[0] = _px[i][1]; ord[1] = _px[i][0]; ord[2] = _px[i][2]; }
        else      { ord[0] = _px[i][0]; ord[1] = _px[i][1]; ord[2] = _px[i][2]; }

        for (int b = 0; b < 3; b++)
            for (int bit = 7; bit >= 0; bit--) {          // MSB first
                bool one = (ord[b] >> bit) & 1;
                buf[n].level0    = 1;
                buf[n].duration0 = one ? 8 : 4;           // 0.8 us / 0.4 us
                buf[n].level1    = 0;
                buf[n].duration1 = one ? 4 : 8;           // 0.4 us / 0.8 us
                n++;
            }
    }
    if (n == 0) return;
    rmtWrite(g, buf, n, 100);
    delayMicroseconds(60);            // latch
}

void ToolNeoPixel::tick() {
    if (!_ready) return;
    uint32_t iv = (uint32_t)(210 - _speed * 20);
    if (millis() - _lastStep < iv) return;
    _lastStep = millis();

    if (_fx == Fx::Rainbow || _fx == Fx::Breathe) _hue = (_hue + 3) & 0xFF;
    _phase++;
    render();
    show();
}

void ToolNeoPixel::adjust(int delta) {
    switch (_sel) {
        case Sel::Effect: {
            int v = ((int)_fx + delta + (int)Fx::COUNT) % (int)Fx::COUNT;
            _fx = (Fx)v;
            break;
        }
        case Sel::Count:
            _count = constrain(_count + delta, 1, MAXPIX);
            break;
        case Sel::Bright:
            _bright = constrain(_bright + delta * 5, 1, 100);
            break;
        case Sel::Speed:
            _speed = constrain(_speed + delta, 1, 10);
            break;
        case Sel::Hue:
            _hue = (_hue + delta * 8) & 0xFF;
            break;
        default: break;
    }
    render();
    show();
}

bool ToolNeoPixel::onKey(const KeyEvent& ev) {
    switch (ev.key) {
        case Key::Up:
            _sel = (Sel)(((int)_sel + (int)Sel::COUNT - 1) % (int)Sel::COUNT);
            return true;
        case Key::Down:
            _sel = (Sel)(((int)_sel + 1) % (int)Sel::COUNT);
            return true;
        case Key::Left:  adjust(-1); return true;
        case Key::Right: adjust(+1); return true;
        case Key::Char:
            if (ev.ch == ' ') { _fx = Fx::Off; render(); show(); return true; }
            if (ev.ci('g')) {
                _grb = !_grb;
                ui.notify(_grb ? "GRB order" : "RGB order");
                show();
                return true;
            }
            return false;
        default:
            return false;
    }
}

void ToolNeoPixel::draw() {
    char pinlbl[24];
    pinLabel(pin(0), pinlbl, sizeof(pinlbl));
    ui.header("NeoPixel", pinlbl, catColor(cat()));

    struct { const char* k; char v[12]; } rows[(int)Sel::COUNT];
    rows[0].k = "effect";     snprintf(rows[0].v, sizeof(rows[0].v), "%s", fxName(_fx));
    rows[1].k = "pixels";     snprintf(rows[1].v, sizeof(rows[1].v), "%d", _count);
    rows[2].k = "brightness"; snprintf(rows[2].v, sizeof(rows[2].v), "%d%%", _bright);
    rows[3].k = "speed";      snprintf(rows[3].v, sizeof(rows[3].v), "%d", _speed);
    rows[4].k = "hue";        snprintf(rows[4].v, sizeof(rows[4].v), "%d", _hue);

    for (int i = 0; i < (int)Sel::COUNT; i++) {
        int y = BODY_Y + 3 + i * 13;
        bool sel = (i == (int)_sel);
        if (sel) ui.listRow(y - 2, 12, true);
        ui.text(8, y, sel ? C_TITLE : C_DIM, rows[i].k);
        ui.text(84, y, sel ? C_TEXT : C_DIM, rows[i].v);
    }

    // Live preview of the strip along the right edge.
    ui.panel(140, BODY_Y + 2, 94, 62);
    int cnt = _count < 32 ? _count : 32;
    int cols = 8;
    for (int i = 0; i < cnt; i++) {
        int cx = 146 + (i % cols) * 11;
        int cy = BODY_Y + 8 + (i / cols) * 13;
        uint16_t col = ui.g().color565(_px[i][0], _px[i][1], _px[i][2]);
        ui.g().fillRoundRect(cx, cy, 9, 9, 2, col);
        ui.g().drawRoundRect(cx, cy, 9, 9, 2, C_LINE);
    }
    if (_count > 32) ui.text(146, BODY_Y + 54, C_FAINT, "+more");

    // Rough current estimate — the number that decides whether the strip can
    // hang off the Cardputer at all.
    uint32_t mA = 0;
    for (int i = 0; i < _count && i < MAXPIX; i++)
        mA += (uint32_t)((_px[i][0] + _px[i][1] + _px[i][2]) * 20) / 255;
    ui.textf(8, BODY_B - 9, mA > 500 ? C_WARN : C_FAINT,
             "%s  ~%lu mA", _grb ? "GRB" : "RGB", (unsigned long)mA);

    ui.footer("[^v]item [<>]change [SPC]off");
}

}  // namespace cg
