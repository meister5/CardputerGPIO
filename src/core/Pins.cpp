#include "Pins.h"
#include "Board.h"
#include "driver/pulse_cnt.h"
#include "esp_err.h"

namespace cg {

Pins pins;

void Pins::begin() {
    for (int i = 0; i < NPIN; i++) _s[i] = St{};
}

bool Pins::guard(int gpio) const {
    if (gpio < 0 || gpio >= NPIN) return false;
    return pinGpioOk(gpio);
}

PMode Pins::mode(int gpio) const {
    if (gpio < 0 || gpio >= NPIN) return PMode::None;
    return _s[gpio].mode;
}

const char* Pins::modeName(int gpio) const {
    switch (mode(gpio)) {
        case PMode::In:      return "IN";
        case PMode::InPull:  return "IN-PU";
        case PMode::InPulld: return "IN-PD";
        case PMode::Out:     return "OUT";
        case PMode::Adc:     return "ADC";
        case PMode::Pwm:     return "PWM";
        case PMode::Bus:     return "BUS";
        default:             return "-";
    }
}

// ── Configuration ─────────────────────────────────────────────────────────
bool Pins::setInput(int gpio, uint8_t pull) {
    if (!guard(gpio)) return false;
    if (_s[gpio].mode == PMode::Pwm) ledcDetach(gpio);
    pinMode(gpio, pull == 1 ? INPUT_PULLUP : (pull == 2 ? INPUT_PULLDOWN : INPUT));
    _s[gpio].mode = (pull == 1) ? PMode::InPull
                  : (pull == 2) ? PMode::InPulld
                                : PMode::In;
    return true;
}

bool Pins::setOutput(int gpio, bool initial) {
    if (!guard(gpio)) return false;
    if (_s[gpio].mode == PMode::Pwm) ledcDetach(gpio);
    pinMode(gpio, OUTPUT);
    digitalWrite(gpio, initial ? HIGH : LOW);
    _s[gpio].mode  = PMode::Out;
    _s[gpio].level = initial;
    return true;
}

bool Pins::setAdc(int gpio) {
    if (!guard(gpio) || !pinAdcOk(gpio)) return false;
    if (_s[gpio].mode == PMode::Pwm) ledcDetach(gpio);
    pinMode(gpio, INPUT);
    // 12 dB (formerly named 11 dB) gives the full ~0-3.1 V span. The
    // calibration curve behind analogReadMilliVolts() is per-attenuation.
    analogSetPinAttenuation(gpio, ADC_11db);
    _s[gpio].mode = PMode::Adc;
    return true;
}

bool Pins::claimBus(int gpio) {
    if (!guard(gpio)) return false;
    if (_s[gpio].mode == PMode::Pwm) ledcDetach(gpio);
    _s[gpio].mode = PMode::Bus;
    return true;
}

void Pins::release(int gpio) {
    if (gpio < 0 || gpio >= NPIN) return;
    if (_s[gpio].mode == PMode::Pwm) ledcDetach(gpio);
    if (_s[gpio].mode != PMode::None && _s[gpio].mode != PMode::Bus)
        pinMode(gpio, INPUT);
    _s[gpio] = St{};
}

void Pins::releaseAll() {
    for (int i = 0; i < NPIN; i++)
        if (_s[i].mode != PMode::None) release(i);
}

// ── Digital ───────────────────────────────────────────────────────────────
void Pins::write(int gpio, bool high) {
    if (!guard(gpio)) return;
    digitalWrite(gpio, high ? HIGH : LOW);
    _s[gpio].level = high;
}

bool Pins::read(int gpio) {
    if (!guard(gpio)) return false;
    bool v = digitalRead(gpio) == HIGH;
    _s[gpio].level = v;
    return v;
}

void Pins::toggle(int gpio) {
    if (!guard(gpio)) return;
    write(gpio, !_s[gpio].level);
}

bool Pins::level(int gpio) const {
    if (gpio < 0 || gpio >= NPIN) return false;
    return _s[gpio].level;
}

// ── Analog ────────────────────────────────────────────────────────────────
uint16_t Pins::adcRaw(int gpio) {
    if (!pinAdcOk(gpio)) return 0;
    return (uint16_t)analogRead(gpio);
}

uint32_t Pins::adcMilliVolts(int gpio) {
    if (!pinAdcOk(gpio)) return 0;
    return analogReadMilliVolts(gpio);
}

uint32_t Pins::adcMilliVoltsAvg(int gpio, uint8_t n) {
    if (!pinAdcOk(gpio)) return 0;
    if (n < 1)  n = 1;
    if (n > 64) n = 64;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < n; i++) sum += analogReadMilliVolts(gpio);
    return sum / n;
}

