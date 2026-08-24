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

// The web interface asks for a password only when one has been set. The
// username is fixed: one board, one operator, so a second field would be
// ceremony rather than security.
#define CG_WEB_USER  "cardputer"
#define CG_WEB_REALM "CardputerGPIO"
