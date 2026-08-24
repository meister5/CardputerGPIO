/**
 * Mirror.h — ships the device's framebuffer to a browser.
 *
 * The whole UI already renders into one 240x135 sprite, so mirroring it is
 * what makes the web interface a genuine duplicate rather than a second
 * implementation that drifts: every tool, every dialog and every future
 * screen appears in the browser without writing a line of web code for it.
 *
 * Sending 64.8 kB per frame would not keep up, so the screen is diced into
 * 16x15 tiles and only the tiles that changed since the last request are
 * sent. A typical frame -- a couple of numbers ticking over -- moves two or
 * three tiles, about 1.5 kB.
 *
 * Wire format, little-endian throughout:
 *
 *   magic   'C','G','S'
 *   version 1
 *   cols    u8    15
 *   rows    u8    9
 *   tileW   u8    16
 *   tileH   u8    15
 *   count   u16   number of tiles that follow
 *   then count * { index u16, tileW*tileH * u16 RGB565 }
 */

#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../core/Theme.h"

namespace cg {

class Mirror {
public:
    static constexpr int TILE_W = 16;
    static constexpr int TILE_H = 15;
    static constexpr int COLS   = SCR_W / TILE_W;    // 15
    static constexpr int ROWS   = SCR_H / TILE_H;    // 9
    static constexpr int TILES  = COLS * ROWS;       // 135
    static constexpr int TILE_BYTES = TILE_W * TILE_H * 2;
    static constexpr int HEADER_BYTES = 10;

    void begin();

    // Marks every tile dirty, so the next request redraws the whole screen.
    void invalidate() { _haveHashes = false; }

    // Finds the tiles that changed. Returns how many; the caller then asks
    // for each in turn. force = send everything regardless.
    int  scan(bool force);

    size_t payloadSize() const { return HEADER_BYTES + (size_t)_dirtyN * (2 + TILE_BYTES); }
    void   header(uint8_t* out) const;

    int  dirtyCount() const { return _dirtyN; }
    int  dirtyIndex(int i) const { return _dirty[i]; }

    // Fills dst with 2 + TILE_BYTES: the tile index then its pixels.
    void tile(int i, uint8_t* dst);

    bool available() const;

private:
    uint32_t _hash[TILES]   = {};
    uint16_t _dirty[TILES]  = {};
    int      _dirtyN        = 0;
    bool     _haveHashes    = false;

    static uint32_t hashTile(const uint16_t* fb, int tx, int ty);
};

extern Mirror mirror;

}  // namespace cg