// ── PWM ───────────────────────────────────────────────────────────────────
// LEDC divides an 80 MHz source by (2^bits) to reach the target frequency, so
// the usable resolution falls as frequency rises. Asking for more bits than
// the frequency allows makes ledcAttach fail outright, which is exactly the
// trap the v1 firmware fell into with its fixed 10 bits.
uint8_t Pins::pwmBestBits(uint32_t hz) {
    if (hz == 0) return 0;
    const uint32_t SRC = 80000000UL;
    for (int bits = 14; bits >= 1; bits--) {
        uint64_t need = (uint64_t)hz << bits;
        if (need <= SRC) return (uint8_t)bits;
    }
    return 0;
}

uint32_t Pins::setPwm(int gpio, uint32_t freqHz, float dutyPct) {
    if (!guard(gpio)) return 0;
    if (freqHz > PWM_MAX_HZ) freqHz = PWM_MAX_HZ;
    if (freqHz == 0) return 0;

    if (_s[gpio].mode == PMode::Pwm) ledcDetach(gpio);

    // Walk resolution down until the peripheral accepts the pairing.
    for (int bits = pwmBestBits(freqHz); bits >= 1; bits--) {
        if (!ledcAttach(gpio, freqHz, (uint8_t)bits)) continue;

        _s[gpio].mode = PMode::Pwm;
        _s[gpio].bits = (uint8_t)bits;
        _s[gpio].freq = freqHz;
        pwmSetDuty(gpio, dutyPct);

        // ledcChangeFrequency reports what the divider could actually hit.
        uint32_t actual = ledcChangeFrequency(gpio, freqHz, (uint8_t)bits);
        if (actual) _s[gpio].freq = actual;
        pwmSetDuty(gpio, dutyPct);
        return _s[gpio].freq;
    }
    return 0;
}

uint32_t Pins::pwmSetFreq(int gpio, uint32_t hz) {
    if (!guard(gpio) || _s[gpio].mode != PMode::Pwm) return 0;
    if (hz > PWM_MAX_HZ) hz = PWM_MAX_HZ;
    if (hz == 0) return 0;

    uint8_t bits = pwmBestBits(hz);
    if (bits == 0) return 0;

    // Changing bit width needs a re-attach; changing only frequency does not.
    if (bits != _s[gpio].bits) return setPwm(gpio, hz, _s[gpio].duty);

    uint32_t actual = ledcChangeFrequency(gpio, hz, bits);
    if (!actual) return 0;
    _s[gpio].freq = actual;
    pwmSetDuty(gpio, _s[gpio].duty);
    return actual;
}

bool Pins::pwmSetDuty(int gpio, float pct) {
    if (!guard(gpio) || _s[gpio].mode != PMode::Pwm) return false;
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    _s[gpio].duty = pct;

    uint32_t maxDuty = (1UL << _s[gpio].bits) - 1;
    uint32_t raw     = (uint32_t)((pct / 100.0f) * maxDuty + 0.5f);
    return ledcWrite(gpio, raw);
}

uint32_t Pins::pwmFreq(int gpio) const {
    return (gpio >= 0 && gpio < NPIN) ? _s[gpio].freq : 0;
}
float Pins::pwmDuty(int gpio) const {
    return (gpio >= 0 && gpio < NPIN) ? _s[gpio].duty : 0;
}
uint8_t Pins::pwmBits(int gpio) const {
    return (gpio >= 0 && gpio < NPIN) ? _s[gpio].bits : 0;
}

// ── PulseCounter ──────────────────────────────────────────────────────────
// PCNT's counter is 16-bit. A watch point at the high limit fires a callback
// on every wrap, and we accumulate there so the running total is 64-bit.
static constexpr int PCNT_HIGH =  30000;
static constexpr int PCNT_LOW  = -30000;

