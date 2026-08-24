#include "ToolDigitalIn.h"

namespace cg {

ToolDigitalIn toolDigitalIn;

static const Role ROLES[] = {
    { "IN 1", RoleDir::In, "-> signal 1", -1 },
    { "IN 2", RoleDir::In, "-> signal 2", -1 },
    { "IN 3", RoleDir::In, "-> signal 3", -1 },
    { "IN 4", RoleDir::In, "-> signal 4", -1 },
    { "IN 5", RoleDir::In, "-> signal 5", -1 },
    { "IN 6", RoleDir::In, "-> signal 6", -1 },
};
const Role* ToolDigitalIn::roles() const { return ROLES; }

static const char* HELP[] = {
    "Six inputs sampled continuously, with",
    "edges counted in an interrupt so short",
    "pulses are not missed.",
    "",
    "  R      reset all counters",
    "  P      cycle pull-up / pull-down / none",
    "",
    "Rate is edges per second over a 1 s",
    "window. Above roughly 50 kHz the ISR",
    "starts losing edges -- use Frequency",
    "Counter, which counts in hardware.",
};
const char* const* ToolDigitalIn::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

// ── ISR state ─────────────────────────────────────────────────────────────
static volatile uint32_t s_edges[6] = {};

static void IRAM_ATTR edgeIsr(void* arg) {
    uint32_t i = (uint32_t)(uintptr_t)arg;
    if (i < 6) s_edges[i]++;
}

const char* ToolDigitalIn::pullName(uint8_t p) {
    return p == 1 ? "pull-up" : (p == 2 ? "pull-down" : "floating");
}

void ToolDigitalIn::attach() {
    for (int i = 0; i < CH; i++) {
        int g = pin(i);
        if (g < 0) continue;
        pins.setInput(g, _pull);
        attachInterruptArg(digitalPinToInterrupt(g), edgeIsr,
                           (void*)(uintptr_t)i, CHANGE);
    }
    _attached = true;
}

void ToolDigitalIn::detach() {
    if (!_attached) return;
    for (int i = 0; i < CH; i++) {
        int g = pin(i);
        if (g >= 0) detachInterrupt(digitalPinToInterrupt(g));
    }
    _attached = false;
}

void ToolDigitalIn::resetCounts() {
    for (int i = 0; i < CH; i++) {
        s_edges[i]    = 0;
        _shown[i]     = 0;
        _rateBase[i]  = 0;
        _hz[i]        = 0;
    }
    _lastRate = millis();
}

void ToolDigitalIn::onEnter() {
    resetCounts();
    attach();
}

void ToolDigitalIn::onExit() {
    detach();
    for (int i = 0; i < CH; i++)
        if (pin(i) >= 0) pins.release(pin(i));
}

void ToolDigitalIn::tick() {
    for (int i = 0; i < CH; i++) {
        int g = pin(i);
        if (g >= 0) _lvl[i] = pins.read(g);
        _shown[i] = s_edges[i];
    }

    uint32_t now = millis();
    uint32_t dt  = now - _lastRate;
    if (dt >= 1000) {
        for (int i = 0; i < CH; i++) {
            // CHANGE fires on both edges, so a full cycle is two interrupts.
            uint32_t d = _shown[i] - _rateBase[i];
            _hz[i]     = (d * 1000.0f) / (dt * 2.0f);
            _rateBase[i] = _shown[i];
        }
        _lastRate = now;
    }
}

bool ToolDigitalIn::onKey(const KeyEvent& ev) {
    if (ev.ci('r')) { resetCounts(); ui.notify("counters cleared"); return true; }
    if (ev.ci('p')) {
        detach();
        _pull = (uint8_t)((_pull + 1) % 3);
        attach();
        resetCounts();
        ui.notify("input: %s", pullName(_pull));
        return true;
    }
    return false;
}

void ToolDigitalIn::draw() {
    ui.header("Digital In", pullName(_pull), C_HDR);

    int y = BODY_Y + 2;
    for (int i = 0; i < CH; i++) {
        int g = pin(i);
        if (g < 0) {
            ui.textf(6, y + 3, C_WARN, "IN %d  unassigned", i + 1);
            y += 17;
            continue;
        }

        char lbl[20];
        pinLabel(g, lbl, sizeof(lbl));
        ui.textf(6, y + 3, C_TEXT, "%-10.10s", lbl);

        ui.state(72, y + 2, _lvl[i]);

        ui.textf(108, y + 3, C_DIM, "%8lu edges", (unsigned long)_shown[i]);

        if (_hz[i] >= 1000.0f)
            ui.textf(186, y + 3, C_INFO, "%5.1fk", _hz[i] / 1000.0f);
        else if (_hz[i] > 0.0f)
            ui.textf(186, y + 3, C_INFO, "%5.0fHz", _hz[i]);
        else
            ui.text(186, y + 3, C_FAINT, "   --");

        y += 17;
    }

    ui.footer("[R] reset counters   [P] pull mode");
}


const char* ToolDigitalIn::logHeader() const { return "d0,d1,d2,d3,d4,d5"; }

bool ToolDigitalIn::logRow(char* out, size_t n) {
    int w = 0;
    for (int i = 0; i < CH; i++) {
        if (pin(i) >= 0) w += snprintf(out + w, n - w, "%s%d", i ? "," : "", _lvl[i] ? 1 : 0);
        else             w += snprintf(out + w, n - w, "%s", i ? "," : "");
        if (w >= (int)n) return false;
    }
    return true;
}

}  // namespace cg
