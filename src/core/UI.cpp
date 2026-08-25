#include "UI.h"
#include "Keys.h"
#include "Settings.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace cg {

UI ui;

bool UI::begin() {
    auto& disp = M5Cardputer.Display;
    disp.setRotation(1);
    disp.setColorDepth(16);

    _canvas.setColorDepth(16);
    _canvas.setPsram(false);           // there is no PSRAM on the Stamp-S3A
    _sprite = _canvas.createSprite(SCR_W, SCR_H);

    _g = _sprite ? (LovyanGFX*)&_canvas : (LovyanGFX*)&disp;

    // Font0 at size 1 is a 6x8 cell: exactly 40 columns by 16 rows, which is
    // what every layout in this firmware is measured against.
    _g->setFont(&fonts::Font0);
    _g->setTextSize(1);
    _g->setTextWrap(false);
    return _sprite;
}

void UI::clear(uint16_t col) {
    _g->fillScreen(col);
    _g->setFont(&fonts::Font0);
    _g->setTextSize(1);
    _g->setTextWrap(false);
}

void UI::push() {
    // The frame is still composed while the panel is off -- the web mirror
    // reads the sprite, not the display -- it just does not reach the glass.
    if (!_awake) return;
    if (_sprite) _canvas.pushSprite(0, 0);
}

// ── Display power ─────────────────────────────────────────────────────────
void UI::wakeDisplay() {
    if (_awake) return;
    _awake = true;
    auto& disp = M5Cardputer.Display;
    disp.wakeup();
    disp.setBrightness(settings.brightness());
    // The panel lost its contents while it slept; the next push() repaints
    // the whole sprite, which is within one frame of here.
}

void UI::idleTick() {
    uint16_t secs = settings.screenOff();
    if (secs == 0) { wakeDisplay(); return; }   // timeout turned off: stay lit
    if (!_awake) return;

    if ((uint32_t)(millis() - keys.lastActivity()) < (uint32_t)secs * 1000UL) return;

    _awake = false;
    auto& disp = M5Cardputer.Display;
    disp.setBrightness(0);      // the backlight is what actually costs power
    disp.sleep();               // and this drops the panel's own draw
}

// ── Text ──────────────────────────────────────────────────────────────────
void UI::text(int x, int y, uint16_t col, const char* s) {
    _g->setTextSize(1);
    _g->setTextColor(col);
    _g->setCursor(x, y);
    _g->print(s);
}

void UI::textf(int x, int y, uint16_t col, const char* fmt, ...) {
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    text(x, y, col, buf);
}

void UI::textBig(int x, int y, uint16_t col, uint8_t size, const char* s) {
    _g->setTextSize(size);
    _g->setTextColor(col);
    _g->setCursor(x, y);
    _g->print(s);
    _g->setTextSize(1);
}

void UI::textBigf(int x, int y, uint16_t col, uint8_t size, const char* fmt, ...) {
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    textBig(x, y, col, size, buf);
}

void UI::textRight(int xRight, int y, uint16_t col, const char* s) {
    text(xRight - (int)strlen(s) * CH_W, y, col, s);
}

void UI::textCenter(int cx, int y, uint16_t col, const char* s) {
    text(cx - (int)strlen(s) * CH_W / 2, y, col, s);
}

// ── Chrome ────────────────────────────────────────────────────────────────
int UI::header(const char* title, const char* right, uint16_t accent) {
    _g->fillRect(0, 0, SCR_W, HDR_H, accent);
    _g->drawFastHLine(0, HDR_H, SCR_W, C_LINE);
    text(4, 4, C_TITLE, title);
    if (right && *right) textRight(SCR_W - 4, 4, C_TEXT, right);
    return BODY_Y;
}

void UI::footer(const char* hints) {
    _g->fillRect(0, SCR_H - FTR_H, SCR_W, FTR_H, C_FTR);
    _g->drawFastHLine(0, SCR_H - FTR_H - 1, SCR_W, C_LINE);
    text(3, SCR_H - FTR_H + 2, C_DIM, hints);
}

