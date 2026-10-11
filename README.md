# MatriXLang

**A compiler for a small matrix programming language, written from scratch in C++ and running entirely in your browser through WebAssembly.**

**[Try it live](https://riri-5.github.io/MatriXLang/)** · no install, no server

```
matrix A = [[1,2],[3,4]];
matrix B = [[2,0],[1,2]];
matrix C = A * B + transpose(A);
print(C);
```
```
5 7
12 12
```

## What it does

You write matrix math; the compiler checks it and runs it.

- **Catches shape errors before running.** `A * B` where A is 2x3 and B is 2x2 is rejected with the line number and both shapes, instead of crashing at runtime.
- **Two backends.** An interpreter runs programs directly, and a code generator turns them into standalone C++. A test compares both outputs byte for byte.
- **Runs in the browser.** The whole compiler is compiled to WebAssembly with Emscripten, so the live IDE needs no backend. It shows each stage: tokens, syntax tree, type checks, generated C++ and output.
- **Fast.** Multiplying 256x256 matrices takes about **9 ms**, roughly **6x faster** than the same loop in plain JavaScript (see [Benchmark](#benchmark)).

## How it works

```
source code
   │
   ▼
 Lexer ──────────► tokens        matrix, A, =, [, [, 1, ...
   │
   ▼
 Parser ─────────► syntax tree   recursive descent; precedence: unary -  >  * /  >  + -
   │
   ▼
 Semantic ───────► checked tree  tracks every variable's shape (rows x cols) and rejects
 analysis                        invalid +, -, *, /, transpose before anything runs
   │
   ├──► Interpreter ──► output         (used by the browser IDE and `--run`)
   └──► C++ code generator ──► .cpp    (compile with any C++17 compiler)
```

| Stage | File |
|---|---|
| Lexer | [`src/lexer.cpp`](src/lexer.cpp) |
| Parser (recursive descent) | [`src/parser.cpp`](src/parser.cpp) |
| Semantic analysis (types and dimensions) | [`src/semantic.cpp`](src/semantic.cpp) |
| Interpreter | [`src/interpreter.cpp`](src/interpreter.cpp) |
| C++ code generator | [`src/codegen.cpp`](src/codegen.cpp) |
| Pipeline shared by CLI and browser | [`src/driver.cpp`](src/driver.cpp) |
| WebAssembly entry point | [`src/wasm_api.cpp`](src/wasm_api.cpp) |
| Browser IDE | [`web/`](web/) |

## The language

```
// declare (shape is fixed from here on)
matrix A = [[1, -2], [3, 4]];
matrix I = identity(2);          // also: zeros(rows, cols), ones(rows, cols)

// reassign (must keep the same shape)
A = A * I + 1;

print(A);                        // matrices print row by row
print(transpose(A) - A / 2);
print(-A);
print(2 + 3 * 4);                // scalars work too
```

| Operation | Allowed shapes |
|---|---|
| `A + B`, `A - B` | same shape, or one side a scalar |
| `A * B` | matrix product: columns of A = rows of B; or one side a scalar |
| `A / s` | matrix divided by a scalar |
| `transpose(A)`, `-A` | any matrix |

## Build and run

You need the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) for the browser and Node builds, or any C++17 compiler for the native one.

```bash
make                      # native compiler (g++), produces ./matrixlang
./matrixlang samples/complex_expression.mx --run      # run a program
./matrixlang samples/complex_expression.mx -o out.cpp # generate C++
./matrixlang samples/complex_expression.mx --tokens   # also: --ast, --analyze

make web                  # WebAssembly build for the IDE in web/
cd web && python -m http.server 8000                  # open http://localhost:8000
```

## Testing and verification

The project includes a small compiler-verification setup, all in Python ([`tests/`](tests/)).

### 1. Test suite: `make test`
[`tests/run_tests.py`](tests/run_tests.py) runs **44 hand-written tests** from [`tests/cases/`](tests/cases/):
- **24 valid programs**, each with a hand-computed expected output (`.out`): arithmetic, matrix products of different shapes, transpose, negatives, built-ins, reassignment, float and scientific-notation literals.
- **20 invalid programs**, each with the expected error (`.err`): undefined variables, every kind of shape mismatch, redeclaration, ragged or empty matrices, bad built-in arguments, syntax errors, correct line numbers, and making sure one mistake gives exactly one error.

It writes a pass/fail report to `build/test_report.md` and exits with code 1 if anything fails.

### 2. Differential testing: `make test-differential`
The same suite, but every valid program is **also compiled to C++, built with Clang and run**. Both backends must print exactly the same thing.

### 3. Fuzzing against NumPy: `make fuzz`
[`tests/fuzz.py`](tests/fuzz.py) is a random program generator in the spirit of Csmith. Each random program is generated **twice at the same time**: once in MatriXLang and once as Python/NumPy code. That gives three independent answers to compare:

| | must match |
|---|---|
| interpreter vs generated C++ | exactly |
| interpreter vs NumPy (reference) | within 1e-5 relative (print shows 6 significant digits) |

It also feeds the compiler **randomly broken programs** and checks it never crashes: each one must either run or be rejected with a normal error. Failing programs are saved to `build/fuzz/failures/` and a report goes to `build/fuzz_report.md`. The same `--seed` always gives the same programs, so every failure is reproducible.

```
$ python tests/fuzz.py --programs 300 --mutants 500 --seed 7
300/300 programs agree (interpreter = C++ = NumPy), 500/500 broken programs handled without a crash
```

### Bugs found by fuzzing (fixed, with regression tests)
- **Miscompilation:** the C++ backend wrote number literals with 10 fixed decimals, so `0.0000016467947` turned into `0.0000016468` and the C++ output differed from the interpreter. Literals are now written with 17 significant digits, which round-trips every double exactly. Regression test: [`tests/cases/float_literals.mx`](tests/cases/float_literals.mx).
- **Missing scientific notation:** the lexer rejected literals like `9.05e-07`.

## Benchmark

`make bench` multiplies two N x N matrices. It times the full MatriXLang pipeline (lex, parse, check, run) in WebAssembly against the same loop written in plain JavaScript, taking the median of 7 runs (Node.js 20):

| Size | MatriXLang (WebAssembly) | Plain JavaScript | Speedup |
|---|---|---|---|
| 64 x 64 | 0.3 ms | 1.1 ms | ~4x |
| 128 x 128 | 1.2 ms | 7.3 ms | ~6x |
| 256 x 256 | 9.3 ms | 58 ms | ~6x |
| 512 x 512 | 73 ms | 650-740 ms | ~9-10x |

## History

The first version (a semester compiler-design project) generated C++ on a Node.js server and compiled it with Emscripten for each request. Then the compiler itself was compiled to WebAssembly and given an interpreter backend, so the IDE became a static site that anyone can open. Along the way: negative numbers, unary minus, `identity`/`zeros`/`ones`, checks for redeclared variables and reassignments that change shape, single-error reporting instead of cascades, and the automated test suite.

## License

[MIT](LICENSE)
