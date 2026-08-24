#include "ToolScope.h"
#include "soc/gpio_reg.h"
#include <string.h>

namespace cg {

ToolScope toolScope;

static const Role ROLES[] = {
    { "CH 1", RoleDir::In, "-> signal 1 (trigger)", -1 },
    { "CH 2", RoleDir::In, "-> signal 2", -1 },
    { "CH 3", RoleDir::In, "-> signal 3", -1 },
    { "CH 4", RoleDir::In, "-> signal 4", -1 },
};
const Role* ToolScope::roles() const { return ROLES; }

static const char* HELP[] = {
    "SPACE  arm and capture one buffer",
    "T      trigger: none / rising / falling",
    "1-4    which channel triggers",
    "+ -    sample rate",
    "Z      zoom in     X   zoom out",
    "< >    pan (hold to scroll)",
    "C      cursor on / off",
    "",
    "Capture is a burst: 1024 samples taken",
    "back to back, then the screen updates.",
    "With a trigger set, sampling waits for",
    "the edge (1 s timeout, then it captures",
    "anyway and says 'no trig').",
};
const char* const* ToolScope::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

static const uint32_t RATES[] = {
    10000, 50000, 100000, 500000, 1000000, 2000000, 4000000
};
static constexpr int RATE_N = (int)(sizeof(RATES) / sizeof(RATES[0]));

uint32_t ToolScope::rateHz(uint8_t i) { return RATES[i % RATE_N]; }

const char* ToolScope::trigName(Trig t) {
    switch (t) {
        case Trig::None: return "free";
        case Trig::Rise: return "rise";
        case Trig::Fall: return "fall";
        default:         return "?";
    }
}

void ToolScope::onEnter() {
    for (int i = 0; i < CH; i++)
        if (pin(i) >= 0) pins.setInput(pin(i), 1);
    _phase  = Phase::Idle;
    _pan    = 0;
    _zoom   = 1;
    _cursor = -1;
}

void ToolScope::onExit() {
    for (int i = 0; i < CH; i++)
        if (pin(i) >= 0) pins.release(pin(i));
}

// Reading the two GPIO input registers once per sample and masking out the
// four bits we care about is far cheaper than four digitalRead() calls, and
// it is what lets the fast rates actually hit their interval.
void ToolScope::capture() {
    int  gp[CH];
    for (int i = 0; i < CH; i++) gp[i] = pin(i);

    uint32_t rate    = rateHz(_rateIdx);
    uint32_t cpuHz   = getCpuFrequencyMhz() * 1000000UL;
    uint32_t perSamp = cpuHz / rate;
    if (perSamp < 8) perSamp = 8;

    // ── Wait for the trigger edge ─────────────────────────────────────────
    _triggered = true;
    if (_trig != Trig::None && gp[_trigCh] >= 0) {
        bool want = (_trig == Trig::Rise);
        uint32_t deadline = millis() + 1000;
        bool prev = digitalRead(gp[_trigCh]);
        _triggered = false;
        while (millis() < deadline) {
            bool now = digitalRead(gp[_trigCh]);
            if (now != prev && now == want) { _triggered = true; break; }
            prev = now;
        }
    }

    // ── Burst ─────────────────────────────────────────────────────────────
    uint32_t mask0 = 0, mask1 = 0;
    for (int i = 0; i < CH; i++) {
        if (gp[i] < 0) continue;
        if (gp[i] < 32) mask0 |= (1UL << gp[i]);
        else            mask1 |= (1UL << (gp[i] - 32));
    }
    (void)mask0; (void)mask1;

    noInterrupts();
    uint32_t t0 = ESP.getCycleCount();
    for (int s = 0; s < SAMPLES; s++) {
        uint32_t due = t0 + (uint32_t)s * perSamp;
        while ((int32_t)(ESP.getCycleCount() - due) < 0) { }

        uint32_t in0 = REG_READ(GPIO_IN_REG);
        uint32_t in1 = REG_READ(GPIO_IN1_REG);

        uint8_t v = 0;
        for (int i = 0; i < CH; i++) {
            int g = gp[i];
            if (g < 0) continue;
            bool bit = (g < 32) ? ((in0 >> g) & 1) : ((in1 >> (g - 32)) & 1);
            if (bit) v |= (uint8_t)(1 << i);
        }
        _buf[s] = v;
    }
    interrupts();

    _phase  = Phase::Done;
    _pan    = 0;
    _cursor = -1;
}

bool ToolScope::onKey(const KeyEvent& ev) {
    int d = ev.digit();
    if (d >= 1 && d <= CH) { _trigCh = d - 1; ui.notify("trigger on ch%d", d); return true; }

    switch (ev.key) {
        case Key::Left:
            _pan -= _zoom * 8;
            if (_pan < 0) _pan = 0;
            return true;
        case Key::Right: {
            int maxPan = SAMPLES - (SCR_W - 30) * _zoom;
            _pan += _zoom * 8;
            if (_pan > maxPan) _pan = maxPan < 0 ? 0 : maxPan;
            return true;
        }
        case Key::Char:
            if (ev.ch == ' ') { capture(); return true; }
            if (ev.ci('t')) {
                _trig = (Trig)(((int)_trig + 1) % (int)Trig::COUNT);
                ui.notify("trigger %s", trigName(_trig));
                return true;
            }
            if (ev.ci('z')) { if (_zoom > 1) _zoom /= 2; return true; }
            if (ev.ci('x')) { if (_zoom < 16) _zoom *= 2; return true; }
            if (ev.ci('c')) { _cursor = (_cursor < 0) ? (SCR_W - 30) / 2 : -1; return true; }
            if (ev.ch == '+' || ev.ch == '=') {
                if (_rateIdx < RATE_N - 1) _rateIdx++;
                ui.notify("%lu Sa/s", (unsigned long)rateHz(_rateIdx));
                return true;
            }
            if (ev.ch == '-' || ev.ch == '_') {
                if (_rateIdx > 0) _rateIdx--;
                ui.notify("%lu Sa/s", (unsigned long)rateHz(_rateIdx));
                return true;
            }
            return false;
        default:
            return false;
    }
}

void ToolScope::draw() {
    char right[20];
    uint32_t r = rateHz(_rateIdx);
    if (r >= 1000000UL) snprintf(right, sizeof(right), "%luMSa", (unsigned long)(r / 1000000UL));
    else                snprintf(right, sizeof(right), "%lukSa", (unsigned long)(r / 1000UL));
    ui.header("Logic Analyzer", right, C_HDR);

    if (_phase != Phase::Done) {
        ui.text(8, BODY_Y + 26, C_TEXT, "Press SPACE to capture.");
        ui.textf(8, BODY_Y + 40, C_DIM, "trigger: %s on ch%d", trigName(_trig), _trigCh + 1);
        ui.textf(8, BODY_Y + 52, C_DIM, "%d samples at %lu Sa/s", SAMPLES, (unsigned long)r);
        float win = (SAMPLES * 1000000.0f) / r;
        if (win >= 1000.0f) ui.textf(8, BODY_Y + 64, C_FAINT, "window %.2f ms", win / 1000.0f);
        else                ui.textf(8, BODY_Y + 64, C_FAINT, "window %.1f us", win);
        ui.footer("[SPC] capture [T] trig [1-4] ch [+/-] rate");
        return;
    }

    drawWaves();

    char f[48];
    snprintf(f, sizeof(f), "[SPC] again [Z/X] zoom x%d %s", _zoom,
             _triggered ? "" : " NO TRIG");
    ui.footer(f);
}

void ToolScope::drawWaves() {
    const int LBL_W = 26;
    const int WX    = LBL_W;
    const int WW    = SCR_W - LBL_W - 4;
    const int CH_H  = 22;
    const int TOP   = BODY_Y + 2;
    const int RISE  = 7;

    for (int c = 0; c < CH; c++) {
        int mid = TOP + c * CH_H + CH_H / 2;
        int yH  = mid - RISE;
        int yL  = mid + RISE;

        uint16_t col = (pin(c) >= 0) ? C_HIGH : C_FAINT;
        ui.textf(2, mid - 4, (c == _trigCh) ? C_TITLE : C_DIM, "G%-2d",
                 pin(c) >= 0 ? pin(c) : 0);

        // Baseline so an idle channel is still visible.
        ui.g().drawFastHLine(WX, yL, WW, C_WAVE_BG);

        if (pin(c) < 0) continue;

        int prevY = -1;
        for (int px = 0; px < WW; px++) {
            int s = _pan + px * _zoom;
            if (s >= SAMPLES) break;

            // When zoomed out, a column is high if any sample in it is high,
            // so narrow pulses do not vanish between pixels.
            bool v = false;
            for (int k = 0; k < _zoom && (s + k) < SAMPLES; k++)
                if ((_buf[s + k] >> c) & 1) { v = true; break; }

            int y = v ? yH : yL;
            if (prevY >= 0 && y != prevY)
                ui.g().drawFastVLine(WX + px, y < prevY ? y : prevY,
                                     (y > prevY ? y - prevY : prevY - y) + 1, col);
            ui.g().drawPixel(WX + px, y, col);
            prevY = y;
        }
    }

    if (_cursor >= 0) {
        int cx = WX + _cursor;
        ui.g().drawFastVLine(cx, TOP, CH * CH_H, C_WARN);
        int s = _pan + _cursor * _zoom;
        float us = (s * 1000000.0f) / rateHz(_rateIdx);
        ui.textf(WX + 2, BODY_B - 9, C_WARN, "t=%.1fus s=%d", us, s);
    } else {
        float span = (WW * _zoom * 1000000.0f) / rateHz(_rateIdx);
        if (span >= 1000.0f) ui.textf(WX, BODY_B - 9, C_FAINT, "span %.2fms  pos %d", span / 1000.0f, _pan);
        else                 ui.textf(WX, BODY_B - 9, C_FAINT, "span %.0fus  pos %d", span, _pan);
    }
}

}  // namespace cg
