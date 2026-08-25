#include "ToolWiFi.h"

#if CG_ENABLE_WEB

#include "../core/Settings.h"
#include "../net/WebPortal.h"
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

namespace cg {

ToolWiFi toolWiFi;

static const char* HELP[] = {
    "  < >    choose a mode",
    "  ENTER  apply it",
    "  S      scan for networks (join mode)",
    "  A      start the radio at boot",
    "  R      new access-point password",
    "",
    "Access point: the Cardputer makes its own",
    "network. Nothing else is needed -- useful",
    "on a bench with no wifi at all.",
    "",
    "Join: it appears on your own network at",
    "the address shown, and also at",
    "http://cardputer.local",
    "",
    "While the radio is on, G13 and G15 cannot",
    "be read as analog inputs -- ADC2 shares",
    "its hardware with the radio. Everything",
    "else works normally.",
};
const char* const* ToolWiFi::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

static const char* MODE_NAME[] = { "Off", "Access point", "Join a network" };

void ToolWiFi::onEnter() {
    _view = View::Main;
    _sel  = portal.running() ? portal.mode() : settings.wifiMode();
    if (_sel > 2) _sel = 0;
}

void ToolWiFi::startScan() {
    // Scanning needs the station interface up; if the portal is not already
    // running this brings the radio on for the duration.
    if (!portal.running()) {
        WiFi.mode(WIFI_STA);
        boardSetRadioActive(true);
    }
    WiFi.scanDelete();
    WiFi.scanNetworks(true);         // async: tick() collects the result
    _netN   = 0;
    _netSel = 0;
    _netTop = 0;
    _view   = View::Scanning;
}

void ToolWiFi::endScanRadio() {
    if (portal.running()) return;      // the portal owns the radio, leave it
    WiFi.scanDelete();
    WiFi.mode(WIFI_OFF);
    boardSetRadioActive(false);
}

void ToolWiFi::onExit() {
    if (_view == View::Scanning || _view == View::Pick || _view == View::Password)
        endScanRadio();
}

void ToolWiFi::applyMode(uint8_t mode) {
    settings.setWifiMode(mode);
    if (mode == Settings::NET_OFF) {
        portal.stop();
        ui.notify("radio off");
        return;
    }
    if (mode == Settings::NET_STA && strlen(settings.wifiSsid()) == 0) {
        ui.notify("no network saved -- press S");
        return;
    }
    if (portal.start(mode)) ui.notify("%s starting", MODE_NAME[mode]);
    else                    ui.notify("could not start");
}

void ToolWiFi::tick() {
    if (_view == View::Scanning) {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) return;
        if (n < 0) { ui.notify("scan failed"); _view = View::Main; return; }
        _netN = n;
        _view = (n > 0) ? View::Pick : View::Main;
        if (n == 0) ui.notify("no networks found");
    }
}

