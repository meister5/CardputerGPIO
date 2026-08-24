#include "Mirror.h"
#include "../core/UI.h"

namespace cg {

Mirror mirror;

void Mirror::begin() { _haveHashes = false; }

bool Mirror::available() const { return ui.frame() != nullptr; }

// Change detection runs straight off the raw buffer: byte order is
// irrelevant when all we are asking is "did these pixels move".
uint32_t Mirror::hashTile(const uint16_t* fb, int tx, int ty) {
    uint32_t h = 2166136261u;                    // FNV-1a
    const uint16_t* p = fb + (size_t)(ty * TILE_H) * SCR_W + tx * TILE_W;
    for (int y = 0; y < TILE_H; y++, p += SCR_W)
        for (int x = 0; x < TILE_W; x++) {
            h ^= p[x];
            h *= 16777619u;
        }
    return h;
}

int Mirror::scan(bool force) {
    _dirtyN = 0;
    const uint16_t* fb = ui.frame();
    if (!fb) return 0;

    bool all = force || !_haveHashes;
    for (int t = 0; t < TILES; t++) {
        uint32_t h = hashTile(fb, t % COLS, t / COLS);
        if (all || h != _hash[t]) {
            _hash[t] = h;
            _dirty[_dirtyN++] = (uint16_t)t;
        }
    }
    _haveHashes = true;
    return _dirtyN;
}

void Mirror::header(uint8_t* out) const {
    out[0] = 'C'; out[1] = 'G'; out[2] = 'S';
    out[3] = 1;
    out[4] = COLS;
    out[5] = ROWS;
    out[6] = TILE_W;
    out[7] = TILE_H;
    out[8] = (uint8_t)(_dirtyN & 0xFF);
    out[9] = (uint8_t)(_dirtyN >> 8);
}

void Mirror::tile(int i, uint8_t* dst) {
    int t  = _dirty[i];
    int tx = t % COLS, ty = t / COLS;

    dst[0] = (uint8_t)(t & 0xFF);
    dst[1] = (uint8_t)(t >> 8);

    // readRect goes through the sprite's own colour converter, so the byte
    // order LovyanGFX happens to keep internally does not matter here. Its
    // 16-bit output is byte-swapped by default and plain RGB565 with
    // setSwapBytes(true) -- pin that down rather than inherit whatever the
    // drawing code last left set.
    LovyanGFX& g = ui.g();
    const bool prev = g.getSwapBytes();
    g.setSwapBytes(true);
    g.readRect(tx * TILE_W, ty * TILE_H, TILE_W, TILE_H, (uint16_t*)(dst + 2));
    g.setSwapBytes(prev);
}

}  // namespace cg
