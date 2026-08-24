#include "ToolBoard.h"
#include <M5Cardputer.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <stdio.h>

namespace cg {

ToolBoard toolBoard;

static const char* HELP[] = {
    "  TAB    next page",
    "  ^ v    scroll",
    "",
    "Pins marked LOCK are refused everywhere:",
    "G8 and G9 carry the system I2C bus that",
    "the keyboard controller sits on. Driving",
    "them takes the keyboard down with them,",
    "and the only way back is a reflash.",
    "",
    "Pins marked SD are shared with the card",
    "reader and are only offered when you turn",
    "'allow SD pins' on in Settings.",
    "",
    "ADC1 pins read while WiFi is up; ADC2",
    "pins do not. This firmware never starts",
    "WiFi, so both work here.",
};
const char* const* ToolBoard::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

void ToolBoard::onEnter() { _scroll = 0; }

int ToolBoard::rowsOnPage() const { return 7; }

bool ToolBoard::onKey(const KeyEvent& ev) {
    switch (ev.key) {
        case Key::Tab:
            _page   = (Page)(((int)_page + 1) % (int)Page::COUNT);
            _scroll = 0;
            return true;
        case Key::Left:
            _page   = (Page)(((int)_page + (int)Page::COUNT - 1) % (int)Page::COUNT);
            _scroll = 0;
            return true;
        case Key::Right:
            _page   = (Page)(((int)_page + 1) % (int)Page::COUNT);
            _scroll = 0;
            return true;
        case Key::Up:
            if (_scroll > 0) _scroll--;
            return true;
        case Key::Down:
            _scroll++;
            return true;
        default:
            return false;
    }
}

void ToolBoard::drawSummary() {
    esp_chip_info_t ci;
    esp_chip_info(&ci);

    int y = BODY_Y + 2;
    ui.textf(8, y, C_TITLE, "%s", boardName());
    y += 12;
    if (!boardIsAdv()) {
        ui.text(8, y, C_LOW, "pin map assumes the ADV!");
        y += 11;
    }

    ui.textf(8, y, C_DIM,  "SoC");       ui.textf(60, y, C_TEXT, "ESP32-S3 rev%d, %d core", ci.revision, ci.cores);
    y += 11;
    ui.textf(8, y, C_DIM,  "flash");     ui.textf(60, y, C_TEXT, "%lu MB", (unsigned long)(ESP.getFlashChipSize() / (1024 * 1024)));
    y += 11;
    ui.textf(8, y, C_DIM,  "PSRAM");     ui.textf(60, y, ESP.getPsramSize() ? C_TEXT : C_DIM,
                                                  "%lu KB", (unsigned long)(ESP.getPsramSize() / 1024));
    y += 11;
    ui.textf(8, y, C_DIM,  "free heap"); ui.textf(60, y, C_TEXT, "%lu KB", (unsigned long)(ESP.getFreeHeap() / 1024));
    y += 11;

    int batt = M5.Power.getBatteryLevel();
    ui.textf(8, y, C_DIM, "battery");
    if (batt >= 0) ui.textf(60, y, batt < 20 ? C_WARN : C_TEXT, "%d%%", batt);
    else           ui.text(60, y, C_DIM, "unknown");
    y += 11;

    ui.textf(8, y, C_DIM, "uptime");
    uint32_t s = millis() / 1000;
    ui.textf(60, y, C_TEXT, "%luh %02lum %02lus",
             (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
}

void ToolBoard::drawHeader() {
    const PinInfo* t = pinTable();
    int n = pinCount();
    int rows = rowsOnPage();
    if (_scroll > n - rows) _scroll = n - rows;
    if (_scroll < 0) _scroll = 0;

    ui.text(8, BODY_Y, C_DIM, "pin");
    ui.text(46, BODY_Y, C_DIM, "where");
    ui.text(104, BODY_Y, C_DIM, "silk");
    ui.text(156, BODY_Y, C_DIM, "notes");

    for (int i = 0; i < rows && (i + _scroll) < n; i++) {
        const PinInfo& p = t[i + _scroll];
        int y = BODY_Y + 12 + i * 12;
        bool ok = pinGpioOk(p.gpio);

        ui.textf(8, y, ok ? C_TEXT : C_LOW, "G%d", (int)p.gpio);
        ui.text(46, y, C_DIM, p.header);
        ui.text(104, y, C_TEXT, p.silk);

        int x = 156;
        if (p.flags & PF_LOCKED) { ui.chip(x, y - 2, "LOCK", C_BLACK, C_LOW); x += 30; }
        if (p.flags & PF_SD)     { ui.chip(x, y - 2, "SD",   C_BLACK, C_WARN); x += 22; }
        if (p.flags & PF_ADC1)   { ui.chip(x, y - 2, "A1",   C_BLACK, C_ROLE_ADC); x += 22; }
        else if (p.flags & PF_ADC2) { ui.chip(x, y - 2, "A2", C_BLACK, C_ROLE_ADC); x += 22; }
    }

    ui.scrollbar(SCR_W - 4, BODY_Y + 12, rows * 12, _scroll, rows, n);
}

void ToolBoard::drawInternal() {
    const PeriphInfo* items = periphTable();
    const int n = periphCount();
    int rows = rowsOnPage() + 1;
    if (_scroll > n - rows) _scroll = n - rows;
    if (_scroll < 0) _scroll = 0;

    for (int i = 0; i < rows && (i + _scroll) < n; i++) {
        const PeriphInfo& it = items[i + _scroll];
        int y = BODY_Y + 2 + i * 12;
        ui.text(8, y, C_DIM, it.what);
        ui.text(104, y, C_TEXT, it.pins);
    }
    ui.scrollbar(SCR_W - 4, BODY_Y + 2, rows * 12, _scroll, rows, n);
}

void ToolBoard::draw() {
    static const char* PAGE[] = { "summary", "headers", "internals" };
    ui.header("Board Info", PAGE[(int)_page], catColor(cat()));

    switch (_page) {
        case Page::Summary:  drawSummary();  break;
        case Page::Header:   drawHeader();   break;
        default:             drawInternal(); break;
    }

    ui.footer("[TAB] page  [^v] scroll  [DEL] back");
}

}  // namespace cg
