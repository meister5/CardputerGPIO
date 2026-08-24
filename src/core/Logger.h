/**
 * Logger.h — CSV capture from any measuring tool.
 *
 * Two sinks, either or both:
 *
 *   Serial   always available, costs nothing, and works while the sketch is
 *            plugged into the machine you are debugging from.
 *   microSD  survives being unplugged, but the card reader shares G14/G39/G40
 *            with the EXT header. If the running tool has claimed one of
 *            those pins, mounting the card would fight it, so we refuse and
 *            say why rather than corrupting both.
 *
 * A tool opts in by implementing Tool::logHeader() and Tool::logRow(); the
 * shell does the rest, so every tool that can produce numbers logs them the
 * same way, with the same keystroke.
 */

#pragma once
#include <stdint.h>
#include <stddef.h>

namespace cg {

enum class LogSink : uint8_t { None = 0, Serial_ = 1, Sd = 2, Both = 3 };

class Logger {
public:
    // Opens a sink and writes the CSV header. Returns false and leaves a
    // reason in lastError() if it could not.
    bool start(const char* toolId, const char* header, LogSink sink);
    void stop();

    // Rate-limited by intervalMs; call it every frame and it does the right
    // thing. Returns true when a row was actually written.
    bool row(const char* csv);

    bool        active()   const { return _sink != LogSink::None; }
    LogSink     sink()     const { return _sink; }
    uint32_t    rows()     const { return _rows; }
    uint32_t    seconds()  const;
    const char* fileName() const { return _file; }
    const char* lastError()const { return _err; }

    uint32_t interval() const { return _intervalMs; }
    void setInterval(uint32_t ms) { _intervalMs = ms < 20 ? 20 : ms; }

    // True when the microSD lines are free to be claimed right now.
    static bool sdPinsFree();

    // Mount the card just to look at it -- listing and downloading past
    // captures from the web interface. A capture already holds the card
    // mounted, in which case these are no-ops and it stays mounted.
    bool browseBegin();
    void browseEnd();

private:
    LogSink  _sink       = LogSink::None;
    uint32_t _rows       = 0;
    uint32_t _startMs    = 0;
    uint32_t _lastMs     = 0;
    uint32_t _intervalMs = 200;
    char     _file[32]   = {};
    char     _err[40]    = {};
    void*    _fh         = nullptr;   // File*, heap-allocated while open
    bool     _sdMounted  = false;
    bool     _browsing   = false;

    bool mountSd();
    void unmountSd();
    void write(const char* line);
};

extern Logger logger;

}  // namespace cg
