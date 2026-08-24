/**
 * ToolBoard.h — what is actually on this board, and where.
 *
 * Every pin decision in the firmware comes out of one table; this screen
 * shows it, plus the internal peripherals that own the pins you cannot have.
 * It is the page to open when a tool refuses a pin and you want to know why.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolBoard : public Tool {
public:
    const char* id()    const override { return "board"; }
    const char* name()  const override { return "Board Info"; }
    const char* blurb() const override { return "pinout, memory, internals"; }
    Cat         cat()   const override { return Cat::System; }

    void onEnter() override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;

private:
    enum class Page : uint8_t { Summary, Header, Internal, COUNT };

    Page _page   = Page::Summary;
    int  _scroll = 0;

    void drawSummary();
    void drawHeader();
    void drawInternal();
    int  rowsOnPage() const;
};

extern ToolBoard toolBoard;

}  // namespace cg
