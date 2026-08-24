/**
 * ToolSettings.h — preferences, and the two safety switches.
 *
 * The switches matter more than the cosmetics: "arm outputs" is what stops a
 * pin set saved weeks ago from driving current the instant a tool opens, and
 * "SD pins" is what keeps G14/G39/G40 out of the pin pools while a card is
 * in the slot.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolSettings : public Tool {
public:
    const char* id()    const override { return "config"; }
    const char* name()  const override { return "Settings"; }
    const char* blurb() const override { return "screen, sound, safety"; }
    Cat         cat()   const override { return Cat::System; }

    void onEnter() override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;

private:
    enum class Row : uint8_t { Bright, Beep, Arm, SdPins, LogRate, Reset, COUNT };

    Row  _row     = Row::Bright;
    bool _confirm = false;

    void adjust(int delta);
};

extern ToolSettings toolSettings;

}  // namespace cg
