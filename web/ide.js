// MatriXLang IDE
// the whole compiler (lexer, parser, semantic analysis, C++ codegen, interpreter)
// is written in C++ and compiled to WebAssembly, so everything runs in the browser
// no server needed :)

import createCompiler from "./matrixlang.js";

// these numbers must match the Mode enum in include/driver.hpp
const MODE = {
    TOKENS: 0,
    AST: 1,
    ANALYZE: 2,
    CPP: 3,
    RUN: 4,
};

// grab all the elements we need
const sourceEl = document.getElementById("source");
const tokensView = document.getElementById("tokensView");
const astView = document.getElementById("astView");
const semanticView = document.getElementById("semanticView");
const cppView = document.getElementById("cppView");
const outputView = document.getElementById("outputView");
const runBtn = document.getElementById("runBtn");
const exampleSelect = document.getElementById("exampleSelect");

let compiler = null; // the wasm module, set once it finishes loading

// calls mx_process() from src/wasm_api.cpp
// it returns a JSON string like {"ok": true, "output": "...", "errors": "..."}
const compile = (source, mode) => {
    const json = compiler.ccall("mx_process", "string", ["string", "number"], [source, mode]);
    return JSON.parse(json);
};

// put text in a panel, and make it red if it's an error
const show = (view, text, isError = false) => {
    view.textContent = text;
    view.classList.toggle("status-error", isError);
};

const handleRun = () => {
    const source = sourceEl.value;

    if (!source.trim()) {
        show(outputView, "write some MatriXLang code first", true);
        return;
    }

    const startTime = performance.now();

    // step 1: lexer
    const tokens = compile(source, MODE.TOKENS);
    show(tokensView, tokens.output + tokens.errors, !tokens.ok);

    // step 2: parser
    const ast = compile(source, MODE.AST);
    show(astView, ast.ok ? ast.output : ast.errors, !ast.ok);

    // step 3: semantic analysis (types + matrix shapes)
    const semantic = compile(source, MODE.ANALYZE);
    show(semanticView, semantic.ok ? semantic.output : semantic.errors, !semantic.ok);

    // step 4: C++ code generation
    const cpp = compile(source, MODE.CPP);
    show(cppView, cpp.ok ? cpp.output : "fix the errors above to see the generated C++", !cpp.ok);

    // step 5: actually run it
    const result = compile(source, MODE.RUN);
    const timeTaken = (performance.now() - startTime).toFixed(1);

    if (result.ok) {
        const output = result.output || "(program didn't print anything)";
        show(outputView, `${output}\n-- compiled and ran in ${timeTaken} ms (WebAssembly)`);
    } else {
        show(outputView, result.errors, true);
    }
};

// example programs for the dropdown
const examples = {
    basic_addition: `// Basic: Matrix Addition
matrix A = [[1,2],[3,4]];
matrix B = [[5,6],[7,8]];
matrix C = A + B;
print(C);`,

    scalar_multiplication: `// Basic: Scalar Multiplication
matrix M = [[1,2],[3,4]];
matrix N = 2 * M;
print(N);`,

    matrix_multiplication: `// Intermediate: Matrix Multiplication (not element-wise)
matrix A = [[1,2],[3,4]];
matrix B = [[2,0],[1,2]];
matrix C = A * B;
print(C);`,

    transpose_operation: `// Intermediate: Transpose
matrix A = [[1,2,3],[4,5,6]];
matrix B = transpose(A);
print(B);`,

    builtins: `// Intermediate: identity(n), zeros(rows, cols), ones(rows, cols)
matrix I = identity(3);
matrix A = [[1,-2,3],[4,5,-6],[7,8,9]];
print(A * I);
print(ones(2,3) * 0.5);`,

    complex_expression: `// Advanced: multiply, transpose and add together
matrix A = [[1,2],[3,4]];
matrix B = [[2,0],[1,2]];
matrix C = A * B + transpose(A);
print(C);`,

    mixed_operations: `// Advanced: scalars and matrices mixed
matrix A = [[1,2],[3,4]];
matrix B = [[2,1],[1,2]];
matrix C = 3 * A + B - 1;
print(-C);`,

    type_error: `// Error demo: the compiler catches shape errors before running anything
matrix A = [[1,2,3],[4,5,6]];   // 2x3
matrix B = [[1,2],[3,4]];       // 2x2
matrix C = A * B;               // 3 columns but 2 rows -> error
print(C);`,
};

// buttons + shortcuts
runBtn.addEventListener("click", handleRun);

// ctrl + enter (or cmd + enter on mac) runs the code
sourceEl.addEventListener("keydown", (e) => {
    if (e.key === "Enter" && (e.ctrlKey || e.metaKey)) {
        e.preventDefault();
        if (compiler) handleRun();
    }
});

exampleSelect.addEventListener("change", () => {
    const selected = exampleSelect.value;
    if (selected && examples[selected]) {
        sourceEl.value = examples[selected];
        if (compiler) handleRun();
    }
});

// placeholder text until the compiler is ready
tokensView.textContent = "tokens will show up here";
astView.textContent = "AST will show up here";
semanticView.textContent = "semantic analysis results will show up here";
cppView.textContent = "generated C++ will show up here";
outputView.textContent = "loading the compiler (WebAssembly)...";

// load the wasm compiler, then run the default program once
const init = async () => {
    try {
        compiler = await createCompiler();
        runBtn.disabled = false;
        runBtn.textContent = "Run ▶";
        handleRun();
    } catch (error) {
        show(outputView, "could not load the compiler: " + error.message, true);
    }
};

init();
