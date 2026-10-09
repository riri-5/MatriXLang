#include "../include/driver.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        exit(1);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <input.mx> [--run] [-o output.cpp] [--tokens] [--ast] [--analyze]" << std::endl;
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile;
    Mode mode = Mode::CPP;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            outputFile = argv[++i];
        } else if (arg == "--tokens") {
            mode = Mode::TOKENS;
        } else if (arg == "--ast") {
            mode = Mode::AST;
        } else if (arg == "--analyze") {
            mode = Mode::ANALYZE;
        } else if (arg == "--run") {
            mode = Mode::RUN;
        }
    }

    CompileResult result = compileSource(readFile(inputFile), mode);
    std::cerr << result.errors;
    if (!result.ok) {
        return 1;
    }

    // C++ goes to a file (output.cpp by default) unless "-o -" asks for stdout
    if (mode == Mode::CPP && outputFile != "-") {
        if (outputFile.empty()) {
            outputFile = "output.cpp";
        }
        std::ofstream out(outputFile);
        if (!out.is_open()) {
            std::cerr << "Error: Cannot write to file " << outputFile << std::endl;
            return 1;
        }
        out << result.output;
        std::cout << "Generated C++ code written to " << outputFile << std::endl;
        return 0;
    }

    std::cout << result.output;
    return 0;
}
