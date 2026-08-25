#include "ToolSetups.h"
#include "../core/Settings.h"
#include "../ui/Shell.h"
#include <Preferences.h>
#include <string.h>
#include <stdio.h>

namespace cg {

ToolSetups toolSetups;

// One record per tool: the id it was filed under, then its eight pins.
struct Rec {
    char   id[9];
    int8_t pin[Tool::MAX_ROLES];
};

static const char* NS = "cgsetup";

static const char* HELP[] = {
    "A setup is every tool's pin assignment,",
    "saved together under one name.",
    "",
    "  ^ v    choose a slot",
    "  ENTER  load it",
    "  S      save the current pins here",
    "  D      delete",
    "",
    "Useful when you keep more than one rig:",
    "save 'breadboard' with the pins you use",
    "on the EXT header and 'grove' with G1/G2,",
    "and switching rigs is two keystrokes",
    "instead of re-picking pins in every tool.",
    "",
    "Loading writes the pins straight through",
    "to permanent storage, so it survives a",
    "reboot exactly as if you had picked them",
    "by hand.",
};
const char* const* ToolSetups::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

void ToolSetups::keyName(int i, char* out, int n) { snprintf(out, n, "n%d", i); }
void ToolSetups::keyBlob(int i, char* out, int n) { snprintf(out, n, "b%d", i); }

void ToolSetups::refresh() {
    Preferences p;
    if (!p.begin(NS, true)) {
        for (int i = 0; i < SLOTS; i++) _slot[i] = Slot{};
        return;
    }
    for (int i = 0; i < SLOTS; i++) {
        char kn[8], kb[8];
        keyName(i, kn, sizeof(kn));
        keyBlob(i, kb, sizeof(kb));
        size_t blen = p.getBytesLength(kb);
        _slot[i].used  = (blen >= sizeof(Rec));
        _slot[i].tools = (uint8_t)(blen / sizeof(Rec));
        p.getString(kn, _slot[i].name, NAMELEN);
        if (_slot[i].used && !_slot[i].name[0])
            snprintf(_slot[i].name, NAMELEN, "slot %d", i + 1);
    }
    p.end();
}

void ToolSetups::onEnter() {
    _mode = Mode::List;
    refresh();
}

void ToolSetups::save(int i, const char* name) {
    static Rec recs[Shell::MAX_TOOLS];
    int n = 0;
    for (int t = 0; t < shell.toolCount() && n < Shell::MAX_TOOLS; t++) {
        Tool* tool = shell.toolAt(t);
        if (!tool || tool->roleCount() == 0) continue;   // nothing to store
        memset(&recs[n], 0, sizeof(Rec));
        snprintf(recs[n].id, sizeof(recs[n].id), "%s", tool->id());
        for (int r = 0; r < Tool::MAX_ROLES; r++)
            recs[n].pin[r] = (int8_t)tool->pin(r);
        n++;
    }
    if (n == 0) { ui.notify("nothing to save"); return; }

    Preferences p;
    if (!p.begin(NS, false)) { ui.notify("storage unavailable"); return; }
    char kn[8], kb[8];
    keyName(i, kn, sizeof(kn));
    keyBlob(i, kb, sizeof(kb));
    p.putBytes(kb, recs, n * sizeof(Rec));
    p.putString(kn, name && name[0] ? name : "unnamed");
    p.end();

    refresh();
    ui.notify("saved %d tools", n);
}

bool ToolSetups::load(int i) {
    Preferences p;
    if (!p.begin(NS, true)) return false;
    char kb[8];
    keyBlob(i, kb, sizeof(kb));
    size_t len = p.getBytesLength(kb);
    if (len < sizeof(Rec)) { p.end(); return false; }

    static Rec recs[Shell::MAX_TOOLS];
    size_t max = sizeof(recs);
    if (len > max) len = max;
    p.getBytes(kb, recs, len);
    p.end();

    int n = (int)(len / sizeof(Rec));
    int applied = 0;
    for (int r = 0; r < n; r++) {
        recs[r].id[8] = 0;
        for (int t = 0; t < shell.toolCount(); t++) {
            Tool* tool = shell.toolAt(t);
            if (!tool || strcmp(tool->id(), recs[r].id) != 0) continue;
            for (int role = 0; role < tool->roleCount(); role++) {
                int g = recs[r].pin[role];
                // A saved pin that is no longer legal (SD opt-in turned off,
                // say) is dropped rather than silently re-enabled.
                if (g >= 0 && !pinGpioOk(g)) g = -1;
                tool->setPin(role, g);
                settings.setPin(tool->id(), role, g);
            }
            applied++;
            break;
        }
    }
    ui.notify("loaded %d tools", applied);
    return applied > 0;
}

void ToolSetups::erase(int i) {
    Preferences p;
    if (!p.begin(NS, false)) return;
    char kn[8], kb[8];
    keyName(i, kn, sizeof(kn));
    keyBlob(i, kb, sizeof(kb));
    p.remove(kb);
    p.remove(kn);
    p.end();
    refresh();
}

bool ToolSetups::onKey(const KeyEvent& ev) {
    if (_mode == Mode::Naming) {
        switch (ev.key) {
            case Key::Enter:
                save(_cursor, _edit);
                _mode = Mode::List;
                return true;
            case Key::Esc:
                _mode = Mode::List;
                return true;
            case Key::Back:
                if (_editLen > 0) _edit[--_editLen] = 0;
                return true;
            case Key::Char:
                if (ev.ch >= ' ' && ev.ch <= '~' && _editLen < NAMELEN - 1) {
                    _edit[_editLen++] = ev.ch;
                    _edit[_editLen]   = 0;
                }
                return true;
            default:
                return true;      // swallow everything while editing
        }
    }

    if (_mode == Mode::ConfirmDelete) {
        if (ev.ci('y')) { erase(_cursor); _mode = Mode::List; return true; }
        _mode = Mode::List;
        return true;
    }

    switch (ev.key) {
        case Key::Up:   _cursor = (_cursor + SLOTS - 1) % SLOTS; return true;
        case Key::Down: _cursor = (_cursor + 1) % SLOTS;         return true;
        case Key::Enter:
            if (!_slot[_cursor].used) { ui.notify("slot is empty"); return true; }
            load(_cursor);
            return true;
        case Key::Char:
            if (ev.ci('s')) {
                _mode = Mode::Naming;
                snprintf(_edit, NAMELEN, "%s", _slot[_cursor].name);
                _editLen = (int)strlen(_edit);
                return true;
            }
            if (ev.ci('d')) {
                if (_slot[_cursor].used) _mode = Mode::ConfirmDelete;
                return true;
            }
            return false;
        default:
            return false;
    }
}

void ToolSetups::draw() {
    ui.header("Saved Setups", nullptr, catColor(cat()));

    for (int i = 0; i < SLOTS; i++) {
        int  y   = BODY_Y + 2 + i * 16;
        bool sel = (i == _cursor);
        if (sel) ui.listRow(y - 1, 15, true);

        ui.textf(8, y + 3, sel ? C_TITLE : C_DIM, "%d", i + 1);
        if (_slot[i].used) {
            ui.text(22, y + 3, sel ? C_TEXT : C_DIM, _slot[i].name);
            ui.textf(SCR_W - 62, y + 3, C_FAINT, "%d tools", _slot[i].tools);
        } else {
            ui.text(22, y + 3, C_FAINT, "-- empty --");
        }
    }

    if (_mode == Mode::Naming) {
        int y = ui.modal("Name this setup", 190, 54, C_INFO);
        ui.g().fillRoundRect(30, y + 4, 180, 15, 3, C_PANEL2);
        ui.textf(35, y + 8, C_TEXT, "%s%s", _edit,
                 ((millis() / 400) & 1) ? "_" : "");
        ui.text(30, y + 24, C_DIM, "[ENT] save   [Fn+`] cancel");
    } else if (_mode == Mode::ConfirmDelete) {
        int y = ui.modal("Delete setup?", 170, 44, C_WARN);
        ui.text(40, y + 4,  C_TEXT, _slot[_cursor].name);
        ui.text(40, y + 18, C_DIM,  "[Y] delete   any key: keep");
    }

    ui.footer("[ENT]load [S]save [D]delete");
}

}  // namespace cg
