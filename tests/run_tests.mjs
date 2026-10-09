// Test runner for MatriXLang.
//
//   node tests/run_tests.mjs                 run every case through the interpreter
//   node tests/run_tests.mjs --differential  also compile each valid program to C++,
//                                            build it with em++ and check that both
//                                            backends print exactly the same output
//
// tests/cases/<name>.mx + <name>.out  -> program must print exactly <name>.out
// tests/cases/<name>.mx + <name>.err  -> compiler must fail with an error containing that text
//                                       (or exactly that text, if the file starts with "=")

import { execFileSync, spawnSync } from "node:child_process";
import { readdirSync, readFileSync, existsSync, mkdirSync, writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const casesDir = join(root, "tests", "cases");
const cli = join(root, "build", "matrixlang.js");
const differential = process.argv.includes("--differential");

if (!existsSync(cli)) {
    console.error("build/matrixlang.js not found - run `make cli` first");
    process.exit(1);
}

function compile(file, ...flags) {
    const r = spawnSync("node", [cli, file, ...flags], { encoding: "utf8" });
    return { code: r.status, stdout: r.stdout, stderr: r.stderr };
}

const names = readdirSync(casesDir).filter(f => f.endsWith(".mx")).map(f => f.slice(0, -3)).sort();
let passed = 0;
const failures = [];

for (const name of names) {
    const file = join(casesDir, name + ".mx");
    const outFile = join(casesDir, name + ".out");
    const errFile = join(casesDir, name + ".err");

    if (existsSync(outFile)) {
        const expected = readFileSync(outFile, "utf8");
        const r = compile(file, "--run");
        if (r.code !== 0 || r.stdout !== expected) {
            failures.push(`${name}: expected\n${expected}got (exit ${r.code})\n${r.stdout}${r.stderr}`);
            continue;
        }
        if (differential) {
            // Generated C++ must agree with the interpreter byte for byte
            const tmp = join(root, "build", "difftest");
            mkdirSync(tmp, { recursive: true });
            const cpp = compile(file, "-o", "-").stdout;
            writeFileSync(join(tmp, name + ".cpp"), cpp);
            const exe = join(tmp, name + ".js");
            try {
                execFileSync("em++", ["-std=c++17", "-O1", join(tmp, name + ".cpp"), "-o", exe],
                             { stdio: "pipe", shell: process.platform === "win32" });
            } catch (e) {
                failures.push(`${name}: generated C++ failed to compile\n${e.stderr}`);
                continue;
            }
            const native = spawnSync("node", [exe], { encoding: "utf8" }).stdout;
            if (native !== expected) {
                failures.push(`${name}: C++ backend printed\n${native}but interpreter printed\n${expected}`);
                continue;
            }
        }
        passed++;
    } else if (existsSync(errFile)) {
        // A .err file starting with "=" must match stderr exactly (checks no extra errors)
        const raw = readFileSync(errFile, "utf8");
        const exact = raw.startsWith("=");
        const expected = exact ? raw.slice(1) : raw.trim();
        const r = compile(file, "--run");
        const stderr = r.stderr.replace(/\r\n/g, "\n");
        if (r.code === 0 || (exact ? stderr !== expected : !stderr.includes(expected))) {
            failures.push(`${name}: expected an error containing "${expected}", got (exit ${r.code})\n${r.stderr}`);
            continue;
        }
        passed++;
    }
}

for (const f of failures) console.log("FAIL " + f + "\n");
console.log(`${passed}/${names.length} tests passed` + (differential ? " (interpreter + C++ backend)" : ""));
process.exit(failures.length ? 1 : 0);
