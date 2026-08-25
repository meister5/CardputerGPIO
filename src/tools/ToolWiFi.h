/**
 * ToolWiFi.h — turn the web interface on, and find out where it is.
 *
 * Two modes. Access point needs nothing else in the room: the Cardputer makes
 * its own network, shows you the name and password, and serves the interface
 * at its own address. Join mode puts it on a network you already have, where
 * it also answers to http://cardputer.local.
 *
 * The usual first run is: start the access point, connect a laptop to it,
 * open the page, and set the home network from there -- typing a WPA2
 * password on a 56-key thumb keyboard is not something to inflict on anyone.
 * Scanning and typing it here works too, for when there is no other machine.
 */

#pragma once
#include "../core/Config.h"

#if CG_ENABLE_WEB

#include "../core/Tool.h"

namespace cg {

class ToolWiFi : public Tool {
public:
    const char* id()    const override { return "wifi"; }
    const char* name()  const override { return "Web Interface"; }
    const char* blurb() const override { return "browser control over WiFi"; }
    Cat         cat()   const override { return Cat::System; }

    bool textEntry() const override { return _view == View::Password; }

    void onEnter() override;
    void onExit()  override;
    void tick()    override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;

private:
    enum class View : uint8_t { Main, Scanning, Pick, Password };

    View     _view    = View::Main;
    uint8_t  _sel     = 0;         // mode selection on the main view
    int      _netN    = 0;
    int      _netSel  = 0;
    int      _netTop  = 0;
    char     _pass[65] = {};
    int      _passLen  = 0;
    char     _pickSsid[33] = {};
    uint32_t _lastPoll = 0;

    void drawMain();
    void drawPick();
    void drawPassword();
    void startScan();
    // Scanning needs the radio; if we turned it on just for that, put it
    // back when the scan is abandoned. Leaving it up would keep ADC2 dead
    // with nothing to show for it.
    void endScanRadio();
    void applyMode(uint8_t mode);
};

extern ToolWiFi toolWiFi;

}  // namespace cg

#endif  // CG_ENABLE_WEB
