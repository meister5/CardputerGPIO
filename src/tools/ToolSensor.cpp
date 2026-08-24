#include "ToolSensor.h"
#include <string.h>

namespace cg {

ToolSensor toolSensor;

static const Role ROLES[] = {
    { "DATA", RoleDir::In, "-> sensor data + 4.7k to 3V3", -1 },
};
const Role* ToolSensor::roles() const { return ROLES; }

static const char* HELP[] = {
    "S      next sensor type",
    "U      Celsius / Fahrenheit",
    "R      force a re-read",
    "",
    "Wiring, all three types:",
    "  VCC  -> 3V3        GND -> GND",
    "  DATA -> the pin, plus a pull-up",
    "         resistor to 3V3.",
    "",
    "4.7k for DS18B20, 10k for DHT. Without",
    "it the line cannot return high fast",
    "enough and every read fails CRC.",
    "",
    "DHT sensors will not answer faster than",
    "once every 2 seconds.",
};
const char* const* ToolSensor::help(int& n) const {
    n = (int)(sizeof(HELP) / sizeof(HELP[0]));
    return HELP;
}

const char* ToolSensor::kindName(Kind k) {
    switch (k) {
        case Kind::DS18B20: return "DS18B20";
        case Kind::DHT22:   return "DHT22";
        case Kind::DHT11:   return "DHT11";
        default:            return "?";
    }
}

// ── 1-Wire bit banging ────────────────────────────────────────────────────
// The line is never driven high: it is either pulled low, or released and
// allowed to float up through the external pull-up. Driving it high would
// fight a device holding it down.
static inline void owLow(int g)     { pinMode(g, OUTPUT); digitalWrite(g, LOW); }
static inline void owRelease(int g) { pinMode(g, INPUT); }

bool ToolSensor::owReset(int g) {
    owLow(g);
    delayMicroseconds(480);
    owRelease(g);
    delayMicroseconds(70);
    bool present = (digitalRead(g) == LOW);
    delayMicroseconds(410);
    return present;
}

void ToolSensor::owWriteBit(int g, bool b) {
    noInterrupts();
    owLow(g);
    delayMicroseconds(b ? 6 : 60);
    owRelease(g);
    delayMicroseconds(b ? 64 : 10);
    interrupts();
}

bool ToolSensor::owReadBit(int g) {
    noInterrupts();
    owLow(g);
    delayMicroseconds(3);
    owRelease(g);
    delayMicroseconds(10);
    bool v = digitalRead(g) == HIGH;
    interrupts();
    delayMicroseconds(50);
    return v;
}

void ToolSensor::owWrite(int g, uint8_t v) {
    for (int i = 0; i < 8; i++) owWriteBit(g, (v >> i) & 1);
}

uint8_t ToolSensor::owRead(int g) {
    uint8_t v = 0;
    for (int i = 0; i < 8; i++) if (owReadBit(g)) v |= (uint8_t)(1 << i);
    return v;
}

uint8_t ToolSensor::crc8(const uint8_t* d, int n) {
    uint8_t crc = 0;
    while (n--) {
        uint8_t b = *d++;
        for (int i = 0; i < 8; i++) {
            uint8_t mix = (uint8_t)((crc ^ b) & 0x01);
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            b >>= 1;
        }
    }
    return crc;
}

bool ToolSensor::readDS18B20(int g) {
    if (!owReset(g)) { _err = "no device on the bus"; return false; }

    // ROM code, so two probes on a bench are distinguishable.
    owWrite(g, 0x33);                       // READ ROM (single device only)
    for (int i = 0; i < 8; i++) _rom[i] = owRead(g);
    _romOk = (_rom[0] != 0x00 && _rom[0] != 0xFF && crc8(_rom, 8) == 0);

    if (!owReset(g)) { _err = "device dropped off"; return false; }
    owWrite(g, 0xCC);                       // SKIP ROM
    owWrite(g, 0x44);                       // CONVERT T
    delay(750);                             // 12-bit conversion time

    if (!owReset(g)) { _err = "no presence after convert"; return false; }
    owWrite(g, 0xCC);
    owWrite(g, 0xBE);                       // READ SCRATCHPAD

    uint8_t sp[9];
    for (int i = 0; i < 9; i++) sp[i] = owRead(g);
    if (crc8(sp, 9) != 0) { _err = "scratchpad CRC failed"; return false; }

    int16_t raw = (int16_t)((sp[1] << 8) | sp[0]);
    _tempC = raw / 16.0f;
    _humid = -1;
    _err   = nullptr;
    return true;
}

bool ToolSensor::readDHT(int g, bool dht22) {
    uint8_t data[5] = {};

    // Start pulse, then hand the line back to the sensor.
    pinMode(g, OUTPUT);
    digitalWrite(g, LOW);
    delay(dht22 ? 2 : 20);
    pinMode(g, INPUT_PULLUP);

    noInterrupts();
    auto waitFor = [&](int level, uint32_t limitUs) -> uint32_t {
        uint32_t t0 = micros();
        while (digitalRead(g) != level) {
            if ((micros() - t0) > limitUs) return 0;
        }
        return micros() - t0;
    };

    if (!waitFor(LOW,  200)) { interrupts(); _err = "no response pulse"; return false; }
    if (!waitFor(HIGH, 200)) { interrupts(); _err = "response stuck low"; return false; }
    if (!waitFor(LOW,  200)) { interrupts(); _err = "response stuck high"; return false; }

    // 40 bits: each is a ~50 us low, then a high whose length is the value.
    for (int i = 0; i < 40; i++) {
        if (!waitFor(HIGH, 150)) { interrupts(); _err = "bit timeout"; return false; }
        uint32_t t0 = micros();
        if (!waitFor(LOW, 200))  { interrupts(); _err = "bit stuck high"; return false; }
        uint32_t width = micros() - t0;
        if (width > 45) data[i / 8] |= (uint8_t)(1 << (7 - (i % 8)));
    }
    interrupts();

    uint8_t sum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (sum != data[4]) { _err = "checksum failed"; return false; }

    if (dht22) {
        _humid = ((data[0] << 8) | data[1]) / 10.0f;
        int16_t t = (int16_t)(((data[2] & 0x7F) << 8) | data[3]);
        _tempC = t / 10.0f;
        if (data[2] & 0x80) _tempC = -_tempC;
    } else {
        _humid = data[0] + data[1] / 10.0f;
        _tempC = data[2] + (data[3] & 0x0F) / 10.0f;
    }
    _romOk = false;
    _err   = nullptr;
    return true;
}

void ToolSensor::onEnter() {
    _valid = false;
    _reads = _fails = 0;
    _err   = "reading...";
    _lastRead = 0;
    if (pin(0) >= 0) pins.claimBus(pin(0));
}

void ToolSensor::onExit() {
    if (pin(0) >= 0) pins.release(pin(0));
}

void ToolSensor::tick() {
    int g = pin(0);
    if (g < 0) return;

    uint32_t now = millis();
    uint32_t interval = (_kind == Kind::DS18B20) ? 1200 : 2200;
    if (_lastRead && (uint32_t)(now - _lastRead) < interval) return;
    _lastRead = now;

    bool ok = (_kind == Kind::DS18B20) ? readDS18B20(g)
                                       : readDHT(g, _kind == Kind::DHT22);
    _valid = ok;
    _reads++;
    if (!ok) _fails++;
}

bool ToolSensor::onKey(const KeyEvent& ev) {
    if (ev.ci('s')) {
        _kind  = (Kind)(((int)_kind + 1) % (int)Kind::COUNT);
        _valid = false;
        _lastRead = 0;
        _err = "reading...";
        ui.notify("%s", kindName(_kind));
        return true;
    }
    if (ev.ci('u')) { _fahrenheit = !_fahrenheit; return true; }
    if (ev.ci('r')) { _lastRead = 0; ui.notify("re-reading"); return true; }
    return false;
}

void ToolSensor::draw() {
    char right[20];
    pinLabel(pin(0), right, sizeof(right));
    ui.header(kindName(_kind), right, C_HDR);

    if (_valid) {
        float t = _fahrenheit ? (_tempC * 9.0f / 5.0f + 32.0f) : _tempC;
        ui.textBigf(6, BODY_Y + 6, C_ROLE_ADC, 4, "%5.1f", t);
        ui.textBig(126, BODY_Y + 18, C_DIM, 2, _fahrenheit ? "F" : "C");

        if (_humid >= 0) {
            ui.textBigf(150, BODY_Y + 10, C_INFO, 2, "%4.1f%%", _humid);
            ui.text(150, BODY_Y + 28, C_DIM, "humidity");
            ui.hbar(150, BODY_Y + 38, 84, 8, _humid, C_INFO);
        }

        if (_romOk) {
            ui.text(6, BODY_Y + 52, C_DIM, "ROM");
            char buf[26];
            int p = 0;
            for (int i = 7; i >= 0 && p < 24; i--)
                p += snprintf(buf + p, sizeof(buf) - p, "%02X", _rom[i]);
            ui.textf(32, BODY_Y + 52, C_FAINT, "%s", buf);
        }
    } else {
        ui.text(8, BODY_Y + 16, C_LOW, "No valid reading.");
        ui.textf(8, BODY_Y + 30, C_WARN, "%.36s", _err ? _err : "unknown");
        ui.text(8, BODY_Y + 48, C_DIM, "Check the pull-up resistor to 3V3");
        ui.text(8, BODY_Y + 58, C_DIM, "and that VCC/GND are connected.");
    }

    ui.footerf("[S] type  [U] C/F  [R] read   ok %lu/%lu",
               (unsigned long)(_reads - _fails), (unsigned long)_reads);
}


const char* ToolSensor::logHeader() const { return "sensor,tempC,humidity"; }

bool ToolSensor::logRow(char* out, size_t n) {
    if (!_valid) return false;
    if (_kind == Kind::DS18B20) snprintf(out, n, "%s,%.2f,", kindName(_kind), _tempC);
    else snprintf(out, n, "%s,%.1f,%.1f", kindName(_kind), _tempC, _humid);
    return true;
}

}  // namespace cg