struct PcntAccum {
    volatile int64_t wraps = 0;
};
static PcntAccum s_accum[4];
static int       s_accumNext = 0;

static bool IRAM_ATTR pcntOnReach(pcnt_unit_handle_t,
                                  const pcnt_watch_event_data_t* edata,
                                  void* user_ctx) {
    PcntAccum* a = (PcntAccum*)user_ctx;
    if (edata->watch_point_value == PCNT_HIGH)      a->wraps += PCNT_HIGH;
    else if (edata->watch_point_value == PCNT_LOW)  a->wraps += PCNT_LOW;
    return false;
}

bool PulseCounter::begin(int gpio, uint16_t glitchNs) {
    end();
    if (!pinGpioOk(gpio)) return false;

    pcnt_unit_config_t ucfg = {};
    ucfg.high_limit = PCNT_HIGH;
    ucfg.low_limit  = PCNT_LOW;
    pcnt_unit_handle_t unit = nullptr;
    if (pcnt_new_unit(&ucfg, &unit) != ESP_OK) return false;

    if (glitchNs) {
        pcnt_glitch_filter_config_t fcfg = {};
        fcfg.max_glitch_ns = glitchNs;
        pcnt_unit_set_glitch_filter(unit, &fcfg);
    }

    pcnt_chan_config_t ccfg = {};
    ccfg.edge_gpio_num  = gpio;
    ccfg.level_gpio_num = -1;
    pcnt_channel_handle_t chan = nullptr;
    if (pcnt_new_channel(unit, &ccfg, &chan) != ESP_OK) {
        pcnt_del_unit(unit);
        return false;
    }

    // Count rising edges only; ignore level input entirely.
    pcnt_channel_set_edge_action(chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                       PCNT_CHANNEL_EDGE_ACTION_HOLD);
    pcnt_channel_set_level_action(chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                        PCNT_CHANNEL_LEVEL_ACTION_KEEP);

    PcntAccum* acc = &s_accum[s_accumNext % 4];
    s_accumNext++;
    acc->wraps = 0;

    pcnt_unit_add_watch_point(unit, PCNT_HIGH);
    pcnt_unit_add_watch_point(unit, PCNT_LOW);
    pcnt_event_callbacks_t cbs = {};
    cbs.on_reach = pcntOnReach;
    pcnt_unit_register_event_callbacks(unit, &cbs, acc);

    pcnt_unit_enable(unit);
    pcnt_unit_clear_count(unit);
    pcnt_unit_start(unit);

    _unit = unit;
    _chan = chan;
    _gpio = gpio;
    pins.claimBus(gpio);
    return true;
}

void PulseCounter::end() {
    if (!_unit) return;
    pcnt_unit_handle_t unit = (pcnt_unit_handle_t)_unit;
    pcnt_unit_stop(unit);
    pcnt_unit_disable(unit);
    if (_chan) pcnt_del_channel((pcnt_channel_handle_t)_chan);
    pcnt_del_unit(unit);
    if (_gpio >= 0) pins.release(_gpio);
    _unit = _chan = nullptr;
    _gpio = -1;
    _gating = false;
}

void PulseCounter::clear() {
    if (!_unit) return;
    pcnt_unit_clear_count((pcnt_unit_handle_t)_unit);
    for (int i = 0; i < 4; i++) s_accum[i].wraps = 0;
}

int64_t PulseCounter::count() {
    if (!_unit) return 0;
    int raw = 0;
    pcnt_unit_get_count((pcnt_unit_handle_t)_unit, &raw);
    int64_t total = raw;
    for (int i = 0; i < 4; i++) total += s_accum[i].wraps;
    return total;
}

void PulseCounter::startGate(uint32_t gateMs) {
    if (!_unit) return;
    if (gateMs < 20)    gateMs = 20;
    if (gateMs > 10000) gateMs = 10000;
    _gateMs  = gateMs;
    clear();
    _gateEnd = millis() + gateMs;
    _gating  = true;
}

bool PulseCounter::ready() {
    if (!_gating) return false;
    if ((int32_t)(millis() - _gateEnd) < 0) return false;
    int64_t n = count();
    _hz = (float)n * 1000.0f / (float)_gateMs;
    _gating = false;
    return true;
}

}  // namespace cg