bool ToolWiFi::onKey(const KeyEvent& ev) {
    switch (_view) {
        case View::Scanning:
            if (ev.key == Key::Esc || ev.key == Key::Back) {
                endScanRadio();
                _view = View::Main;
                return true;
            }
            return true;                       // nothing else while scanning

        case View::Pick:
            if (ev.key == Key::Up)   { if (_netSel > 0) _netSel--; return true; }
            if (ev.key == Key::Down) { if (_netSel < _netN - 1) _netSel++; return true; }
            if (ev.key == Key::Enter) {
                snprintf(_pickSsid, sizeof(_pickSsid), "%s", WiFi.SSID(_netSel).c_str());
                _pass[0] = 0;
                _passLen = 0;
                _view = View::Password;
                return true;
            }
            if (ev.key == Key::Esc || ev.key == Key::Back) {
                endScanRadio();
                _view = View::Main;
                return true;
            }
            return true;

        case View::Password:
            if (ev.key == Key::Enter) {
                settings.setWifiCreds(_pickSsid, _pass);
                WiFi.scanDelete();
                _view = View::Main;
                _sel  = Settings::NET_STA;
                applyMode(Settings::NET_STA);
                return true;
            }
            if (ev.key == Key::Back) {
                if (_passLen > 0) _pass[--_passLen] = 0;
                return true;
            }
            if (ev.key == Key::Esc) { endScanRadio(); _view = View::Main; return true; }
            if (ev.key == Key::Char && ev.ch >= ' ' && ev.ch <= '~' &&
                _passLen < (int)sizeof(_pass) - 1) {
                _pass[_passLen++] = ev.ch;
                _pass[_passLen]   = 0;
            }
            return true;

        default: break;
    }

    switch (ev.key) {
        case Key::Left:  _sel = (uint8_t)((_sel + 2) % 3); return true;
        case Key::Right: _sel = (uint8_t)((_sel + 1) % 3); return true;
        case Key::Enter: applyMode(_sel); return true;
        case Key::Char:
            if (ev.ci('s')) { startScan(); return true; }
            if (ev.ci('a')) {
                settings.setWifiAuto(!settings.wifiAuto());
                ui.notify(settings.wifiAuto() ? "starts at boot" : "manual start");
                return true;
            }
            if (ev.ci('r')) {
                settings.setApPass("");            // regenerated on next start
                ui.notify("new password on next start");
                return true;
            }
            // The way back in after forgetting the web password. Without
            // this the only recovery would be a factory reset, which also
            // throws away every saved pin assignment.
            if (ev.ci('w')) {
                if (settings.webPass()[0]) {
                    settings.setWebPass("");
                    ui.notify("web password cleared");
                } else {
                    ui.notify("no web password set");
                }
                return true;
            }
            return false;
        default:
            return false;
    }
}

void ToolWiFi::drawMain() {
    // Mode selector.
    ui.text(6, BODY_Y + 2, C_DIM, "mode");
    int x = 40;
    for (int i = 0; i < 3; i++) {
        bool sel = (i == _sel);
        bool live = portal.running() ? (portal.mode() == i)
                                     : (i == Settings::NET_OFF);
        int w = ui.chipW(MODE_NAME[i]);
        ui.chip(x, BODY_Y, MODE_NAME[i],
                sel ? C_BLACK : (live ? C_HIGH : C_DIM),
                sel ? C_TITLE : C_PANEL2);
        x += w + 4;
    }

    int y = BODY_Y + 18;

    if (!portal.running()) {
        ui.text(6, y, C_DIM, "radio is off");
        y += 12;
        ui.text(6, y, C_FAINT, "ADC2 (G13/G15) available");
        y += 14;
        ui.text(6, y, C_TEXT, "pick a mode and press ENTER");
    } else if (portal.mode() == Settings::NET_AP) {
        ui.text(6, y, C_DIM, "network");
        ui.text(58, y, C_TITLE, portal.ssid());
        y += 12;
        ui.text(6, y, C_DIM, "password");
        ui.text(58, y, C_TITLE, portal.pass());
        y += 12;
        ui.text(6, y, C_DIM, "open");
        ui.textf(58, y, C_HIGH, "http://%s", portal.ip().c_str());
        y += 12;
        ui.textf(6, y, C_FAINT, "%d client(s) connected", portal.clients());
    } else {
        ui.text(6, y, C_DIM, "network");
        ui.text(58, y, C_TITLE, portal.ssid());
        y += 12;
        ui.text(6, y, C_DIM, "status");
        ui.text(58, y, portal.joined() ? C_HIGH : C_WARN, portal.statusText());
        y += 12;
        if (portal.joined()) {
            ui.text(6, y, C_DIM, "open");
            ui.textf(58, y, C_HIGH, "http://%s", portal.ip().c_str());
            y += 12;
            ui.textf(6, y, C_FAINT, "or http://%s.local   %d dBm",
                     portal.hostname(), portal.rssi());
        }
    }

    if (portal.running())
        ui.textf(6, BODY_B - 9, C_WARN, "ADC2 off: G13/G15 not analog");
    else
        ui.textf(6, BODY_B - 9, C_FAINT, "boot: %s",
                 settings.wifiAuto() ? "auto-start" : "manual");

    // Whether the browser is asked for a password, and the key that clears
    // it -- this is the only way back in after forgetting it.
    if (settings.webPass()[0])
        ui.text(SCR_W - 86, BODY_B - 9, C_WARN,  "web locked [W]");
    else
        ui.text(SCR_W - 86, BODY_B - 9, C_FAINT, "web open");
}

