#include "Logger.h"
#include "Board.h"
#include "Pins.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <stdio.h>
#include <string.h>

namespace cg {

Logger logger;

bool Logger::sdPinsFree() {
    const int sd[] = { PIN_SD_MOSI, PIN_SD_MISO, PIN_SD_CLK, PIN_SD_CS };
    for (int i = 0; i < 4; i++)
        if (pins.mode(sd[i]) != PMode::None) return false;
    return true;
}

bool Logger::mountSd() {
    if (_sdMounted) return true;
    if (!sdPinsFree()) {
        snprintf(_err, sizeof(_err), "SD pins in use by tool");
        return false;
    }
    SPI.begin(PIN_SD_CLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS, SPI, 20000000)) {
        SPI.end();
        snprintf(_err, sizeof(_err), "no card / mount failed");
        return false;
    }
    _sdMounted = true;
    return true;
}

void Logger::unmountSd() {
    if (!_sdMounted) return;
    SD.end();
    SPI.end();
    _sdMounted = false;
}

bool Logger::browseBegin() {
    _err[0] = 0;
    if (_sdMounted) return true;      // a capture already has it open
    if (!mountSd()) return false;
    _browsing = true;
    return true;
}

void Logger::browseEnd() {
    if (!_browsing) return;           // a capture owns the mount, leave it
    _browsing = false;
    unmountSd();
}

bool Logger::start(const char* toolId, const char* header, LogSink sink) {
    stop();
    _err[0] = 0;
    if (sink == LogSink::None) return false;

    bool wantSd  = ((uint8_t)sink & (uint8_t)LogSink::Sd) != 0;
    bool wantSer = ((uint8_t)sink & (uint8_t)LogSink::Serial_) != 0;
    uint8_t got = 0;

    if (wantSd && mountSd()) {
        // Find the next free index so runs never overwrite each other.
        for (int i = 0; i < 1000; i++) {
            snprintf(_file, sizeof(_file), "/%.8s%03d.csv", toolId, i);
            if (!SD.exists(_file)) break;
        }
        File* f = new File(SD.open(_file, FILE_WRITE));
        if (f && *f) {
            _fh = f;
            got |= (uint8_t)LogSink::Sd;
        } else {
            delete f;
            unmountSd();
            snprintf(_err, sizeof(_err), "could not create file");
        }
    }

    if (wantSer) {
        if (!Serial) Serial.begin(115200);
        got |= (uint8_t)LogSink::Serial_;
    }

    if (!got) {
        if (!_err[0]) snprintf(_err, sizeof(_err), "no sink available");
        return false;
    }

    _sink    = (LogSink)got;
    _rows    = 0;
    _startMs = millis();
    _lastMs  = 0;

    char line[128];
    snprintf(line, sizeof(line), "ms,%s", header ? header : "value");
    write(line);
    return true;
}

void Logger::stop() {
    if (_fh) {
        File* f = (File*)_fh;
        f->flush();
        f->close();
        delete f;
        _fh = nullptr;
    }
    unmountSd();
    _sink = LogSink::None;
}

void Logger::write(const char* line) {
    if (((uint8_t)_sink & (uint8_t)LogSink::Serial_) && Serial) Serial.println(line);
    if (_fh) {
        File* f = (File*)_fh;
        f->println(line);
        // Flush every 32 rows: often enough that yanking the card loses at
        // most a second of data, rarely enough not to stall the frame rate.
        if ((_rows & 31) == 0) f->flush();
    }
}

bool Logger::row(const char* csv) {
    if (_sink == LogSink::None || !csv) return false;
    uint32_t now = millis();
    if (_lastMs && (now - _lastMs) < _intervalMs) return false;
    _lastMs = now;

    char line[160];
    snprintf(line, sizeof(line), "%lu,%s", (unsigned long)(now - _startMs), csv);
    write(line);
    _rows++;
    return true;
}

uint32_t Logger::seconds() const {
    return _sink == LogSink::None ? 0 : (millis() - _startMs) / 1000;
}

}  // namespace cg
