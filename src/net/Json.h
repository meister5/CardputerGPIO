/**
 * Json.h — a JSON writer that fits in a header.
 *
 * The API surface here is small and entirely ours: a handful of objects and
 * arrays of numbers and short strings. Pulling in ArduinoJson for that would
 * add a dependency, a version to track, and more code than the whole web
 * layer. This writes straight into a caller-owned buffer and truncates
 * rather than overflowing.
 *
 * Nothing parses JSON on the device. Commands arrive as ordinary query
 * parameters, which the HTTP server has already split for us.
 */

#pragma once
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace cg {

class Json {
public:
    Json(char* buf, size_t cap) : _b(buf), _cap(cap) { _b[0] = 0; }

    // ── Structure ─────────────────────────────────────────────────────────
    Json& objOpen()  { sep(); raw('{'); _first = true;  return *this; }
    Json& objClose() { raw('}'); _first = false;        return *this; }
    Json& arrOpen()  { sep(); raw('['); _first = true;  return *this; }
    Json& arrClose() { raw(']'); _first = false;        return *this; }

    // Named containers.
    Json& obj(const char* k) { key(k); raw('{'); _first = true; return *this; }
    Json& arr(const char* k) { key(k); raw('['); _first = true; return *this; }

    // ── Named members ─────────────────────────────────────────────────────
    Json& kv(const char* k, const char* v)   { key(k); str(v); return *this; }
    Json& kv(const char* k, int v)           { key(k); fmt("%d", v); return *this; }
    Json& kv(const char* k, long v)          { key(k); fmt("%ld", v); return *this; }
    Json& kv(const char* k, unsigned long v) { key(k); fmt("%lu", v); return *this; }
    Json& kv(const char* k, long long v)     { key(k); fmt("%lld", v); return *this; }
    Json& kv(const char* k, bool v)          { key(k); fmt("%s", v ? "true" : "false"); return *this; }
    Json& kv(const char* k, float v, int dp = 2) {
        key(k);
        if (isnan(v) || isinf(v)) fmt("null");
        else                      fmt("%.*f", dp, v);
        return *this;
    }
    Json& kvNull(const char* k) { key(k); fmt("null"); return *this; }
    Json& kvf(const char* k, const char* f, ...) {
        char tmp[128];
        va_list ap; va_start(ap, f);
        vsnprintf(tmp, sizeof(tmp), f, ap);
        va_end(ap);
        key(k); str(tmp);
        return *this;
    }

    // ── Bare array elements ───────────────────────────────────────────────
    Json& val(const char* v) { sep(); str(v); return *this; }
    Json& val(int v)         { sep(); fmt("%d", v); return *this; }
    Json& val(float v, int dp = 2) {
        sep();
        if (isnan(v) || isinf(v)) fmt("null");
        else                      fmt("%.*f", dp, v);
        return *this;
    }

    const char* c_str()     const { return _b; }
    size_t      length()    const { return _n; }
    bool        truncated() const { return _trunc; }

private:
    char*  _b;
    size_t _cap;
    size_t _n     = 0;
    bool   _first = true;
    bool   _trunc = false;

    void raw(char c) {
        if (_n + 1 >= _cap) { _trunc = true; return; }
        _b[_n++] = c;
        _b[_n]   = 0;
    }
    void fmt(const char* f, ...) {
        if (_n + 1 >= _cap) { _trunc = true; return; }
        va_list ap; va_start(ap, f);
        int w = vsnprintf(_b + _n, _cap - _n, f, ap);
        va_end(ap);
        if (w < 0) return;
        if ((size_t)w >= _cap - _n) { _n = _cap - 1; _trunc = true; }
        else _n += (size_t)w;
    }
    // Emits the separating comma, then marks the slot as taken. A key/value
    // pair calls this once, for the key -- the value that follows must not.
    void sep() {
        if (!_first) raw(',');
        _first = false;
    }
    void key(const char* k) { sep(); str(k); raw(':'); }

    // Minimal escaping: quotes, backslash and control characters are all we
    // can actually produce from the data on this device.
    void str(const char* s) {
        raw('"');
        for (; s && *s; s++) {
            unsigned char c = (unsigned char)*s;
            if      (c == '"' || c == '\\') { raw('\\'); raw((char)c); }
            else if (c == '\n') { raw('\\'); raw('n'); }
            else if (c == '\r') { raw('\\'); raw('r'); }
            else if (c == '\t') { raw('\\'); raw('t'); }
            else if (c < 0x20)  { fmt("\\u%04x", c); }
            else raw((char)c);
        }
        raw('"');
    }
};

}  // namespace cg
