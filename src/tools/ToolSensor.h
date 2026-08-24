/**
 * ToolSensor.h — 1-Wire and DHT sensors on a single pin.
 *
 * Both protocols are bit-banged here rather than pulled in from a library,
 * for two reasons: the firmware stays installable with nothing but the M5
 * libraries, and the timing stays visible where it can be reasoned about.
 *
 * DS18B20   1-Wire. Reset, SKIP ROM, CONVERT T, read the scratchpad. Also
 *           reads the 64-bit ROM code so you can tell two probes apart.
 * DHT11/22  Single-wire, not 1-Wire despite the resemblance: a start pulse
 *           followed by 40 bits timed by the width of each high period.
 *
 * All three need a pull-up on the data line -- 4.7k to 3V3 for DS18B20,
 * 10k for DHT. The internal pull-up is far too weak (~45k) to be reliable.
 */

#pragma once
#include "../core/Tool.h"

namespace cg {

class ToolSensor : public Tool {
public:
    const char* id()    const override { return "sensor"; }
    const char* name()  const override { return "1-Wire / DHT"; }
    const char* blurb() const override { return "DS18B20, DHT11, DHT22"; }
    Cat         cat()   const override { return Cat::Bus; }

    const Role* roles()     const override;
    int         roleCount() const override { return 1; }

    void onEnter() override;
    void onExit()  override;
    void tick()    override;
    void draw()    override;
    bool onKey(const KeyEvent& ev) override;
    const char* const* help(int& n) const override;
    const char* logHeader() const override;
    bool logRow(char* out, size_t n) override;

private:
    enum class Kind : uint8_t { DS18B20, DHT22, DHT11, COUNT };

    Kind     _kind  = Kind::DS18B20;
    bool     _valid = false;
    float    _tempC = 0, _humid = 0;
    uint8_t  _rom[8] = {};
    bool     _romOk = false;
    uint32_t _lastRead = 0;
    uint32_t _reads = 0, _fails = 0;
    const char* _err = "no reading yet";
    bool     _fahrenheit = false;

    // ── 1-Wire primitives ─────────────────────────────────────────────────
    bool owReset(int g);
    void owWriteBit(int g, bool b);
    bool owReadBit(int g);
    void owWrite(int g, uint8_t v);
    uint8_t owRead(int g);
    static uint8_t crc8(const uint8_t* d, int n);

    bool readDS18B20(int g);
    bool readDHT(int g, bool dht22);

    static const char* kindName(Kind k);
};

extern ToolSensor toolSensor;

}  // namespace cg
