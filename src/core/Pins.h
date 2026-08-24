/**
 * Pins.h — GPIO abstraction with a hard safety gate.
 *
 * Every entry point runs the requested pin past Board's map first. A pin that
 * is not on a header, or that belongs to the system I2C bus, is refused. That
 * matters more here than on a typical board: driving G8/G9 takes the keyboard
 * down with it and the only way back is a reflash.
 *
 * Three things this fixes versus the v1 PinManager:
 *
 *   ADC   v1 did (raw / 4095) * 3.3, which ignores the per-chip calibration
 *         burned into eFuse and the non-linearity of the 11 dB attenuator.
 *         analogReadMilliVolts() uses the calibration curve and is accurate
 *         to a few mV instead of a few hundred.
 *
 *   PWM   v1 hard-coded 10-bit resolution, which caps LEDC at 80 MHz / 1024
 *         = 78 kHz -- so the advertised "up to 1 MHz" silently did nothing.
 *         Resolution is now derived from the requested frequency.
 *
 *   LEDC  v1 allocated a channel counter it never used and then wrote
 *         ledcChan = 0 for every pin. Core 3.x addresses LEDC by pin, so the
 *         whole notion is gone.
 */

#pragma once
#include <Arduino.h>
#include <stdint.h>

namespace cg {

enum class PMode : uint8_t {
    None = 0,
    In,        // floating
    InPull,    // pull-up
    InPulld,   // pull-down
    Out,
    Adc,
    Pwm,
    Bus,       // claimed by UART/SPI/1-Wire/RMT — hands off
};

class Pins {
public:
    void begin();

    // ── Configuration ─────────────────────────────────────────────────────
    bool setInput (int gpio, uint8_t pull = 1);   // 0 none, 1 up, 2 down
    bool setOutput(int gpio, bool initial = false);
    bool setAdc   (int gpio);
    bool claimBus (int gpio);                     // for UART/SPI/RMT owners

    // Returns the frequency LEDC actually produced, or 0 on failure.
    uint32_t setPwm(int gpio, uint32_t freqHz, float dutyPct);

    void release(int gpio);
    void releaseAll();

    // ── Digital ───────────────────────────────────────────────────────────
    void write (int gpio, bool high);
    bool read  (int gpio);
    void toggle(int gpio);
    bool level (int gpio) const;   // last known level, no bus access

    // ── Analog ────────────────────────────────────────────────────────────
    uint16_t adcRaw(int gpio);            // 0..4095
    uint32_t adcMilliVolts(int gpio);     // eFuse-calibrated
    // Oversampled read; n is clamped to 1..64.
    uint32_t adcMilliVoltsAvg(int gpio, uint8_t n);

    // ── PWM ───────────────────────────────────────────────────────────────
    uint32_t pwmSetFreq(int gpio, uint32_t hz);   // returns actual, 0 on fail
    bool     pwmSetDuty(int gpio, float pct);
    uint32_t pwmFreq(int gpio) const;
    float    pwmDuty(int gpio) const;
    uint8_t  pwmBits(int gpio) const;

    // Highest LEDC resolution usable at this frequency, 0 if unreachable.
    static uint8_t pwmBestBits(uint32_t hz);
    static constexpr uint32_t PWM_MAX_HZ = 20000000UL;
    static constexpr uint32_t PWM_MIN_HZ = 5UL;   // below this, use software

    PMode mode(int gpio) const;
    const char* modeName(int gpio) const;

private:
    static constexpr int NPIN = 48;
    struct St {
        PMode    mode  = PMode::None;
        bool     level = false;
        uint32_t freq  = 1000;
        float    duty  = 50.0f;
        uint8_t  bits  = 10;
    };
    St _s[NPIN];

    bool guard(int gpio) const;
};

extern Pins pins;

// ── Hardware pulse counter (PCNT) ─────────────────────────────────────────
// Counts edges in hardware, so it keeps up far past what an ISR could.
// Interrupt-driven counting tops out around 100 kHz on this chip; PCNT is
// good to tens of MHz. Overflows are accumulated by a watch-point callback,
// so the total is 64-bit and does not wrap at 32767.
class PulseCounter {
public:
    bool begin(int gpio, uint16_t glitchNs = 1000);
    void end();
    bool active() const { return _unit != nullptr; }

    void     clear();
    int64_t  count();

    // Gated measurement: counts for gateMs and returns Hz. Non-blocking --
    // call repeatedly; ready() goes true when a window completes.
    void  startGate(uint32_t gateMs);
    bool  ready();
    float hz() const { return _hz; }
    uint32_t gateMs() const { return _gateMs; }

private:
    void*    _unit    = nullptr;   // pcnt_unit_handle_t
    void*    _chan    = nullptr;   // pcnt_channel_handle_t
    int      _gpio    = -1;
    uint32_t _gateMs  = 1000;
    uint32_t _gateEnd = 0;
    bool     _gating  = false;
    float    _hz      = 0.0f;
};

}  // namespace cg
