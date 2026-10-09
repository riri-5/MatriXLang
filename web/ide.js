// MatriXLang IDE client-side JavaScript
// The whole compiler (lexer, parser, semantic analyzer, C++ code generator and
// interpreter) is C++ compiled to WebAssembly, so nothing runs on a server.

import createCompiler from "./matrixlang.js";

const MODE = { TOKENS: 0, AST: 1, ANALYZE: 2, CPP: 3, RUN: 4 };

// DOM elements
const sourceEl = document.getElementById("source");
const tokensView = document.getElementById("tokensView");
const astView = document.getElementById("astView");
const semanticView = document.getElementById("semanticView");
const cppView = document.getElementById("cppView");
const outputView = document.getElementById("outputView");
const runBtn = document.getElementById("runBtn");
const exampleSelect = document.getElementById("exampleSelect");

let compiler = null;

function compile(source, mode) {
    const json = compiler.ccall("mx_process", "string", ["string", "number"], [source, mode]);
    return JSON.parse(json);
}

function show(view, text, isError) {
    view.textContent = text;
    view.classList.toggle("status-error", Boolean(isError));
}

// Run every compiler stage and fill each panel
function handleRun() {
    const source = sourceEl.value;
    if (!source.trim()) {
        show(outputView, "Please enter some MatriXLang code", true);
        return;
    }

    const t0 = performance.now();

    const tokens = compile(source, MODE.TOKENS);
    show(tokensView, tokens.ok ? tokens.output : tokens.output + tokens.errors, !tokens.ok);

    const ast = compile(source, MODE.AST);
    show(astView, ast.ok ? ast.output : ast.errors, !ast.ok);

    const semantic = compile(source, MODE.ANALYZE);
    show(semanticView, semantic.ok ? semantic.output : semantic.errors, !semantic.ok);

    const cpp = compile(source, MODE.CPP);
    show(cppView, cpp.ok ? cpp.output : "Fix the errors above to see the generated C++", !cpp.ok);

    const run = compile(source, MODE.RUN);
    const elapsed = (performance.now() - t0).toFixed(1);
    if (run.ok) {
        const output = run.output || "(program printed nothing)";
        show(outputView, output + `\n-- compiled and ran in ${elapsed} ms (WebAssembly)`, false);
    } else {
        show(outputView, run.errors, true);
    }
}

// Example programs
const examples = {
    basic_addition: `// Basic: Matrix Addition
// Demonstrates simple matrix addition operation
matrix A = [[1,2],[3,4]];
matrix B = [[5,6],[7,8]];
matrix C = A + B;
print(C);`,
    scalar_multiplication: `// Basic: Scalar Multiplication
// Demonstrates multiplying a matrix by a scalar
matrix M = [[1,2],[3,4]];
matrix N = 2 * M;
print(N);`,
    matrix_multiplication: `// Intermediate: Matrix Multiplication
// Demonstrates matrix multiplication (not element-wise)
matrix A = [[1,2],[3,4]];
matrix B = [[2,0],[1,2]];
matrix C = A * B;
print(C);`,
    transpose_operation: `// Intermediate: Transpose Operation
// Demonstrates matrix transpose function
matrix A = [[1,2,3],[4,5,6]];
matrix B = transpose(A);
print(B);`,
    builtins: `// Intermediate: Built-in matrices
// identity(n), zeros(rows, cols) and ones(rows, cols)
matrix I = identity(3);
matrix A = [[1,-2,3],[4,5,-6],[7,8,9]];
print(A * I);
print(ones(2,3) * 0.5);`,
    complex_expression: `// Advanced: Complex Expression
// Demonstrates multiple operations: multiplication, transpose, and addition
matrix A = [[1,2],[3,4]];
matrix B = [[2,0],[1,2]];
matrix C = A * B + transpose(A);
print(C);`,
    mixed_operations: `// Advanced: Mixed Operations
// Demonstrates scalar and matrix operations combined
matrix A = [[1,2],[3,4]];
matrix B = [[2,1],[1,2]];
matrix C = 3 * A + B - 1;
print(-C);`,
    type_error: `// Error demo: the compiler catches shape errors before running
matrix A = [[1,2,3],[4,5,6]];   // 2x3
matrix B = [[1,2],[3,4]];       // 2x2
matrix C = A * B;               // 3 columns vs 2 rows
print(C);`
};

// Event listeners
runBtn.addEventListener("click", handleRun);
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

// Initialize UI
tokensView.textContent = "Tokens will appear here";
astView.textContent = "AST will appear here";
semanticView.textContent = "Semantic analysis results will appear here";
cppView.textContent = "Generated C++ will appear here";
outputView.textContent = "Loading the compiler (WebAssembly)...";

createCompiler().then((module) => {
    compiler = module;
    runBtn.disabled = false;
    runBtn.textContent = "Run ▶";
    handleRun();
}).catch((error) => {
    show(outputView, "Could not load the compiler: " + error.message, true);
});
