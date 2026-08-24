#include "WebPortal.h"

#if CG_ENABLE_WEB

#include "Json.h"
#include "Mirror.h"
#include "WebAssets.h"
#include "../core/Board.h"
#include "../core/Keys.h"
#include "../core/Logger.h"
#include "../core/Pins.h"
#include "../core/Settings.h"
#include "../core/UI.h"
#include "../ui/Shell.h"
#include "../tools/ToolSetups.h"
#include <ESPmDNS.h>
#include <SD.h>
#include <esp_wifi.h>

namespace cg {

WebPortal portal;

// ── Small helpers ─────────────────────────────────────────────────────────
static const char* dirName(RoleDir d) { return roleDirName(d); }

static bool argBool(WebServer& s, const char* k, bool dflt) {
    if (!s.hasArg(k)) return dflt;
    String v = s.arg(k);
    return v == "1" || v == "true" || v == "on" || v == "yes";
}

static int argInt(WebServer& s, const char* k, int dflt) {
    return s.hasArg(k) ? s.arg(k).toInt() : dflt;
}

void WebPortal::sendJson(const char* body) {
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", body);
}

void WebPortal::sendJson(const Json& j) {
    if (j.truncated()) { sendErr(500, "response did not fit its buffer"); return; }
    sendJson(j.c_str());
}

void WebPortal::sendOk(const char* msg) {
    char buf[128];
    Json j(buf, sizeof(buf));
    j.objOpen().kv("ok", true).kv("msg", msg).objClose();
    sendJson(buf);
}

void WebPortal::sendErr(int code, const char* msg) {
    char buf[160];
    Json j(buf, sizeof(buf));
    j.objOpen().kv("ok", false).kv("error", msg).objClose();
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(code, "application/json", buf);
}

void WebPortal::chunkBegin() {
    _server.sendHeader("Cache-Control", "no-store");
    _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    _server.send(200, "application/json", "");
}

void WebPortal::chunkEnd() { _server.sendContent(""); }

// ── Lifecycle ─────────────────────────────────────────────────────────────
void WebPortal::ensureApPassword() {
    if (strlen(settings.apPass()) >= 8) return;

    // Derived from hardware entropy, not the MAC -- the MAC is printed in
    // the SSID for anyone in range to read.
    static const char AL[] = "abcdefghijkmnpqrstuvwxyz23456789";
    char pw[11];
    for (int i = 0; i < 10; i++) pw[i] = AL[esp_random() % (sizeof(AL) - 1)];
    pw[10] = 0;
    settings.setApPass(pw);
}

bool WebPortal::start(uint8_t mode) {
    if (_running) stop();
    if (mode != Settings::NET_AP && mode != Settings::NET_STA) return false;

    if (mode == Settings::NET_AP) {
        ensureApPassword();
        uint8_t mac[6] = {};
        WiFi.macAddress(mac);
        snprintf(_apSsid, sizeof(_apSsid), "Cardputer-%02X%02X", mac[4], mac[5]);

        WiFi.mode(WIFI_AP);
        if (!WiFi.softAP(_apSsid, settings.apPass())) return false;
    } else {
        if (strlen(settings.wifiSsid()) == 0) return false;
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(CG_HOSTNAME);
        WiFi.begin(settings.wifiSsid(), settings.wifiPass());
        // Association is left to run in the background; joined() reports it.
    }

    // Latency beats power here: with modem sleep on, a keypress from the
    // browser can wait 100 ms for the next beacon.
    WiFi.setSleep(false);

    _mode    = mode;
    _running = true;
    boardSetRadioActive(true);

    if (!_routed) { routes(); _routed = true; }
    if (!_started) { _server.begin(); _started = true; }
    MDNS.end();
    MDNS.begin(CG_HOSTNAME);
    MDNS.addService("http", "tcp", 80);

    mirror.invalidate();
    return true;
}

void WebPortal::stop() {
    if (!_running) return;
    MDNS.end();
    if (_started) { _server.stop(); _started = false; }
    WiFi.disconnect(true, false);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    _running = false;
    _mode    = Settings::NET_OFF;
    boardSetRadioActive(false);
}

void WebPortal::begin() {
    if (settings.wifiAuto() && settings.wifiMode() != Settings::NET_OFF)
        start(settings.wifiMode());
}

void WebPortal::loop() {
    if (!_started) return;
    _server.handleClient();
}

bool WebPortal::joined() const {
    return _running && _mode == Settings::NET_STA && WiFi.status() == WL_CONNECTED;
}

const char* WebPortal::ssid() const {
    if (!_running) return "";
    return (_mode == Settings::NET_AP) ? _apSsid : settings.wifiSsid();
}

const char* WebPortal::pass() const {
    if (!_running) return "";
    return (_mode == Settings::NET_AP) ? settings.apPass() : "";
}

String WebPortal::ip() const {
    if (!_running) return String("");
    if (_mode == Settings::NET_AP) return WiFi.softAPIP().toString();
    return joined() ? WiFi.localIP().toString() : String("");
}

const char* WebPortal::hostname() const { return CG_HOSTNAME; }

int WebPortal::clients() const {
    return (_running && _mode == Settings::NET_AP) ? WiFi.softAPgetStationNum() : -1;
}

int WebPortal::rssi() const { return joined() ? WiFi.RSSI() : 0; }

const char* WebPortal::statusText() const {
    if (!_running) return "off";
    if (_mode == Settings::NET_AP) return "access point";
    switch (WiFi.status()) {
        case WL_CONNECTED:       return "joined";
        case WL_NO_SSID_AVAIL:   return "network not found";
        case WL_CONNECT_FAILED:  return "wrong password";
        case WL_IDLE_STATUS:     return "connecting";
        case WL_DISCONNECTED:    return "connecting";
        default:                 return "connecting";
    }
}

// ── Routes ────────────────────────────────────────────────────────────────
void WebPortal::routes() {
    _server.on("/",                  HTTP_GET,  [this] { hIndex(); });
    _server.on("/api/state",         HTTP_GET,  [this] { hState(); });
    _server.on("/api/tools",         HTTP_GET,  [this] { hTools(); });
    _server.on("/api/tool/open",     HTTP_POST, [this] { hToolOpen(); });
    _server.on("/api/tool/start",    HTTP_POST, [this] { hToolStart(); });
    _server.on("/api/tool/back",     HTTP_POST, [this] { hToolBack(); });
    _server.on("/api/pins",          HTTP_GET,  [this] { hPins(); });
    _server.on("/api/pin",           HTTP_POST, [this] { hPinSet(); });
    _server.on("/api/live",          HTTP_GET,  [this] { hLive(); });
    _server.on("/api/key",           HTTP_POST, [this] { hKey(); });
    _server.on("/api/screen",        HTTP_GET,  [this] { hScreen(); });
    _server.on("/api/settings",      HTTP_GET,  [this] { hSettingsGet(); });
    _server.on("/api/settings",      HTTP_POST, [this] { hSettingsSet(); });
    _server.on("/api/setups",        HTTP_GET,  [this] { hSetups(); });
    _server.on("/api/setup",         HTTP_POST, [this] { hSetupAct(); });
    _server.on("/api/board",         HTTP_GET,  [this] { hBoard(); });
    _server.on("/api/logs",          HTTP_GET,  [this] { hLogs(); });
    _server.on("/api/log",           HTTP_GET,  [this] { hLogDownload(); });
    _server.on("/api/log/ctl",       HTTP_POST, [this] { hLogControl(); });
    _server.on("/api/wifi",          HTTP_POST, [this] { hWifiSet(); });
    _server.on("/favicon.ico", HTTP_GET, [this] { _server.send(204); });
    _server.onNotFound([this] { hNotFound(); });
}

void WebPortal::hIndex() {
    _requests++;
    _lastReq = millis();
    _server.sendHeader("Content-Encoding", "gzip");
    _server.sendHeader("Cache-Control", "public, max-age=600");
    _server.send_P(200, "text/html", (const char*)WEB_INDEX_GZ, WEB_INDEX_GZ_LEN);
}

void WebPortal::hNotFound() {
    // Anything unknown goes to the app, so a bookmarked deep link still works.
    if (_server.uri().startsWith("/api/")) { sendErr(404, "no such endpoint"); return; }
    hIndex();
}

// ── State ─────────────────────────────────────────────────────────────────
void WebPortal::hState() {
    _requests++;
    _lastReq = millis();

    char buf[1100];
    Json j(buf, sizeof(buf));
    j.objOpen();
    j.kv("board", boardName());
    j.kv("adv", boardIsAdv());
    j.kv("state", shell.stateName());
    j.kv("heap", (unsigned long)ESP.getFreeHeap());
    j.kv("uptime", (unsigned long)(millis() / 1000));
    j.kv("batt", M5.Power.getBatteryLevel());
    j.kv("radio", boardRadioActive());
    j.kv("sdPins", settings.allowSdPins());

    Tool* t = shell.activeTool();
    if (t) {
        j.obj("tool");
        j.kv("id", t->id());
        j.kv("name", t->name());
        j.kv("blurb", t->blurb());
        j.kv("cat", catName(t->cat()));
        j.kv("drives", t->drivesOutputs());
        j.kv("canLog", t->canLog());
        j.objClose();
    } else {
        j.kvNull("tool");
    }

    j.obj("wifi");
    j.kv("mode", (int)_mode);
    j.kv("running", _running);
    j.kv("ssid", ssid());
    j.kv("staSsid", settings.wifiSsid());
    j.kv("ip", ip().c_str());
    j.kv("host", hostname());
    j.kv("clients", clients());
    j.kv("rssi", rssi());
    j.kv("status", statusText());
    j.kv("auto", settings.wifiAuto());
    j.objClose();

    j.obj("log");
    j.kv("active", logger.active());
    j.kv("rows", (unsigned long)logger.rows());
    j.kv("secs", (unsigned long)logger.seconds());
    j.kv("file", logger.fileName());
    j.kv("interval", (unsigned long)logger.interval());
    j.objClose();

    j.objClose();
    sendJson(j);
}

// ── Tools ─────────────────────────────────────────────────────────────────
void WebPortal::hTools() {
    _requests++;
    chunkBegin();
    _server.sendContent("[");

    for (int i = 0; i < shell.toolCount(); i++) {
        Tool* t = shell.toolAt(i);
        if (!t) continue;

        char buf[1300];
        Json j(buf, sizeof(buf));
        j.objOpen();
        j.kv("id", t->id());
        j.kv("name", t->name());
        j.kv("blurb", t->blurb());
        j.kv("cat", catName(t->cat()));
        j.kv("drives", t->drivesOutputs());
        j.kv("canLog", t->canLog());
        j.arr("roles");
        for (int r = 0; r < t->roleCount(); r++) {
            const Role& role = t->roles()[r];
            int g = t->pin(r);
            j.objOpen();
            j.kv("i", r);
            j.kv("label", role.label);
            j.kv("dir", dirName(role.dir));
            j.kv("hint", role.hint);
            j.kv("pin", g);
            j.kv("ok", g >= 0 && (role.dir == RoleDir::Adc ? pinAdcOk(g) : pinGpioOk(g)));
            j.objClose();
        }
        j.arrClose();
        j.objClose();

        if (i) _server.sendContent(",");
        _server.sendContent(j.truncated() ? "{\"id\":\"?\",\"name\":\"(too long)\",\"roles\":[]}"
                                          : buf);
    }

    _server.sendContent("]");
    chunkEnd();
}

void WebPortal::hToolOpen() {
    _requests++;
    if (!_server.hasArg("id")) { sendErr(400, "id required"); return; }
    if (!shell.openToolById(_server.arg("id").c_str())) {
        sendErr(404, "no such tool");
        return;
    }
    mirror.invalidate();
    sendOk("opened");
}

void WebPortal::hToolStart() {
    _requests++;
    Tool* t = shell.activeTool();
    if (!t) { sendErr(409, "no tool is open"); return; }

    if (argBool(_server, "confirm", false)) {
        if (strcmp(shell.stateName(), "arm") != 0) {
            sendErr(409, "nothing is waiting to be armed");
            return;
        }
        // Confirming the arm prompt goes through the very same key path the
        // device uses, so there is one implementation of "yes, drive it".
        KeyEvent ev;
        ev.key = Key::Enter;
        keys.inject(ev);
        sendOk("armed");
        return;
    }

    bool started = shell.startActiveTool();
    mirror.invalidate();
    char buf[128];
    Json j(buf, sizeof(buf));
    j.objOpen().kv("ok", true).kv("started", started)
     .kv("needsArm", !started).objClose();
    sendJson(buf);
}

void WebPortal::hToolBack() {
    _requests++;
    shell.backToMenu();
    mirror.invalidate();
    sendOk("menu");
}

// ── Pins ──────────────────────────────────────────────────────────────────
void WebPortal::hPins() {
    _requests++;
    chunkBegin();
    _server.sendContent("[");

    const PinInfo* tbl = pinTable();
    for (int i = 0; i < pinCount(); i++) {
        const PinInfo& p = tbl[i];
        char buf[440];
        Json j(buf, sizeof(buf));
        j.objOpen();
        j.kv("gpio", (int)p.gpio);
        j.kv("header", p.header);
        j.kv("silk", p.silk);
        j.kv("grove", (bool)(p.flags & PF_GROVE));
        j.kv("ext", (bool)(p.flags & PF_EXT));
        j.kv("sd", (bool)(p.flags & PF_SD));
        j.kv("locked", (bool)(p.flags & PF_LOCKED));
        j.kv("adc1", (bool)(p.flags & PF_ADC1));
        j.kv("adc2", (bool)(p.flags & PF_ADC2));
        j.kv("gpioOk", pinGpioOk(p.gpio));
        j.kv("adcOk", pinAdcOk(p.gpio));
        j.kv("mode", pins.modeName(p.gpio));
        j.kv("warn", p.warn ? p.warn : "");

        PMode m = pins.mode(p.gpio);
        if (m == PMode::Adc) j.kv("mv", (unsigned long)pins.adcMilliVolts(p.gpio));
        else                 j.kvNull("mv");
        if (m == PMode::In || m == PMode::InPull || m == PMode::InPulld)
            j.kv("level", pins.read(p.gpio));
        else if (m == PMode::Out)
            j.kv("level", pins.level(p.gpio));
        else
            j.kvNull("level");
        j.objClose();

        if (i) _server.sendContent(",");
        _server.sendContent(j.truncated() ? "{\"gpio\":-1,\"header\":\"(too long)\"}"
                                          : buf);
    }

    _server.sendContent("]");
    chunkEnd();
}

void WebPortal::hPinSet() {
    _requests++;
    if (!_server.hasArg("tool") || !_server.hasArg("role")) {
        sendErr(400, "tool and role required");
        return;
    }
    String id = _server.arg("tool");
    int role  = argInt(_server, "role", -1);
    int gpio  = argInt(_server, "gpio", -1);

    Tool* target = nullptr;
    for (int i = 0; i < shell.toolCount(); i++) {
        Tool* t = shell.toolAt(i);
        if (t && id == t->id()) { target = t; break; }
    }
    if (!target) { sendErr(404, "no such tool"); return; }
    if (role < 0 || role >= target->roleCount()) { sendErr(400, "role out of range"); return; }

    if (gpio >= 0) {
        const Role& r = target->roles()[role];
        bool ok = (r.dir == RoleDir::Adc) ? pinAdcOk(gpio) : pinGpioOk(gpio);
        if (!ok) {
            // Say which of the reasons applies rather than a bare refusal --
            // this is exactly the question someone has when a pin is greyed.
            const PinInfo* pi = pinInfo(gpio);
            if (!pi)                              sendErr(400, "pin is not on a header");
            else if (pi->flags & PF_LOCKED)       sendErr(400, "system I2C bus, never available");
            else if ((pi->flags & PF_SD) && !settings.allowSdPins())
                                                  sendErr(400, "microSD pin; enable 'allow SD pins' first");
            else if (r.dir == RoleDir::Adc && (pi->flags & PF_ADC2) && boardRadioActive())
                                                  sendErr(400, "ADC2 cannot be read while WiFi is on");
            else if (r.dir == RoleDir::Adc)       sendErr(400, "this pin has no ADC");
            else                                  sendErr(400, "pin unavailable");
            return;
        }
    }

    target->setPin(role, gpio);
    settings.setPin(target->id(), role, gpio);
    sendOk("assigned");
}

// ── Live values ───────────────────────────────────────────────────────────
// Reuses the CSV logging hooks the tools already implement, so a browser gets
// live numbers out of every tool that can produce them with no extra per-tool
// code -- and the columns match the CSV exactly.
void WebPortal::hLive() {
    _requests++;
    Tool* t = shell.activeTool();

    char buf[640];
    Json j(buf, sizeof(buf));
    j.objOpen();
    if (!t || !t->canLog()) {
        j.kv("has", false).objClose();
        sendJson(j);
        return;
    }

    char row[160] = {};
    bool got = t->logRow(row, sizeof(row));

    j.kv("has", true);
    j.kv("tool", t->id());
    j.kv("header", t->logHeader());
    j.kv("row", got ? row : "");
    j.kv("valid", got);
    j.objClose();
    sendJson(j);
}

// ── Key injection ─────────────────────────────────────────────────────────
void WebPortal::hKey() {
    _requests++;
    if (!_server.hasArg("k")) { sendErr(400, "k required"); return; }
    String k = _server.arg("k");

    KeyEvent ev;
    ev.ctrl  = argBool(_server, "ctrl", false);
    ev.shift = argBool(_server, "shift", false);
    ev.alt   = argBool(_server, "alt", false);

    if      (k == "up")    ev.key = Key::Up;
    else if (k == "down")  ev.key = Key::Down;
    else if (k == "left")  ev.key = Key::Left;
    else if (k == "right") ev.key = Key::Right;
    else if (k == "enter") ev.key = Key::Enter;
    else if (k == "back")  ev.key = Key::Back;
    else if (k == "esc")   ev.key = Key::Esc;
    else if (k == "tab")   ev.key = Key::Tab;
    else if (k == "fdel")  ev.key = Key::FwdDel;
    else if (k.startsWith("f")) {
        int n = k.substring(1).toInt();
        if (n < 1 || n > 10) { sendErr(400, "f1..f10 only"); return; }
        ev.key = Key::Fkey;
        ev.num = (uint8_t)n;
    } else if (k == "char") {
        int c = argInt(_server, "c", 0);
        if (c < 32 || c > 126) { sendErr(400, "printable characters only"); return; }
        ev.key = Key::Char;
        ev.ch  = (char)c;
    } else {
        sendErr(400, "unknown key");
        return;
    }

    keys.inject(ev);
    sendOk("sent");
}

// ── Framebuffer ───────────────────────────────────────────────────────────
void WebPortal::hScreen() {
    _requests++;
    _lastReq = millis();

    if (!mirror.available()) { sendErr(503, "no sprite to mirror"); return; }

    int n = mirror.scan(argBool(_server, "full", false));

    _server.sendHeader("Cache-Control", "no-store");
    _server.setContentLength(mirror.payloadSize());
    _server.send(200, "application/octet-stream", "");

    uint8_t hdr[Mirror::HEADER_BYTES];
    mirror.header(hdr);
    _server.sendContent((const char*)hdr, sizeof(hdr));

    // One tile at a time: 482 bytes of stack instead of a 64 kB allocation
    // that would fail exactly when the heap is under pressure.
    uint8_t tile[2 + Mirror::TILE_BYTES];
    for (int i = 0; i < n; i++) {
        mirror.tile(i, tile);
        _server.sendContent((const char*)tile, sizeof(tile));
    }
}

// ── Settings ──────────────────────────────────────────────────────────────
void WebPortal::hSettingsGet() {
    _requests++;
    char buf[420];
    Json j(buf, sizeof(buf));
    j.objOpen();
    j.kv("brightness", (int)settings.brightness());
    j.kv("beep", settings.beep());
    j.kv("armOutputs", settings.armOutputs());
    j.kv("allowSdPins", settings.allowSdPins());
    j.kv("logInterval", (unsigned long)logger.interval());
    j.objClose();
    sendJson(j);
}

void WebPortal::hSettingsSet() {
    _requests++;
    if (_server.hasArg("brightness"))
        settings.setBrightness((uint8_t)argInt(_server, "brightness", settings.brightness()));
    if (_server.hasArg("beep"))
        settings.setBeep(argBool(_server, "beep", settings.beep()));
    if (_server.hasArg("armOutputs"))
        settings.setArmOutputs(argBool(_server, "armOutputs", settings.armOutputs()));
    if (_server.hasArg("allowSdPins"))
        settings.setAllowSdPins(argBool(_server, "allowSdPins", settings.allowSdPins()));
    if (_server.hasArg("logInterval"))
        logger.setInterval((uint32_t)argInt(_server, "logInterval", (int)logger.interval()));
    sendOk("saved");
}

// ── Setups ────────────────────────────────────────────────────────────────
void WebPortal::hSetups() {
    _requests++;
    toolSetups.refresh();

    char buf[640];
    Json j(buf, sizeof(buf));
    j.arrOpen();
    for (int i = 0; i < ToolSetups::SLOTS; i++) {
        j.objOpen();
        j.kv("slot", i);
        j.kv("used", toolSetups.slotUsed(i));
        j.kv("name", toolSetups.slotName(i));
        j.kv("tools", toolSetups.slotTools(i));
        j.objClose();
    }
    j.arrClose();
    sendJson(j);
}

void WebPortal::hSetupAct() {
    _requests++;
    int slot = argInt(_server, "slot", -1);
    if (slot < 0 || slot >= ToolSetups::SLOTS) { sendErr(400, "bad slot"); return; }
    String action = _server.arg("action");

    if (action == "save") {
        String name = _server.hasArg("name") ? _server.arg("name") : String("unnamed");
        toolSetups.save(slot, name.c_str());
        sendOk("saved");
    } else if (action == "load") {
        if (!toolSetups.load(slot)) { sendErr(404, "slot is empty"); return; }
        sendOk("loaded");
    } else if (action == "delete") {
        toolSetups.erase(slot);
        sendOk("deleted");
    } else {
        sendErr(400, "action must be save, load or delete");
    }
}

// ── Board reference ───────────────────────────────────────────────────────
void WebPortal::hBoard() {
    _requests++;
    chunkBegin();

    {
        char buf[220];
        Json j(buf, sizeof(buf));
        j.objOpen();
        j.kv("name", boardName());
        j.kv("adv", boardIsAdv());
        j.kv("flashMB", (int)(ESP.getFlashChipSize() / (1024 * 1024)));
        j.kv("psramKB", (int)(ESP.getPsramSize() / 1024));
        j.kv("heap", (unsigned long)ESP.getFreeHeap());
        _server.sendContent(buf);        // deliberately left open
    }
    _server.sendContent(",\"periph\":[");
    for (int i = 0; i < periphCount(); i++) {
        const PeriphInfo& p = periphTable()[i];
        char buf[160];
        Json j(buf, sizeof(buf));
        j.objOpen().kv("what", p.what).kv("pins", p.pins).objClose();
        if (i) _server.sendContent(",");
        _server.sendContent(buf);
    }
    _server.sendContent("]}");
    chunkEnd();
}

// ── Logs ──────────────────────────────────────────────────────────────────
void WebPortal::hLogs() {
    _requests++;
    if (!logger.browseBegin()) {
        char buf[160];
        Json j(buf, sizeof(buf));
        j.objOpen().kv("ok", false).kv("error", logger.lastError())
         .arr("files").arrClose().objClose();
        sendJson(j);
        return;
    }

    chunkBegin();
    _server.sendContent("{\"ok\":true,\"files\":[");

    File root = SD.open("/");
    int n = 0;
    while (root) {
        File f = root.openNextFile();
        if (!f) break;
        String nm = String(f.name());
        if (!f.isDirectory() && (nm.endsWith(".csv") || nm.endsWith(".CSV"))) {
            char buf[160];
            Json j(buf, sizeof(buf));
            j.objOpen().kv("name", nm.c_str()).kv("size", (unsigned long)f.size()).objClose();
            if (n++) _server.sendContent(",");
            _server.sendContent(buf);
        }
        f.close();
    }
    if (root) root.close();

    _server.sendContent("]}");
    chunkEnd();
    logger.browseEnd();
}

void WebPortal::hLogDownload() {
    _requests++;
    if (!_server.hasArg("f")) { sendErr(400, "f required"); return; }

    String name = _server.arg("f");
    // Only files in the root, and only ours: no traversal, no surprises.
    if (name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 || name.indexOf("..") >= 0) {
        sendErr(400, "bad filename");
        return;
    }
    if (!logger.browseBegin()) { sendErr(503, logger.lastError()); return; }

    String path = "/" + name;
    File f = SD.open(path, FILE_READ);
    if (!f) { logger.browseEnd(); sendErr(404, "no such file"); return; }

    _server.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
    _server.streamFile(f, "text/csv");
    f.close();
    logger.browseEnd();
}

void WebPortal::hLogControl() {
    _requests++;
    String action = _server.arg("action");

    if (action == "stop") {
        logger.stop();
        sendOk("stopped");
        return;
    }
    if (action != "start") { sendErr(400, "action must be start or stop"); return; }

    Tool* t = shell.activeTool();
    if (!t || !t->canLog()) { sendErr(409, "the open tool has nothing to log"); return; }
    if (!logger.start(t->id(), t->logHeader(), LogSink::Both)) {
        sendErr(500, logger.lastError());
        return;
    }
    char buf[200];
    Json j(buf, sizeof(buf));
    j.objOpen().kv("ok", true).kv("file", logger.fileName())
     .kv("sd", (bool)((uint8_t)logger.sink() & (uint8_t)LogSink::Sd)).objClose();
    sendJson(j);
}

// ── WiFi configuration ────────────────────────────────────────────────────
void WebPortal::hWifiSet() {
    _requests++;

    if (_server.hasArg("ssid"))
        settings.setWifiCreds(_server.arg("ssid").c_str(),
                              _server.hasArg("pass") ? _server.arg("pass").c_str()
                                                     : settings.wifiPass());
    if (_server.hasArg("auto"))
        settings.setWifiAuto(argBool(_server, "auto", settings.wifiAuto()));

    if (_server.hasArg("mode")) {
        int m = argInt(_server, "mode", settings.wifiMode());
        settings.setWifiMode((uint8_t)m);
        // Answer before tearing the radio down, or the reply never arrives.
        sendOk(m == Settings::NET_OFF ? "radio stopping" : "mode saved, restarting radio");
        _server.client().stop();
        if (m == Settings::NET_OFF) stop();
        else                         start((uint8_t)m);
        return;
    }
    sendOk("saved");
}

}  // namespace cg

#endif  // CG_ENABLE_WEB
