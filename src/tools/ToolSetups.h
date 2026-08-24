/**
 * ToolSetups.h — save and restore the whole pin map under a name.
 *
 * Re-assigning pins for every tool each time you move to a different rig is
 * the tedious part of using something like this. A setup is a snapshot of
 * every tool's pin assignments; loading one puts the whole toolbox back the
 * way it was for that board.
 *
 * Snapshots are keyed by tool id, so adding tools in a later firmware does
 * not invalidate an old setup -- unknown ids in the blob are skipped and
 * tools missing from it keep whatever they have.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolSetups : public Tool {
public:
    static constexpr int SLOTS   = 6;
    static constexpr int NAMELEN = 13;

    const char* id()    const override { return "setups"; }
    const char* name()  const override { return "Saved Setups"; }
    const char* blurb() const override { return "store/recall pin maps"; }
    Cat         cat()   const override { return Cat::System; }

    void onEnter() override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;

    // ── Shared with the web interface ─────────────────────────────────────
    // The same three actions the on-device screen performs, so a setup saved
    // from a browser is byte-identical to one saved from the keyboard.
    void refresh();
    void save(int slot, const char* name);
    bool load(int slot);
    void erase(int slot);

    bool        slotUsed(int i)  const { return valid(i) && _slot[i].used; }
    const char* slotName(int i)  const { return valid(i) ? _slot[i].name : ""; }
    int         slotTools(int i) const { return valid(i) ? _slot[i].tools : 0; }

private:
    enum class Mode : uint8_t { List, Naming, ConfirmDelete };

    struct Slot {
        char    name[NAMELEN] = {};
        uint8_t tools = 0;
        bool    used  = false;
    };

    static bool valid(int i) { return i >= 0 && i < SLOTS; }

    Slot _slot[SLOTS];
    int  _cursor = 0;
    Mode _mode   = Mode::List;
    char _edit[NAMELEN] = {};
    int  _editLen = 0;

    static void keyName(int i, char* out, int n);
    static void keyBlob(int i, char* out, int n);
};

extern ToolSetups toolSetups;

}  // namespace cg
