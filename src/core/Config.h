/**
 * Config.h — build-time switches.
 */

#pragma once

// ── Web interface ─────────────────────────────────────────────────────────
// Brings in WiFi, the HTTP server and mDNS. That costs about 630 kB of flash,
// which does NOT fit the board's default 1.2 MB app partition: building with
// this on requires Tools -> Partition Scheme -> "8M with spiffs (3MB APP)".
//
// Set to 0 for the lean, radio-silent build that fits the default partition.
// Everything else in the firmware is identical either way.
#ifndef CG_ENABLE_WEB
#define CG_ENABLE_WEB 1
#endif

// Default hostname; the portal is reachable at http://<this>.local
#define CG_HOSTNAME "cardputer"
