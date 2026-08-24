/**
 * Board.h — Cardputer ADV hardware map.
 *
 * Every pin fact in this file is taken from the M5Stack Cardputer-Adv PinMap
 * and from M5Unified's own board tables (see docs/HARDWARE.md for sources).
 * Nothing here is guessed, because getting it wrong on this board means
 * bit-banging the pins that run the keyboard.
 *
 * The three facts that matter most on the ADV:
 *
 *   1. G8/G9 are the SYSTEM I2C bus. The TCA8418 keyboard controller, the
 *      BMI270 IMU and the ES8311 audio codec all live on it. They are brought
 *      out on EXT pins 8/10 so you can hang more I2C devices there, but you
 *      must never drive them as plain GPIO -- you would lose the keyboard.
 *
 *   2. G14/G39/G40 are the microSD SPI bus (MOSI/MISO/CLK). They are free to
 *      use as GPIO as long as you are not using the SD card in the same
 *      session, so they are offered behind an opt-in setting.
 *
 *   3. ESP32-S3 ADC1 is GPIO1..GPIO10 and ADC2 is GPIO11..GPIO20. G39 and G40
 *      have no ADC at all. (The v1 firmware claimed they were ADC1-capable;
 *      they are not, which is why analog reads on them always returned 0.)
 */

#pragma once
#include <stdint.h>
#include <stddef.h>

namespace cg {

enum PinFlag : uint16_t {
    PF_NONE     = 0,
    PF_FREE     = 1 << 0,   // no onboard peripheral contends for this pin
    PF_ADC1     = 1 << 1,   // ADC1 — always available
    PF_ADC2     = 1 << 2,   // ADC2 — usable here because we never start WiFi
    PF_GROVE    = 1 << 3,   // on the HY2.0-4P Grove port
    PF_EXT      = 1 << 4,   // on the EXT 2.54-14P header
    PF_SD       = 1 << 5,   // shared with the microSD SPI bus
    PF_SYS_I2C  = 1 << 6,   // system I2C — keyboard/IMU/codec
    PF_LOCKED   = 1 << 7,   // never offer as general-purpose GPIO
};

struct PinInfo {
    int8_t      gpio;
    const char* header;   // physical position, e.g. "EXT-13"
    const char* silk;     // silkscreen label, e.g. "CS"
    uint16_t    flags;
    const char* warn;     // non-null when using the pin has a consequence
};

// ── Onboard peripherals we can drive without any wiring ───────────────────
constexpr int PIN_IR_TX     = 44;   // infrared emitter
constexpr int PIN_SYS_SDA   =  8;
constexpr int PIN_SYS_SCL   =  9;
constexpr int PIN_GROVE_SCL =  1;
constexpr int PIN_GROVE_SDA =  2;
constexpr int PIN_SD_CS     = 12;
constexpr int PIN_SD_MOSI   = 14;
constexpr int PIN_SD_MISO   = 39;
constexpr int PIN_SD_CLK    = 40;
constexpr int PIN_BAT_ADC   = 10;

// Well-known addresses on the system I2C bus, so the scanner can name them.
const char* i2cKnownName(uint8_t addr);

void        boardBegin();
bool        boardIsAdv();
const char* boardName();

int             pinCount();
const PinInfo*  pinTable();
const PinInfo*  pinInfo(int gpio);

bool  pinExposed(int gpio);       // present on Grove or EXT
bool  pinGpioOk(int gpio);        // safe to drive as GPIO (respects PF_LOCKED + SD opt-in)
bool  pinAdcOk(int gpio);
const char* pinWarn(int gpio);    // nullptr when there is nothing to warn about

// "G13 EXT-12" — for wiring guides and pickers.
void  pinLabel(int gpio, char* out, size_t n);

// Pools, rebuilt whenever the "allow microSD pins" setting changes.
void        poolsRebuild();
const int8_t* poolGpio(int& n);   // pins usable as digital I/O or PWM
const int8_t* poolAdc(int& n);    // pins usable as ADC inputs

}  // namespace cg
