/**
 * UI.h — drawing surface and widget kit.
 *
 * Every screen renders into one 240x135 16-bit sprite and is blitted in a
 * single push. The v1 firmware drew straight to the panel and called
 * fillScreen() on each frame, which is what made live screens flicker.
 * 240*135*2 = 64.8 kB, which fits comfortably in the ~290 kB of heap the
 * sketch leaves free (there is no PSRAM on the Stamp-S3A).
 *
 * If the sprite cannot be allocated we fall back to drawing on the panel
 * directly, so a low-memory board still boots and is merely uglier.
 */

#pragma once
#include <M5Cardputer.h>
#include "Theme.h"

namespace cg {

class UI {
public:
    bool begin();

    // The draw target. Always valid.
    LovyanGFX& g() { return *_g; }

    void clear(uint16_t col = C_BG);
    void push();                       // blit the frame

    // Raw RGB565 frame, row-major, SCR_W * SCR_H, or nullptr when the sprite
    // could not be allocated and we are drawing on the panel directly.
    const uint16_t* frame() const {
        return _sprite ? (const uint16_t*)_canvas.getBuffer() : nullptr;
    }

    // ── Chrome ────────────────────────────────────────────────────────────
    // Draws the title bar and returns the first free body row.
    int  header(const char* title, const char* right = nullptr,
                uint16_t accent = C_HDR);
    void footer(const char* hints);
    void footerf(const char* fmt, ...);

    // ── Text helpers ──────────────────────────────────────────────────────
    void text(int x, int y, uint16_t col, const char* s);
    void textf(int x, int y, uint16_t col, const char* fmt, ...);
    void textBig(int x, int y, uint16_t col, uint8_t size, const char* s);
    void textBigf(int x, int y, uint16_t col, uint8_t size, const char* fmt, ...);
    void textRight(int xRight, int y, uint16_t col, const char* s);
    void textCenter(int cx, int y, uint16_t col, const char* s);

    // ── Widgets ───────────────────────────────────────────────────────────
    void panel(int x, int y, int w, int h, uint16_t fill = C_PANEL,
               uint16_t border = C_LINE);
    void chip(int x, int y, const char* label, uint16_t fg, uint16_t bg);
    int  chipW(const char* label) const;
    void hbar(int x, int y, int w, int h, float pct, uint16_t col,
              uint16_t bg = C_PANEL2);
    void levelDot(int x, int y, bool high);
    void state(int x, int y, bool high);            // HIGH / LOW pill
    void scrollbar(int x, int y, int h, int offset, int visible, int total);
    void divider(int y, uint16_t col = C_LINE);

    // Full-width selectable row, used by every list screen.
    void listRow(int y, int h, bool selected, uint16_t accent = C_CURSOR);

    // Transient banner, drawn over whatever is already in the frame.
    void toast(const char* msg, uint16_t col = C_INFO);

    // Modal box; returns the y of its first content line.
    int  modal(const char* title, int w, int h, uint16_t accent = C_INFO);

    // ── Notifications ─────────────────────────────────────────────────────
    // Short non-blocking message shown by the shell for ~1.4 s.
    void notify(const char* fmt, ...);
    void drawNotification();          // called by the shell after tool draw
    void beep(uint16_t freq = 2200, uint16_t ms = 25);

private:
    M5Canvas       _canvas{&M5Cardputer.Display};
    LovyanGFX*     _g      = nullptr;
    bool           _sprite  = false;

    char     _note[48] = {};
    uint32_t _noteUntil = 0;
};

extern UI ui;

}  // namespace cg
