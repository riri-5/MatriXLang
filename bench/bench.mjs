// Benchmark: N x N matrix multiplication
//   MatriXLang  - the full pipeline (lex, parse, type-check, run) inside the WebAssembly build
//   JavaScript  - the same i-k-j triple loop written directly in plain JS
// Each case runs 7 times; the median is reported.
//
//   node bench/bench.mjs

import Factory from "../web/matrixlang.js";

const M = await Factory();
const runMx = src => JSON.parse(M.ccall("mx_process", "string", ["string", "number"], [src, 4]));

function jsMatmul(n) {
    const a = Array.from({ length: n }, () => new Array(n).fill(1));
    const b = Array.from({ length: n }, () => new Array(n).fill(1));
    const c = Array.from({ length: n }, () => new Array(n).fill(0));
    for (let i = 0; i < n; i++) {
        for (let k = 0; k < n; k++) {
            const aik = a[i][k];
            for (let j = 0; j < n; j++) {
                c[i][j] += aik * b[k][j];
            }
        }
    }
    return c[n - 1][n - 1];
}

function median(fn, runs = 7) {
    const times = [];
    for (let r = 0; r < runs; r++) {
        const t0 = performance.now();
        fn();
        times.push(performance.now() - t0);
    }
    times.sort((x, y) => x - y);
    return times[Math.floor(runs / 2)];
}

console.log("size      MatriXLang (WASM)   plain JS    speedup");
for (const n of [64, 128, 256, 512]) {
    // print the sum of all entries of B (should be n^3) so the work can't be skipped
    const src = `matrix A = ones(${n},${n});\nmatrix B = A * A;\nprint(ones(1,${n}) * B * ones(${n},1));\n`;
    const check = runMx(src);
    // print uses 6 significant digits (std::cout default), so compare with a tolerance
    if (!check.ok || Math.abs(Number(check.output) - n ** 3) > 1e-5 * n ** 3) {
        throw new Error("unexpected result: " + JSON.stringify(check));
    }
    const wasmMs = median(() => runMx(src));
    const jsMs = median(() => jsMatmul(n));
    console.log(
        `${String(n).padEnd(4)}x${String(n).padEnd(4)} ${wasmMs.toFixed(2).padStart(10)} ms ${jsMs.toFixed(2).padStart(12)} ms ${(jsMs / wasmMs).toFixed(2).padStart(8)}x`
    );
}