// Painted after the screen's own footer, so it wins the corner. The screen's
// text is clipped by the fill rather than overlapping it.
void UI::footerBadge(const char* s) {
    if (!s || !*s) return;
    int w = (int)strlen(s) * CH_W + 5;
    _g->fillRect(SCR_W - w, SCR_H - FTR_H, w, FTR_H, C_FTR);
    text(SCR_W - w + 3, SCR_H - FTR_H + 2, C_FAINT, s);
}

void UI::footerf(const char* fmt, ...) {
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    footer(buf);
}

// ── Widgets ───────────────────────────────────────────────────────────────
void UI::panel(int x, int y, int w, int h, uint16_t fill, uint16_t border) {
    _g->fillRoundRect(x, y, w, h, 3, fill);
    if (border != fill) _g->drawRoundRect(x, y, w, h, 3, border);
}

int UI::chipW(const char* label) const {
    return (int)strlen(label) * CH_W + 8;
}

void UI::chip(int x, int y, const char* label, uint16_t fg, uint16_t bg) {
    int w = chipW(label);
    _g->fillRoundRect(x, y, w, 11, 3, bg);
    text(x + 4, y + 2, fg, label);
}

void UI::hbar(int x, int y, int w, int h, float pct, uint16_t col, uint16_t bg) {
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    int filled = (int)((pct / 100.0f) * (w - 2) + 0.5f);
    _g->fillRect(x, y, w, h, bg);
    if (filled > 0) _g->fillRect(x + 1, y + 1, filled, h - 2, col);
    _g->drawRect(x, y, w, h, C_LINE);
}

void UI::levelDot(int x, int y, bool high) {
    _g->fillCircle(x, y, 3, high ? C_HIGH : C_LOW);
}

void UI::state(int x, int y, bool high) {
    _g->fillRoundRect(x, y, 28, 11, 3, high ? C_HIGH : C_LOW);
    text(x + 3, y + 2, C_BLACK, high ? "HI" : "LO");
}

void UI::scrollbar(int x, int y, int h, int offset, int visible, int total) {
    if (total <= visible) return;
    _g->fillRect(x, y, 3, h, C_PANEL2);
    int barH = (visible * h) / total;
    if (barH < 6) barH = 6;
    int span = h - barH;
    int barY = y + (span * offset) / (total - visible);
    _g->fillRect(x, barY, 3, barH, C_DIM);
}

void UI::divider(int y, uint16_t col) {
    _g->drawFastHLine(0, y, SCR_W, col);
}

void UI::listRow(int y, int h, bool selected, uint16_t accent) {
    if (!selected) return;
    _g->fillRect(0, y, SCR_W, h, C_SEL);
    _g->fillRect(0, y, 2, h, accent);
}

void UI::toast(const char* msg, uint16_t col) {
    int w = (int)strlen(msg) * CH_W + 16;
    if (w > SCR_W - 8) w = SCR_W - 8;
    int x = (SCR_W - w) / 2;
    int y = SCR_H - FTR_H - 20;
    _g->fillRoundRect(x, y, w, 15, 4, C_PANEL);
    _g->drawRoundRect(x, y, w, 15, 4, col);
    textCenter(SCR_W / 2, y + 4, col, msg);
}

int UI::modal(const char* title, int w, int h, uint16_t accent) {
    int x = (SCR_W - w) / 2;
    int y = (SCR_H - h) / 2;
    _g->fillRoundRect(x, y, w, h, 4, C_PANEL);
    _g->drawRoundRect(x, y, w, h, 4, accent);
    _g->fillRect(x + 1, y + 1, w - 2, 12, accent);
    text(x + 5, y + 3, C_BLACK, title);
    return y + 16;
}

// ── Notifications ─────────────────────────────────────────────────────────
void UI::notify(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(_note, sizeof(_note), fmt, ap);
    va_end(ap);
    _noteUntil = millis() + 1400;
}

void UI::drawNotification() {
    if (!_noteUntil) return;
    if ((int32_t)(millis() - _noteUntil) >= 0) {
        _noteUntil = 0;
        _note[0]   = 0;
        return;
    }
    toast(_note, C_WARN);
}

void UI::beep(uint16_t freq, uint16_t ms) {
    if (!settings.beep()) return;
    M5Cardputer.Speaker.tone(freq, ms);
}

}  // namespace cg
