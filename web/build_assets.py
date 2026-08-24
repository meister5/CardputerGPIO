#!/usr/bin/env python3
"""
Turns web/index.html into src/net/WebAssets.h.

The page is gzipped and stored in flash as a byte array; the server sends it
with Content-Encoding: gzip and every browser unpacks it. That is about 8 kB
of flash for the whole interface, and it means the firmware is a single file
with nothing to upload to a filesystem partition separately.

Run this after editing web/index.html:

    python3 web/build_assets.py
"""

import gzip
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "web" / "index.html"
DST = ROOT / "src" / "net" / "WebAssets.h"

raw = SRC.read_bytes()
# mtime=0 so the output is reproducible and the diff stays empty when the
# page has not actually changed.
gz = gzip.compress(raw, compresslevel=9, mtime=0)

lines = []
for i in range(0, len(gz), 16):
    chunk = ", ".join(f"0x{b:02x}" for b in gz[i:i + 16])
    lines.append("    " + chunk + ",")

DST.write_text(f"""/**
 * WebAssets.h — the web interface, gzipped into flash.
 *
 * GENERATED FILE, DO NOT EDIT. The source is web/index.html; regenerate with
 *
 *     python3 web/build_assets.py
 *
 * Raw {len(raw)} bytes, gzipped {len(gz)} bytes.
 */

#pragma once
#include <stdint.h>
#include <pgmspace.h>

namespace cg {{

static const uint32_t WEB_INDEX_GZ_LEN = {len(gz)};

static const uint8_t WEB_INDEX_GZ[] PROGMEM = {{
{chr(10).join(lines)}
}};

}}  // namespace cg
""")

print(f"{SRC.name}: {len(raw)} bytes -> {len(gz)} gzipped "
      f"({100 * len(gz) / len(raw):.0f}%) -> {DST.relative_to(ROOT)}")
