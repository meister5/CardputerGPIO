/**
 * WebPortal.h — the web interface: WiFi, an HTTP server, and the API behind it.
 *
 * Two ways to reach it:
 *
 *   AP    the Cardputer becomes the access point. Nothing else is needed --
 *         no router, no internet -- so this works on a bench with no network.
 *   STA   it joins a network you already have, and appears at
 *         http://cardputer.local from any machine on that network.
 *
 * Off by default. Turning the radio on has two costs that the rest of the
 * firmware has to know about, so this is a deliberate act rather than
 * something that happens at boot unless you ask for it:
 *
 *   - ADC2 (G11-G20 -- here G13 and G15) shares hardware with the radio and
 *     cannot be read while it is up. Board withdraws those pins from the ADC
 *     pool for as long as the portal runs.
 *   - The radio task adds interrupt load. The tight timing loops in the
 *     logic analyser and the 1-Wire/DHT tool already run with interrupts off
 *     on their own core, so the effect is small, but it is not zero.
 *
 * The browser drives the same state machine the keyboard does, and sees the
 * same framebuffer -- see Mirror.h. There is no second implementation of any
 * tool, which is the only way a duplicate interface stays a duplicate.
 */

#pragma once
#include "../core/Config.h"

#if CG_ENABLE_WEB

#include <stdint.h>
#include "Json.h"
#include <WiFi.h>
#include <WebServer.h>

namespace cg {

class WebPortal {
public:
    // Reads the saved mode and starts the portal if autostart is on.
    void begin();

    // Service the server. Cheap when the radio is off.
    void loop();

    bool start(uint8_t mode);          // Settings::NET_AP or NET_STA
    void stop();

    bool     running() const { return _running; }
    uint8_t  mode()    const { return _mode; }
    bool     joined()  const;          // STA: actually associated

    const char* ssid() const;
    const char* pass() const;
    String      ip() const;
    const char* hostname() const;
    int         clients() const;       // AP mode only, -1 otherwise
    int         rssi() const;          // STA mode only, 0 otherwise
    const char* statusText() const;

    uint32_t requests() const { return _requests; }
    uint32_t lastRequestMs() const { return _lastReq; }

private:
    WebServer _server{80};
    bool      _running  = false;
    bool      _routed   = false;       // handlers registered (once, ever)
    bool      _started  = false;       // the socket is bound and listening
    uint8_t   _mode     = 0;
    uint32_t  _requests = 0;
    uint32_t  _lastReq  = 0;
    char      _apSsid[24] = {};

    void routes();
    void ensureApPassword();

    // ── Handlers ──────────────────────────────────────────────────────────
    void hIndex();
    void hNotFound();
    void hState();
    void hTools();
    void hToolOpen();
    void hToolStart();
    void hToolBack();
    void hPins();
    void hPinSet();
    void hLive();
    void hKey();
    void hScreen();
    void hSettingsGet();
    void hSettingsSet();
    void hSetups();
    void hSetupAct();
    void hBoard();
    void hLogs();
    void hLogDownload();
    void hLogControl();
    void hWifiSet();

    void sendJson(const char* body);
    // Refuses to send a response that overran its buffer: half a JSON
    // document parses as nothing useful and is a miserable thing to debug.
    void sendJson(const Json& j);
    void sendOk(const char* msg = "ok");
    void sendErr(int code, const char* msg);
    void chunkBegin();
    void chunkEnd();
};

extern WebPortal portal;

}  // namespace cg

#endif  // CG_ENABLE_WEB
