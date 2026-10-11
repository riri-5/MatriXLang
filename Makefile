CXX = g++
EMXX = em++
CXXFLAGS = -std=c++17 -Iinclude -O2 -Wall
CORE = src/lexer.cpp src/parser.cpp src/semantic.cpp src/codegen.cpp src/interpreter.cpp src/driver.cpp
OUT = matrixlang

# Emscripten: keep C++ exception catching on (the parser reports errors by throwing)
EMFLAGS = $(CXXFLAGS) -fexceptions -sALLOW_MEMORY_GROWTH=1

all: $(OUT)

# Native command-line compiler (needs a C++17 compiler, e.g. g++ 9+ or clang++)
$(OUT): $(CORE) src/main.cpp
	$(CXX) $(CXXFLAGS) $(CORE) src/main.cpp -o $(OUT)

# Same command-line compiler, built with Emscripten to run under Node.js
cli: build/matrixlang.js
build/matrixlang.js: $(CORE) src/main.cpp
	mkdir -p build
	$(EMXX) $(EMFLAGS) $(CORE) src/main.cpp -sNODERAWFS=1 -sEXIT_RUNTIME=1 -o $@

# The whole compiler as WebAssembly for the browser IDE (web/)
web: web/matrixlang.js
web/matrixlang.js: $(CORE) src/wasm_api.cpp
	$(EMXX) $(EMFLAGS) $(CORE) src/wasm_api.cpp -sMODULARIZE=1 -sEXPORT_ES6=1 \
		-sEXPORTED_FUNCTIONS=_mx_process -sEXPORTED_RUNTIME_METHODS=ccall \
		-sENVIRONMENT=web,node -o $@

PYTHON ?= python3

test: cli
	$(PYTHON) tests/run_tests.py

# Also checks that the generated C++ prints exactly what the interpreter prints
test-differential: cli
	$(PYTHON) tests/run_tests.py --differential

# Random programs checked three ways: interpreter, generated C++ and NumPy (needs numpy)
fuzz: cli
	$(PYTHON) tests/fuzz.py --programs 300 --mutants 500 --seed 7

bench: web
	node bench/bench.mjs

clean:
	rm -rf $(OUT) build web/matrixlang.js web/matrixlang.wasm

.PHONY: all cli web test test-differential fuzz bench clean
