#include "WiringGuide.h"
#include "../core/Settings.h"
#include <string.h>

namespace cg {

// The EXT header exactly as it sits on the board: odd pins down the left,
// even pins down the right. Source: M5Stack Cardputer-Adv PinMap.
struct ExtRow { int8_t lGpio; const char* lSilk; uint8_t lNum;
                uint8_t rNum; const char* rSilk; int8_t rGpio; };

static const ExtRow EXT[] = {
    {  3, "RESET",    1,  2, "5VIN",  -1 },
    {  4, "INT",      3,  4, "GND",   -1 },
    {  6, "BUSY",     5,  6, "5VOUT", -1 },
    { 40, "SCK",      7,  8, "SDA",    8 },
    { 14, "MOSI",     9, 10, "SCL",    9 },
    { 39, "MISO",    11, 12, "U_TX",  13 },
    {  5, "CS",      13, 14, "U_RX",  15 },
};
static constexpr int EXT_N = (int)(sizeof(EXT) / sizeof(EXT[0]));

void WiringGuide::begin(Tool* tool) {
    _tool    = tool;
    _result  = WiringResult::Pending;
    _scroll  = 0;
    _mapView = false;
}

int WiringGuide::roleOf(int gpio) const {
    if (gpio < 0) return -1;
    for (int i = 0; i < _tool->roleCount(); i++)
        if (_tool->pin(i) == gpio) return i;
    return -1;
}

const char* WiringGuide::roleProblem(int i) const {
    const Role& r = _tool->roles()[i];
    int gpio = _tool->pin(i);

    if (gpio < 0) return "no pin assigned";

    const PinInfo* p = pinInfo(gpio);
    if (!p)                     return "not on any header";
    if (p->flags & PF_LOCKED)   return "system I2C: never available";
    if ((p->flags & PF_SD) && !settings.allowSdPins())
                                return "microSD pin: enable in Settings";

    if (r.dir == RoleDir::Adc) {
        if (!(p->flags & (PF_ADC1 | PF_ADC2))) return "this pin has no ADC";
        if ((p->flags & PF_ADC2) && !(p->flags & PF_ADC1) && boardRadioActive())
            return "ADC2 is dead while WiFi is on";
    }
    return nullptr;
}

bool WiringGuide::onKey(const KeyEvent& ev) {
    switch (ev.key) {
        case Key::Enter:
            _result = WiringResult::Start;
            return true;
        case Key::Back:
        case Key::Esc:
            _result = WiringResult::Back;
            return true;
        case Key::Tab:
            _mapView = !_mapView;
            return true;
        case Key::Up:
            if (_scroll > 0) _scroll--;
            return true;
        case Key::Down:
            if (_scroll < _tool->roleCount() - 5) _scroll++;
            return true;
        case Key::Char:
            if (ev.ci('c')) { _result = WiringResult::Configure; return true; }
            if (ev.ci('m')) { _mapView = !_mapView; return true; }
            return true;
        default:
            return true;
    }
}

void WiringGuide::draw() {
    ui.clear();
    char hdr[36];
    snprintf(hdr, sizeof(hdr), "Wiring: %.16s", _tool->name());
    ui.header(hdr, _mapView ? "MAP" : "LIST", C_HDR);

    if (_mapView) drawMap();
    else          drawList();

    ui.footer(_mapView ? "[TAB] list [ENT] start [C] pins"
                       : "[TAB] map  [ENT] start [C] pins");
    ui.drawNotification();
}

void WiringGuide::drawList() {
    int rc = _tool->roleCount();
    if (rc == 0) {
        ui.text(6, BODY_Y + 20, C_TEXT, "No wiring needed.");
        ui.text(6, BODY_Y + 34, C_DIM, "Uses onboard hardware only.");
        return;
    }

    const int ROW_H  = 15;
    const int VISIBLE = 5;
    int y = BODY_Y + 1;
    int end = _scroll + VISIBLE;
    if (end > rc) end = rc;

    for (int i = _scroll; i < end; i++) {
        const Role& r    = _tool->roles()[i];
        int         gpio = _tool->pin(i);
        uint16_t    col  = roleColor(r.dir);

        ui.g().fillRect(2, y + 2, 3, ROW_H - 5, col);
        ui.textf(9, y + 4, C_TEXT, "%-8.8s", r.label);

        char lbl[20];
        if (gpio >= 0) pinLabel(gpio, lbl, sizeof(lbl));
        else           snprintf(lbl, sizeof(lbl), "unset");
        bool bad = roleProblem(i) != nullptr;
        ui.textf(60, y + 4, gpio < 0 ? C_WARN : (bad ? C_LOW : C_HIGH),
                 "%-11.11s", lbl);

        ui.textf(130, y + 4, C_INFO, "%-18.18s", r.hint);
        y += ROW_H;
    }

    ui.scrollbar(SCR_W - 4, BODY_Y + 1, VISIBLE * ROW_H, _scroll, VISIBLE, rc);

    // Surface the first real problem rather than making the user hunt. A pin
    // that cannot work outranks one that merely has a caveat.
    for (int i = 0; i < rc; i++) {
        const char* w = roleProblem(i);
        if (w) {
            ui.textf(6, BODY_Y + VISIBLE * ROW_H + 3, C_LOW,
                     "x %.8s: %.26s", _tool->roles()[i].label, w);
            return;
        }
    }
    for (int i = 0; i < rc; i++) {
        const char* w = pinWarn(_tool->pin(i));
        if (w) {
            ui.textf(6, BODY_Y + VISIBLE * ROW_H + 3, C_WARN,
                     "! %.8s: %.26s", _tool->roles()[i].label, w);
            return;
        }
    }
}

void WiringGuide::drawMap() {
    const int ROW_H = 12;
    const int TOP   = BODY_Y + 10;

    ui.text(4, BODY_Y + 1, C_DIM, "EXT 2.54-14P");
    ui.textRight(SCR_W - 4, BODY_Y + 1, C_DIM, "GND always!");

    for (int i = 0; i < EXT_N; i++) {
        const ExtRow& e = EXT[i];
        int y = TOP + i * ROW_H;

        // ── left column ───────────────────────────────────────────────────
        int rl = roleOf(e.lGpio);
        uint16_t bgL = (rl >= 0) ? roleColor(_tool->roles()[rl].dir) : C_PANEL;
        uint16_t fgL = (rl >= 0) ? C_BLACK : C_DIM;
        ui.g().fillRect(4, y, 96, ROW_H - 1, bgL);
        if (e.lGpio >= 0) ui.textf(6, y + 2, fgL, "G%-2d %-6.6s", e.lGpio, e.lSilk);

        // ── pin numbers down the middle ───────────────────────────────────
        ui.textf(103, y + 2, C_FAINT, "%2d", e.lNum);
        ui.textf(123, y + 2, C_FAINT, "%2d", e.rNum);

        // ── right column ──────────────────────────────────────────────────
        int rr = roleOf(e.rGpio);
        bool sysPin = (e.rGpio == 8 || e.rGpio == 9);
        uint16_t bgR = (rr >= 0) ? roleColor(_tool->roles()[rr].dir)
                                 : (sysPin ? C_FAINT : C_PANEL);
        uint16_t fgR = (rr >= 0) ? C_BLACK : (sysPin ? C_BG : C_DIM);
        ui.g().fillRect(140, y, 96, ROW_H - 1, bgR);
        if (e.rGpio >= 0) ui.textf(142, y + 2, fgR, "G%-2d %-6.6s", e.rGpio, e.rSilk);
        else              ui.textf(142, y + 2, fgR, "%-9.9s", e.rSilk);
    }

    // ── Grove strip ───────────────────────────────────────────────────────
    int gy = TOP + EXT_N * ROW_H + 1;
    ui.text(4, gy, C_DIM, "Grove:");
    int gx = 46;
    const int8_t GROVE[] = { -1, -1, 2, 1 };
    const char*  GLBL[]  = { "GND", "5V", "G2 SDA", "G1 SCL" };
    for (int i = 0; i < 4; i++) {
        int rg = roleOf(GROVE[i]);
        uint16_t bg = (rg >= 0) ? roleColor(_tool->roles()[rg].dir) : C_PANEL;
        uint16_t fg = (rg >= 0) ? C_BLACK : C_DIM;
        int w = 46;
        ui.g().fillRect(gx, gy - 2, w - 2, 11, bg);
        ui.textf(gx + 2, gy, fg, "%-7.7s", GLBL[i]);
        gx += w;
    }
}

}  // namespace cg
