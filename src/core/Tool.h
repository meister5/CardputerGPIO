/**
 * Tool.h — the contract every screen in the toolbox implements.
 *
 * The shell owns the loop. A tool never calls M5Cardputer.update(), never
 * touches the keyboard, and never pushes the sprite; it reacts to KeyEvents
 * and paints into ui. That is what makes the frame rate uniform and the
 * navigation identical everywhere.
 */

#pragma once
#include <stddef.h>
#include "Keys.h"
#include "Pins.h"
#include "Board.h"
#include "UI.h"

namespace cg {

enum class Cat : uint8_t { Digital, Analog, Signal, Bus, System, COUNT };

const char* catName(Cat c);
uint16_t    catColor(Cat c);

enum class RoleDir : uint8_t { Out, In, Adc, Pwm, Bus };

struct Role {
    const char* label;   // short, fits the wiring table
    RoleDir     dir;
    const char* hint;    // what to physically connect it to
    int8_t      pref;    // preferred default GPIO, -1 = first free from pool
};

uint16_t    roleColor(RoleDir d);
const char* roleDirName(RoleDir d);

class Tool {
public:
    static constexpr int MAX_ROLES = 8;

    virtual ~Tool() = default;

    // Persistence key. Max 8 chars, ASCII, and never changed after release --
    // it is what saved pin assignments are filed under.
    virtual const char* id()    const = 0;
    virtual const char* name()  const = 0;
    virtual const char* blurb() const = 0;
    virtual Cat         cat()   const = 0;

    virtual const Role* roles()     const { return nullptr; }
    virtual int         roleCount() const { return 0; }

    // Resolved by the shell from Settings before onEnter().
    int  pin(int role) const {
        return (role >= 0 && role < MAX_ROLES) ? _pins[role] : -1;
    }
    void setPin(int role, int gpio) {
        if (role >= 0 && role < MAX_ROLES) _pins[role] = (int8_t)gpio;
    }

    virtual void onEnter() {}
    virtual void onExit()  {}
    virtual void tick()    {}
    virtual void draw()    = 0;

    // Return true when the key was consumed. Back/Esc are handled by the
    // shell unless a tool claims them (a text field does, for instance).
    virtual bool onKey(const KeyEvent&) { return false; }

    virtual const char* hints() const { return "[DEL] back"; }

    // True while the tool has a text field open. The shell hands ; . , / and
    // ` back to the character layer for as long as it is -- see Keys.h. A
    // tool answers from its own state rather than switching the mode itself,
    // so there is no way to leave the cursor keys dead on the way out.
    virtual bool textEntry() const { return false; }

    // Optional per-tool help page, shown on F1.
    virtual const char* const* help(int& n) const { n = 0; return nullptr; }

    // Tools that produce numbers implement these two and get CSV logging for
    // free: the shell owns the keystroke, the sink and the sample interval,
    // so logging works identically everywhere. logHeader() returns the column
    // names after the leading "ms"; logRow() fills one comma-separated line.
    virtual const char* logHeader() const { return nullptr; }
    virtual bool logRow(char* out, size_t n) { (void)out; (void)n; return false; }
    bool canLog() const { return logHeader() != nullptr; }

    // Tools that drive pins get an arm confirmation before they run, so a
    // stale saved pin set cannot start sourcing current unannounced.
    virtual bool drivesOutputs() const { return false; }

protected:
    int8_t _pins[MAX_ROLES] = { -1, -1, -1, -1, -1, -1, -1, -1 };
};

}  // namespace cg
