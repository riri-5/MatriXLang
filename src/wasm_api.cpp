// Entry point for the WebAssembly build used by the browser IDE.
// Compiled only with Emscripten (see Makefile target "web").
#include "../include/driver.hpp"
#include <emscripten/emscripten.h>
#include <string>

static std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

extern "C" {

// mode: 0 = tokens, 1 = AST, 2 = analyze, 3 = C++, 4 = run
// Returns JSON: {"ok": bool, "output": "...", "errors": "..."}
EMSCRIPTEN_KEEPALIVE
const char* mx_process(const char* source, int mode) {
    static std::string response;
    CompileResult result = compileSource(source, static_cast<Mode>(mode));
    response = std::string("{\"ok\":") + (result.ok ? "true" : "false") +
               ",\"output\":\"" + jsonEscape(result.output) +
               "\",\"errors\":\"" + jsonEscape(result.errors) + "\"}";
    return response.c_str();
}

}