void ToolWiFi::drawPick() {
    ui.textf(6, BODY_Y + 1, C_DIM, "%d networks -- ENTER to pick", _netN);

    const int ROWS = 6;
    if (_netSel < _netTop) _netTop = _netSel;
    if (_netSel >= _netTop + ROWS) _netTop = _netSel - ROWS + 1;

    for (int i = 0; i < ROWS && (i + _netTop) < _netN; i++) {
        int idx = i + _netTop;
        int y   = BODY_Y + 13 + i * 13;
        bool sel = (idx == _netSel);
        if (sel) ui.listRow(y - 1, 12, true);

        ui.text(8, y + 2, sel ? C_TITLE : C_TEXT, WiFi.SSID(idx).c_str());
        int r = WiFi.RSSI(idx);
        ui.textf(SCR_W - 62, y + 2, r > -70 ? C_HIGH : C_DIM, "%d dBm", r);
        if (WiFi.encryptionType(idx) == WIFI_AUTH_OPEN)
            ui.text(SCR_W - 96, y + 2, C_WARN, "open");
    }
    ui.scrollbar(SCR_W - 4, BODY_Y + 13, ROWS * 13, _netTop, ROWS, _netN);
}

void ToolWiFi::drawPassword() {
    int y = ui.modal("Password", 210, 60, C_INFO);
    ui.text(22, y, C_DIM, _pickSsid);

    ui.g().fillRoundRect(20, y + 13, 200, 15, 3, C_PANEL2);
    // Shown in the clear: this is a bench tool, and mistyping a password you
    // cannot see on a 56-key thumb keyboard is worse than the exposure.
    char shown[34];
    int start = _passLen > 32 ? _passLen - 32 : 0;
    snprintf(shown, sizeof(shown), "%s", _pass + start);
    ui.textf(25, y + 17, C_TEXT, "%s%s", shown, ((millis() / 400) & 1) ? "_" : "");

    ui.text(20, y + 33, C_DIM, "[ENT] connect   [Fn+`] cancel");
}

void ToolWiFi::draw() {
    char right[24];
    if (portal.running()) snprintf(right, sizeof(right), "%s", portal.ip().c_str());
    else                  snprintf(right, sizeof(right), "off");
    ui.header("Web Interface", right, catColor(cat()));

    switch (_view) {
        case View::Scanning:
            ui.text(6, BODY_Y + 2, C_DIM, "scanning…");
            ui.hbar(6, BODY_Y + 20, SCR_W - 12, 8,
                    ((millis() / 40) % 100) / 100.0f, C_INFO);
            ui.text(6, BODY_Y + 38, C_FAINT, "this takes a couple of seconds");
            break;
        case View::Pick:     drawPick();     break;
        case View::Password: drawMain(); drawPassword(); break;
        default:             drawMain();     break;
    }

    switch (_view) {
        case View::Pick:     ui.footer("[^v] pick  [ENT] use");            break;
        case View::Password: ui.footer("password + [ENT]   [Fn+`] cancel"); break;
        case View::Scanning: ui.footer("scanning...");                     break;
        default:             ui.footer("[<>]mode [ENT]apply [S]scan");
    }
}

}  // namespace cg

#endif  // CG_ENABLE_WEB
