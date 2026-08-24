#include "ToolSettings.h"
#include "../core/Settings.h"
#include "../core/Logger.h"
#include <M5Cardputer.h>
#include <stdio.h>

namespace cg {

ToolSettings toolSettings;

static const char* HELP[] = {
    "  ^ v    choose        < >    change",
    "  ENTER  toggle / act",
    "",
    "Arm outputs: every tool that can drive a",
    "pin asks first. Leave it on unless you",
    "are certain what is wired.",
    "",
    "Allow SD pins: G14, G39 and G40 are on",
    "the EXT header AND on the microSD bus.",
    "With this off they are kept out of the",
    "pin pools, so a tool can never fight the",
    "card reader. Turn it on only when there",
    "is no card in the slot -- and note that",
    "CSV logging to card will then be refused",
    "while such a pin is claimed.",
    "",
    "Factory reset clears preferences and all",
    "saved pin assignments. Saved setups are",
    "kept.",
};
const char* const* ToolSettings::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

void ToolSettings::onEnter() { _confirm = false; }

void ToolSettings::adjust(int delta) {
    switch (_row) {
        case Row::Bright: {
            int v = settings.brightness() + delta * 15;
            settings.setBrightness((uint8_t)(v < 10 ? 10 : (v > 255 ? 255 : v)));
            break;
        }
        case Row::Beep:
            settings.setBeep(!settings.beep());
            break;
        case Row::Arm:
            settings.setArmOutputs(!settings.armOutputs());
            break;
        case Row::SdPins:
            settings.setAllowSdPins(!settings.allowSdPins());
            ui.notify(settings.allowSdPins() ? "G14/G39/G40 unlocked"
                                             : "G14/G39/G40 reserved");
            break;
        case Row::LogRate: {
            static const uint32_t STEP[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000 };
            const int N = (int)(sizeof(STEP) / sizeof(STEP[0]));
            int idx = 0;
            for (int i = 0; i < N; i++) if (logger.interval() >= STEP[i]) idx = i;
            idx += delta;
            if (idx < 0) idx = 0;
            if (idx >= N) idx = N - 1;
            logger.setInterval(STEP[idx]);
            break;
        }
        default:
            break;
    }
}

bool ToolSettings::onKey(const KeyEvent& ev) {
    if (_confirm) {
        if (ev.ci('y')) {
            settings.factoryReset();
            poolsRebuild();
            ui.notify("reset -- reboot to re-read");
        }
        _confirm = false;
        return true;
    }

    switch (ev.key) {
        case Key::Up:
            _row = (Row)(((int)_row + (int)Row::COUNT - 1) % (int)Row::COUNT);
            return true;
        case Key::Down:
            _row = (Row)(((int)_row + 1) % (int)Row::COUNT);
            return true;
        case Key::Left:  adjust(-1); return true;
        case Key::Right: adjust(+1); return true;
        case Key::Enter:
            if (_row == Row::Reset) _confirm = true;
            else adjust(+1);
            return true;
        default:
            return false;
    }
}

void ToolSettings::draw() {
    ui.header("Settings", boardName(), catColor(cat()));

    struct RowInfo { const char* label; char val[16]; uint16_t col; };
    RowInfo r[(int)Row::COUNT];

    r[0].label = "brightness";
    snprintf(r[0].val, sizeof(r[0].val), "%d", settings.brightness());
    r[0].col = C_TEXT;

    r[1].label = "key beep";
    snprintf(r[1].val, sizeof(r[1].val), "%s", settings.beep() ? "on" : "off");
    r[1].col = settings.beep() ? C_HIGH : C_DIM;

    r[2].label = "arm outputs";
    snprintf(r[2].val, sizeof(r[2].val), "%s", settings.armOutputs() ? "on" : "OFF");
    r[2].col = settings.armOutputs() ? C_HIGH : C_WARN;

    r[3].label = "allow SD pins";
    snprintf(r[3].val, sizeof(r[3].val), "%s", settings.allowSdPins() ? "YES" : "no");
    r[3].col = settings.allowSdPins() ? C_WARN : C_HIGH;

    r[4].label = "log interval";
    snprintf(r[4].val, sizeof(r[4].val), "%lu ms", (unsigned long)logger.interval());
    r[4].col = C_TEXT;

    r[5].label = "factory reset";
    snprintf(r[5].val, sizeof(r[5].val), "%s", "[ENTER]");
    r[5].col = C_LOW;

    for (int i = 0; i < (int)Row::COUNT; i++) {
        int  y   = BODY_Y + 1 + i * 16;
        bool sel = (i == (int)_row);
        if (sel) ui.listRow(y - 1, 15, true);
        ui.text(8, y + 3, sel ? C_TITLE : C_DIM, r[i].label);
        ui.text(126, y + 3, r[i].col, r[i].val);
    }

    // Brightness gets a bar, because a number alone tells you nothing.
    if (_row == Row::Bright)
        ui.hbar(170, BODY_Y + 4, 60, 7, settings.brightness() / 255.0f, C_INFO);

    if (_confirm) {
        int y = ui.modal("Factory reset?", 190, 46, C_LOW);
        ui.text(30, y + 4,  C_TEXT, "Clears prefs and saved pins.");
        ui.text(30, y + 18, C_DIM,  "[Y] confirm   any key: cancel");
    }

    ui.footer("[^v] item  [<>] change  [ENT] toggle");
}

}  // namespace cg
