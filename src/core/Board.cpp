#include "Board.h"
#include "Settings.h"
#include <M5Unified.h>
#include <stdio.h>
#include <string.h>

namespace cg {

// ── The pin map ───────────────────────────────────────────────────────────
// Ordered the way the pins appear on the case: Grove first, then the EXT
// header top-to-bottom, so the picker reads like the physical board.
static const PinInfo TABLE[] = {
    {  1, "Grove-4",  "SCL",     PF_FREE | PF_ADC1 | PF_GROVE, nullptr },
    {  2, "Grove-3",  "SDA",     PF_FREE | PF_ADC1 | PF_GROVE, nullptr },
    {  3, "EXT-1",    "RESET",   PF_FREE | PF_ADC1 | PF_EXT,   nullptr },
    {  4, "EXT-3",    "INT",     PF_FREE | PF_ADC1 | PF_EXT,   nullptr },
    {  6, "EXT-5",    "BUSY",    PF_FREE | PF_ADC1 | PF_EXT,   nullptr },
    {  5, "EXT-13",   "CS",      PF_FREE | PF_ADC1 | PF_EXT,   nullptr },
    { 13, "EXT-12",   "UART_TX", PF_FREE | PF_ADC2 | PF_EXT,   nullptr },
    { 15, "EXT-14",   "UART_RX", PF_FREE | PF_ADC2 | PF_EXT,   nullptr },
    { 40, "EXT-7",    "SCK",     PF_EXT | PF_SD,               "shares microSD CLK" },
    { 14, "EXT-9",    "MOSI",    PF_ADC2 | PF_EXT | PF_SD,     "shares microSD MOSI" },
    { 39, "EXT-11",   "MISO",    PF_EXT | PF_SD,               "shares microSD MISO" },
    {  8, "EXT-8",    "I2C_SDA", PF_ADC1 | PF_EXT | PF_SYS_I2C | PF_LOCKED,
                                 "system I2C: keyboard, IMU, codec" },
    {  9, "EXT-10",   "I2C_SCL", PF_ADC1 | PF_EXT | PF_SYS_I2C | PF_LOCKED,
                                 "system I2C: keyboard, IMU, codec" },
};
static constexpr int TABLE_N = (int)(sizeof(TABLE) / sizeof(TABLE[0]));

static bool s_isAdv = false;
static bool s_radio = false;

// ── Pools ─────────────────────────────────────────────────────────────────
static int8_t s_gpioPool[TABLE_N];
static int    s_gpioPoolN = 0;
static int8_t s_adcPool[TABLE_N];
static int    s_adcPoolN = 0;

void boardBegin() {
    s_isAdv = (M5.getBoard() == m5::board_t::board_M5CardputerADV);
    poolsRebuild();
}

bool boardIsAdv() { return s_isAdv; }

void boardSetRadioActive(bool on) {
    if (s_radio == on) return;
    s_radio = on;
    poolsRebuild();      // ADC2 pins come and go with the radio
}

bool boardRadioActive() { return s_radio; }

const char* boardName() {
    return s_isAdv ? "Cardputer ADV" : "UNSUPPORTED BOARD";
}

int            pinCount() { return TABLE_N; }
const PinInfo* pinTable() { return TABLE; }

const PinInfo* pinInfo(int gpio) {
    for (int i = 0; i < TABLE_N; i++)
        if (TABLE[i].gpio == gpio) return &TABLE[i];
    return nullptr;
}

bool pinExposed(int gpio) { return pinInfo(gpio) != nullptr; }

bool pinGpioOk(int gpio) {
    const PinInfo* p = pinInfo(gpio);
    if (!p) return false;
    if (p->flags & PF_LOCKED) return false;
    if ((p->flags & PF_SD) && !settings.allowSdPins()) return false;
    return true;
}

bool pinAdcOk(int gpio) {
    const PinInfo* p = pinInfo(gpio);
    if (!p) return false;
    if (!(p->flags & (PF_ADC1 | PF_ADC2))) return false;
    // ADC2 is wired to the same hardware the WiFi radio uses. With the web
    // interface running, a read here returns nothing meaningful, so the pin
    // is withdrawn rather than quietly reporting rubbish.
    if ((p->flags & PF_ADC2) && !(p->flags & PF_ADC1) && s_radio) return false;
    return pinGpioOk(gpio);
}

const char* pinWarn(int gpio) {
    const PinInfo* p = pinInfo(gpio);
    return p ? p->warn : "not on any header";
}

void pinLabel(int gpio, char* out, size_t n) {
    const PinInfo* p = pinInfo(gpio);
    if (p) snprintf(out, n, "G%d %s", (int)p->gpio, p->header);
    else   snprintf(out, n, "G%d ?", gpio);
}

void poolsRebuild() {
    s_gpioPoolN = 0;
    s_adcPoolN  = 0;
    for (int i = 0; i < TABLE_N; i++) {
        int g = TABLE[i].gpio;
        if (pinGpioOk(g)) s_gpioPool[s_gpioPoolN++] = (int8_t)g;
        if (pinAdcOk(g))  s_adcPool [s_adcPoolN++]  = (int8_t)g;
    }
}

const int8_t* poolGpio(int& n) { n = s_gpioPoolN; return s_gpioPool; }
const int8_t* poolAdc (int& n) { n = s_adcPoolN;  return s_adcPool;  }

// ── Internal peripherals ──────────────────────────────────────────────────
static const PeriphInfo PERIPH[] = {
    { "display ST7789V2", "G33 34 35 36 37, BL G38" },
    { "keyboard TCA8418", "I2C 0x34 + INT G11"      },
    { "IMU / RTC",        "system I2C G8/G9"        },
    { "audio ES8311",     "G41 43 46, spk G42"      },
    { "microphone",       "PDM on G46/G43"          },
    { "IR emitter",       "G44 (no wiring needed)"  },
    { "microSD",          "CS G12 MOSI G14"         },
    { "",                 "CLK G40 MISO G39"        },
    { "battery sense",    "G10"                     },
    { "Grove port",       "G1 SCL, G2 SDA, 5V"      },
    { "EXT header",       "14 pins, 5V in and out"  },
};

int               periphCount() { return (int)(sizeof(PERIPH) / sizeof(PERIPH[0])); }
const PeriphInfo* periphTable() { return PERIPH; }

// ── I2C address hints ─────────────────────────────────────────────────────
// Enough to make a scan self-explanatory. The three ADV system devices are
// first so that a scan of the system bus never looks like a mystery.
const char* i2cKnownName(uint8_t addr) {
    switch (addr) {
        case 0x34: return "TCA8418 keyboard";
        case 0x18: return "ES8311 codec";
        case 0x68: return "BMI270 IMU / RTC";
        case 0x69: return "BMI270 (alt)";
        case 0x0D: return "QMC5883L mag";
        case 0x1E: return "HMC5883/LSM303";
        case 0x20: case 0x21: case 0x22: case 0x23:
        case 0x24: case 0x25: case 0x26: case 0x27: return "PCF8574 expander";
        case 0x29: return "VL53L0X / TSL2591";
        case 0x38: return "AHT10/20 / FT6236";
        case 0x39: return "APDS-9960 / TSL2561";
        case 0x3C: case 0x3D: return "SSD1306 OLED";
        case 0x40: return "INA219 / SHT3x / HTU21";
        case 0x44: case 0x45: return "SHT3x / SHT4x";
        case 0x48: case 0x49: case 0x4A: case 0x4B: return "ADS1115 / LM75";
        case 0x50: case 0x51: case 0x52: case 0x53:
        case 0x54: case 0x55: case 0x56: case 0x57: return "24Cxx EEPROM";
        case 0x5A: return "MLX90614 / CCS811";
        case 0x60: return "MCP4725 / Si5351";
        case 0x62: return "SCD4x CO2";
        case 0x70: return "TCA9548A mux";
        case 0x76: case 0x77: return "BMP/BME280 / MS5611";
        default:   return nullptr;
    }
}

}  // namespace cg
