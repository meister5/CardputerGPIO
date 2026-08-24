/**
 * ToolUART.h — serial terminal and USB bridge on the EXT header.
 *
 * The EXT header brings out G13 and G15, silkscreened UART_TX and UART_RX.
 * Those labels describe the peripheral you are likely to plug in, not the
 * ESP32's own direction, so both are configurable -- swapping TX and RX is
 * the first thing to try when a link is silent, and it is one keypress here.
 *
 * BRIDGE mode pipes the port straight to USB serial, turning the Cardputer
 * into a USB-to-TTL adapter with a screen.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolUART : public Tool {
public:
    const char* id()    const override { return "uart"; }
    const char* name()  const override { return "UART Terminal"; }
    const char* blurb() const override { return "terminal, hex view, USB bridge"; }
    Cat         cat()   const override { return Cat::Bus; }

    const Role* roles()     const override;
    int         roleCount() const override { return 2; }

    void onEnter() override;
    void onExit()  override;
    void tick()    override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;

private:
    static constexpr int COLS = 39;
    static constexpr int ROWS = 7;

    char     _scr[ROWS][COLS + 1] = {};
    int      _row = 0, _col = 0;
    uint8_t  _baudIdx = 5;
    bool     _hex     = false;
    bool     _bridge  = false;
    bool     _swap    = false;
    bool     _crlf    = true;
    bool     _open    = false;

    char     _input[40] = {};
    int      _inLen     = 0;
    uint32_t _rxCount = 0, _txCount = 0;

    static uint32_t baud(uint8_t i);
    void openPort();
    void closePort();
    void putChar(char c);
    void putByte(uint8_t b);
    void newline();
    void sendLine();
    void clearScreen();
};

extern ToolUART toolUART;

}  // namespace cg
