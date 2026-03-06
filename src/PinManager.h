/**
 * PinManager.h
 * GPIO Abstraction Layer
 */

#pragma once
#include <M5Cardputer.h>
#include <Arduino.h>
#include <vector>
#include <map>

// ── Cardputer expansion header pins (from physical pinout) ─────────────────
// Row 1: G3  G4  G6  G40  G14  G39  G5
// Row 2: G8  G9  G13 G15  (plus power rails)
// G8=SDA, G9=SCL (I2C Grove), reserved by default but user can reassign
static const int SAFE_PINS[]    = { 3, 4, 5, 6, 13, 14, 15, 39, 40 };
static const int SAFE_PIN_COUNT = sizeof(SAFE_PINS) / sizeof(SAFE_PINS[0]);

// ADC1-capable pins only (ADC2 conflicts with WiFi on ESP32-S3)
// ADC1 channels: GPIO 1-10 and GPIO 11-20 on S3
// From safe pins: 3,4,5,6 are ADC1; 39,40 are ADC1; 13,14,15 are ADC2
static const int ADC_PINS[]     = { 3, 4, 5, 6, 39, 40 };
static const int ADC_PIN_COUNT  = sizeof(ADC_PINS) / sizeof(ADC_PINS[0]);

// Grove I2C (on expansion header, marked G8/G9)
static const int GROVE_SDA = 8;
static const int GROVE_SCL = 9;

enum class PinMode_t { UNSET, OUTPUT_PIN, INPUT_PIN, ADC_PIN, PWM_PIN };

struct PinState {
    int        pin;
    PinMode_t  mode     = PinMode_t::UNSET;
    bool       digital  = false;
    int        adcRaw   = 0;
    float      voltage  = 0.0f;
    int        pwmFreq  = 1000;
    int        pwmDuty  = 50;
    int        ledcChan = -1;
};

class PinManager {
public:
    void init();

    const std::vector<int>& safePins() const { return _safePins; }
    const std::vector<int>& adcPins()  const { return _adcPins;  }
    bool isSafe(int pin)               const;

    bool configureOutput(int pin);
    bool configureInput (int pin, bool pullup = true);
    bool configureADC   (int pin);
    bool configurePWM   (int pin, int freqHz = 1000, int dutyPct = 50);

    void     writePin(int pin, bool high);
    bool     readPin (int pin);
    void     togglePin(int pin);

    int      readADC(int pin);
    float    readVoltage(int pin);

    void     setPWMFreq(int pin, int hz);
    void     setPWMDuty(int pin, int pct);

    PinState*       stateOf(int pin);
    const PinState* stateOf(int pin) const;

    void releasePin(int pin);
    void releaseAll();

private:
    std::vector<int>        _safePins;
    std::vector<int>        _adcPins;
    std::map<int, PinState> _states;
    int                     _nextLedcChan = 0;

    int  allocLedcChan();
    bool pinGuard(int pin, const char* caller) const;
};
