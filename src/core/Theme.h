/**
 * Theme.h — RGB565 palette and layout metrics.
 *
 * Everything is drawn into a 240x135 16-bit sprite, so colours are RGB565
 * literals rather than the 24-bit values M5GFX accepts on the live display.
 */

#pragma once
#include <stdint.h>

namespace cg {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// ── Surfaces ──────────────────────────────────────────────────────────────
constexpr uint16_t C_BG      = rgb(  8, 10, 18);
constexpr uint16_t C_PANEL   = rgb( 20, 26, 40);
constexpr uint16_t C_PANEL2  = rgb( 30, 38, 56);
constexpr uint16_t C_HDR     = rgb( 22, 52, 92);
constexpr uint16_t C_FTR     = rgb( 14, 18, 28);
constexpr uint16_t C_LINE    = rgb( 48, 58, 80);

// ── Text ──────────────────────────────────────────────────────────────────
constexpr uint16_t C_TITLE   = rgb(255,212, 64);
constexpr uint16_t C_TEXT    = rgb(214,220,232);
constexpr uint16_t C_DIM     = rgb(118,128,148);
constexpr uint16_t C_FAINT   = rgb( 72, 80, 98);
constexpr uint16_t C_BLACK   = rgb(  0,  0,  0);
constexpr uint16_t C_WHITE   = rgb(255,255,255);

// ── Semantic ──────────────────────────────────────────────────────────────
constexpr uint16_t C_HIGH    = rgb( 42,224,132);   // logic HIGH / ok
constexpr uint16_t C_LOW     = rgb(240, 78, 78);   // logic LOW / error
constexpr uint16_t C_WARN    = rgb(255,164, 42);
constexpr uint16_t C_INFO    = rgb( 62,176,255);
constexpr uint16_t C_ACCENT  = rgb(150,110,255);
constexpr uint16_t C_SEL     = rgb( 30, 74,124);
constexpr uint16_t C_CURSOR  = rgb( 42,224,132);

// ── Pin-role colours (shared by wiring guide, picker, dashboard) ──────────
constexpr uint16_t C_ROLE_OUT = rgb( 42,224,132);
constexpr uint16_t C_ROLE_IN  = rgb( 62,176,255);
constexpr uint16_t C_ROLE_ADC = rgb(255,164, 42);
constexpr uint16_t C_ROLE_PWM = rgb(150,110,255);
constexpr uint16_t C_ROLE_BUS = rgb(255,110,190);
constexpr uint16_t C_WAVE_BG  = rgb( 44, 52, 70);

// ── Layout ────────────────────────────────────────────────────────────────
constexpr int SCR_W  = 240;
constexpr int SCR_H  = 135;
constexpr int HDR_H  = 15;
constexpr int FTR_H  = 11;
constexpr int BODY_Y = HDR_H + 1;
constexpr int BODY_H = SCR_H - HDR_H - FTR_H - 2;
constexpr int BODY_B = BODY_Y + BODY_H;   // first row below the body

// Font0 glyph metrics at size 1.
constexpr int CH_W = 6;
constexpr int CH_H = 8;

}  // namespace cg
