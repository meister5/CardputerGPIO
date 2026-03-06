/**
 * PinManager.cpp
 */

#include "PinManager.h"

void PinManager::init() {
    for (int i = 0; i < SAFE_PIN_COUNT; i++) {
        _safePins.push_back(SAFE_PINS[i]);
        _states[SAFE_PINS[i]].pin = SAFE_PINS[i];
    }
    for (int i = 0; i < ADC_PIN_COUNT; i++) {
        _adcPins.push_back(ADC_PINS[i]);
    }
    Serial.printf("[PinManager] %d safe pins registered.\n", (int)_safePins.size());
}

bool PinManager::pinGuard(int pin, const char* caller) const {
    if (!isSafe(pin)) {
        Serial.printf("[PinManager] %s: pin %d is NOT safe!\n", caller, pin);
        return false;
    }
    return true;
}

bool PinManager::isSafe(int pin) const {
    for (int p : _safePins) if (p == pin) return true;
    return false;
}

bool PinManager::configureOutput(int pin) {
    if (!pinGuard(pin, "configureOutput")) return false;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    _states[pin].mode    = PinMode_t::OUTPUT_PIN;
    _states[pin].digital = false;
    return true;
}

bool PinManager::configureInput(int pin, bool pullup) {
    if (!pinGuard(pin, "configureInput")) return false;
    pinMode(pin, pullup ? INPUT_PULLUP : INPUT);
    _states[pin].mode = PinMode_t::INPUT_PIN;
    return true;
}

bool PinManager::configureADC(int pin) {
    bool ok = false;
    for (int p : _adcPins) if (p == pin) { ok = true; break; }
    if (!ok) {
        Serial.printf("[PinManager] configureADC: pin %d not ADC-capable\n", pin);
        return false;
    }
    pinMode(pin, INPUT);
    analogSetPinAttenuation(pin, ADC_11db);
    _states[pin].mode = PinMode_t::ADC_PIN;
    return true;
}

bool PinManager::configurePWM(int pin, int freqHz, int dutyPct) {
    if (!pinGuard(pin, "configurePWM")) return false;
    ledcAttach(pin, freqHz, 10);
    int raw = (int)((dutyPct / 100.0f) * 1023);
    ledcWrite(pin, raw);
    _states[pin].mode     = PinMode_t::PWM_PIN;
    _states[pin].pwmFreq  = freqHz;
    _states[pin].pwmDuty  = dutyPct;
    _states[pin].ledcChan = 0;
    return true;
}

void PinManager::writePin(int pin, bool high) {
    if (!pinGuard(pin, "writePin")) return;
    digitalWrite(pin, high ? HIGH : LOW);
    _states[pin].digital = high;
}

bool PinManager::readPin(int pin) {
    if (!pinGuard(pin, "readPin")) return false;
    bool v = digitalRead(pin) == HIGH;
    _states[pin].digital = v;
    return v;
}

void PinManager::togglePin(int pin) {
    if (!pinGuard(pin, "togglePin")) return;
    writePin(pin, !_states[pin].digital);
}

int PinManager::readADC(int pin) {
    if (!pinGuard(pin, "readADC")) return 0;
    long sum = 0;
    for (int i = 0; i < 4; i++) sum += analogRead(pin);
    int raw = (int)(sum / 4);
    _states[pin].adcRaw  = raw;
    _states[pin].voltage = (raw / 4095.0f) * 3.3f;
    return raw;
}

float PinManager::readVoltage(int pin) {
    readADC(pin);
    return _states[pin].voltage;
}

void PinManager::setPWMFreq(int pin, int hz) {
    if (!pinGuard(pin, "setPWMFreq")) return;
    PinState& s = _states[pin];
    if (s.ledcChan < 0) return;
    hz = constrain(hz, 1, 40000000);
    ledcChangeFrequency(pin, hz, 10);
    s.pwmFreq = hz;
    int raw = (int)((s.pwmDuty / 100.0f) * 1023);
    ledcWrite(pin, raw);
}

void PinManager::setPWMDuty(int pin, int pct) {
    if (!pinGuard(pin, "setPWMDuty")) return;
    PinState& s = _states[pin];
    if (s.ledcChan < 0) return;
    s.pwmDuty = constrain(pct, 0, 100);
    int raw   = (int)((s.pwmDuty / 100.0f) * 1023);
    ledcWrite(pin, raw);
}

PinState* PinManager::stateOf(int pin) {
    auto it = _states.find(pin);
    return (it != _states.end()) ? &it->second : nullptr;
}

const PinState* PinManager::stateOf(int pin) const {
    auto it = _states.find(pin);
    return (it != _states.end()) ? &it->second : nullptr;
}

void PinManager::releasePin(int pin) {
    if (!pinGuard(pin, "releasePin")) return;
    PinState& s = _states[pin];
    if (s.ledcChan >= 0) ledcDetach(pin);
    pinMode(pin, INPUT);
    s.mode     = PinMode_t::UNSET;
    s.ledcChan = -1;
}

void PinManager::releaseAll() {
    for (int p : _safePins) releasePin(p);
}

int PinManager::allocLedcChan() {
    if (_nextLedcChan >= 8) return -1;
    return _nextLedcChan++;
}
