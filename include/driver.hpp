#ifndef DRIVER_HPP
#define DRIVER_HPP

#include <string>

// What the compiler should produce for a given source file
enum class Mode {
    TOKENS,   // token stream
    AST,      // syntax tree
    ANALYZE,  // semantic analysis report
    CPP,      // generated C++ code
    RUN       // execute the program (interpreter backend)
};

struct CompileResult {
    bool ok;
    std::string output;   // the requested artifact (tokens, AST, C++ or program output)
    std::string errors;   // one error per line
};

// Runs the full pipeline (lexer -> parser -> semantic analysis -> backend).
// Shared by the command-line tool and the WebAssembly build.
CompileResult compileSource(const std::string& source, Mode mode);

#endif // DRIVER_HPP
