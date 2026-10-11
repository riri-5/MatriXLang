#!/usr/bin/env python3
"""Differential fuzzer for MatriXLang (in the spirit of Csmith).

Generates random, well-typed MatriXLang programs, runs each one through both
compiler backends, and reports any program where they disagree:

  1. interpreter:    node build/matrixlang.js prog.mx --run
  2. C++ backend:    node build/matrixlang.js prog.mx -o prog.cpp, compile, run

It also checks that the compiler never crashes on randomly mutated (usually
invalid) programs: it must either run them or reject them with a clean error.

    python tests/fuzz.py --programs 300 --seed 1 --jobs 8

Any failing program is saved to build/fuzz/failures/ so it can be turned into
a regression test.
"""

import argparse
import os
import random
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "build", "matrixlang.js")
WORK = os.path.join(ROOT, "build", "fuzz")
IS_WINDOWS = os.name == "nt"


# ---------------------------------------------------------------- generator

class ProgramGenerator:
    """Builds a random program that the semantic analyzer must accept."""

    def __init__(self, rng):
        self.rng = rng
        self.vars = {}  # name -> (rows, cols)

    def number(self):
        r = self.rng.random()
        if r < 0.6:
            return str(self.rng.randint(-9, 9))
        if r < 0.85:
            return str(self.rng.choice([0.5, 0.25, 1.5, 2.5, 0.125]))
        # long or tiny decimals stress literal round-tripping in code generation
        return repr(self.rng.choice([
            round(self.rng.uniform(-10, 10), self.rng.randint(5, 15)),
            self.rng.uniform(1e-12, 1e-6),
        ]))

    def literal(self, rows, cols):
        body = ",".join(
            "[" + ",".join(self.number() for _ in range(cols)) + "]" for _ in range(rows)
        )
        return "[" + body + "]"

    def scalar(self, depth):
        if depth <= 0 or self.rng.random() < 0.6:
            n = self.number()
            return n if not n.startswith("-") else "(" + n + ")"
        op = self.rng.choice(["+", "-", "*"])
        return "(" + self.scalar(depth - 1) + " " + op + " " + self.scalar(depth - 1) + ")"

    def nonzero_scalar(self):
        return self.rng.choice(["2", "4", "0.5", "8", "(-2)"])

    def matrix(self, rows, cols, depth):
        """Random expression whose type is a rows x cols matrix."""
        options = ["literal"]
        same_shape = [v for v, s in self.vars.items() if s == (rows, cols)]
        if same_shape:
            options += ["var", "var"]
        if depth > 0:
            options += ["add", "sub", "scale", "div", "matmul", "matmul",
                        "transpose", "negate", "broadcast", "builtin"]
        choice = self.rng.choice(options)
        d = depth - 1

        if choice == "literal":
            return self.literal(rows, cols)
        if choice == "var":
            return self.rng.choice(same_shape)
        if choice in ("add", "sub"):
            op = "+" if choice == "add" else "-"
            return "(" + self.matrix(rows, cols, d) + " " + op + " " + self.matrix(rows, cols, d) + ")"
        if choice == "scale":
            if self.rng.random() < 0.5:
                return "(" + self.scalar(1) + " * " + self.matrix(rows, cols, d) + ")"
            return "(" + self.matrix(rows, cols, d) + " * " + self.scalar(1) + ")"
        if choice == "div":
            return "(" + self.matrix(rows, cols, d) + " / " + self.nonzero_scalar() + ")"
        if choice == "matmul":
            inner = self.rng.randint(1, 4)
            return "(" + self.matrix(rows, inner, d) + " * " + self.matrix(inner, cols, d) + ")"
        if choice == "transpose":
            return "transpose(" + self.matrix(cols, rows, d) + ")"
        if choice == "negate":
            return "-" + self.matrix(rows, cols, d)
        if choice == "broadcast":
            op = self.rng.choice(["+", "-"])
            if self.rng.random() < 0.5:
                return "(" + self.matrix(rows, cols, d) + " " + op + " " + self.scalar(1) + ")"
            return "(" + self.scalar(1) + " " + op + " " + self.matrix(rows, cols, d) + ")"
        # builtin
        if rows == cols and self.rng.random() < 0.4:
            return "identity(" + str(rows) + ")"
        return self.rng.choice(["zeros", "ones"]) + "(" + str(rows) + "," + str(cols) + ")"

    def program(self):
        lines = []
        for i in range(self.rng.randint(2, 7)):
            r = self.rng.random()
            if r < 0.45 or not self.vars:
                name = "m" + str(len(self.vars))
                shape = (self.rng.randint(1, 4), self.rng.randint(1, 4))
                lines.append("matrix " + name + " = " + self.matrix(*shape, depth=3) + ";")
                self.vars[name] = shape
            elif r < 0.65:
                name = self.rng.choice(list(self.vars))
                lines.append(name + " = " + self.matrix(*self.vars[name], depth=3) + ";")
            elif r < 0.75:
                lines.append("print(" + self.scalar(2) + ");")
            else:
                shape = (self.rng.randint(1, 4), self.rng.randint(1, 4))
                lines.append("print(" + self.matrix(*shape, depth=3) + ");")
        for name in self.vars:
            lines.append("print(" + name + ");")
        return "\n".join(lines) + "\n"


def mutate(source, rng):
    """Random token-level damage: the result is usually an invalid program."""
    tokens = source.replace("(", " ( ").replace(")", " ) ").replace(";", " ; ").split()
    for _ in range(rng.randint(1, 3)):
        i = rng.randrange(len(tokens))
        action = rng.random()
        if action < 0.35:
            del tokens[i]
        elif action < 0.7:
            tokens.insert(i, rng.choice(["*", "+", "[", "]", "(", ")", ",", ";", "transpose",
                                         "matrix", "m0", "1", "@", "ones(0,2)", "zeros(2)"]))
        else:
            tokens[i] = rng.choice(["m9", "[[1,2],[3]]", "[[]]", "/ m0", "identity(5000)"])
    return " ".join(tokens)


# ---------------------------------------------------------------- execution

def run(cmd, timeout=60):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout,
                          shell=IS_WINDOWS and cmd[0] == "em++")


def find_cxx():
    """Prefer a native C++ compiler for speed, fall back to Emscripten.
    On Windows, MinGW's C runtime prints exponents as e-006 instead of e-06,
    so use Emscripten's Clang there to compare like with like."""
    if IS_WINDOWS and shutil.which("em++"):
        return "em++"
    for cxx in ("clang++", "g++"):
        if shutil.which(cxx):
            probe = os.path.join(WORK, "probe.cpp")
            with open(probe, "w") as f:
                f.write("#include <type_traits>\nint main() { return 0; }\n")
            exe = os.path.join(WORK, "probe.exe")
            if run([cxx, "-std=c++17", probe, "-o", exe]).returncode == 0:
                return cxx
    if shutil.which("em++"):
        return "em++"
    sys.exit("no working C++ compiler found (need clang++, g++ or em++)")


def check_program(index, source, cxx):
    """Returns None if both backends agree, otherwise a description of the failure."""
    base = os.path.join(WORK, "p%04d" % index)
    with open(base + ".mx", "w", newline="\n") as f:
        f.write(source)

    interp = run(["node", CLI, base + ".mx", "--run"])
    if interp.returncode != 0:
        return "interpreter rejected a valid program:\n" + interp.stderr

    gen = run(["node", CLI, base + ".mx", "-o", base + ".cpp"])
    if gen.returncode != 0:
        return "C++ generation failed:\n" + gen.stderr

    if cxx == "em++":
        exe = base + ".js"
        build = run(["em++", "-std=c++17", "-O1", base + ".cpp", "-o", exe])
        native_cmd = ["node", exe]
    else:
        exe = base + ".exe"
        build = run([cxx, "-std=c++17", "-O1", base + ".cpp", "-o", exe])
        native_cmd = [exe]
    if build.returncode != 0:
        return "generated C++ does not compile:\n" + build.stderr[:2000]

    native = run(native_cmd)
    if native.stdout.replace("\r\n", "\n") != interp.stdout.replace("\r\n", "\n"):
        return ("backends disagree\n--- interpreter ---\n" + interp.stdout +
                "--- generated C++ ---\n" + native.stdout)
    return None


def check_mutant(source):
    """The compiler must never crash: exit code 0 (ran) or 1 (clean error), with a message."""
    path = os.path.join(WORK, "mutant_%d.mx" % (abs(hash(source)) % 10**8))
    with open(path, "w", newline="\n") as f:
        f.write(source)
    r = run(["node", CLI, path, "--run"])
    if r.returncode == 0:
        return None
    if r.returncode == 1 and "error" in r.stderr.lower() and "Aborted" not in r.stderr:
        return None
    return "compiler crashed (exit %s):\n%s" % (r.returncode, r.stderr[-1500:])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--programs", type=int, default=200, help="random valid programs to test")
    ap.add_argument("--mutants", type=int, default=300, help="random invalid programs to test")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    args = ap.parse_args()

    if not os.path.exists(CLI):
        sys.exit("build/matrixlang.js not found - run `make cli` first")
    os.makedirs(os.path.join(WORK, "failures"), exist_ok=True)

    rng = random.Random(args.seed)
    programs = [ProgramGenerator(random.Random(rng.random())).program() for _ in range(args.programs)]
    mutants = [mutate(rng.choice(programs), rng) for _ in range(args.mutants)]
    cxx = find_cxx()
    print("seed %d: %d programs (C++ via %s), %d mutants, %d jobs"
          % (args.seed, len(programs), cxx, len(mutants), args.jobs))

    failures = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = pool.map(lambda p: check_program(p[0], p[1], cxx), enumerate(programs))
        for i, (src, err) in enumerate(zip(programs, results)):
            if err:
                failures.append((src, err))
        crash_results = pool.map(check_mutant, mutants)
        for src, err in zip(mutants, crash_results):
            if err:
                failures.append((src, err))

    kinds = {}
    for _, err in failures:
        kind = err.splitlines()[0]
        kinds[kind] = kinds.get(kind, 0) + 1
    for kind, count in sorted(kinds.items(), key=lambda kv: -kv[1]):
        print("  %3d x %s" % (count, kind))

    for n, (src, err) in enumerate(failures):
        path = os.path.join(WORK, "failures", "fail_%03d.mx" % n)
        with open(path, "w", newline="\n") as f:
            f.write(src)
        if n < 2:
            print("\nFAIL (%s)\n%s\n%s" % (path, src.strip(), err.strip()))

    print("\n%d/%d valid programs agree across backends, %d/%d mutants handled without a crash"
          % (len(programs) - sum(1 for s, _ in failures if s in programs), len(programs),
             len(mutants) - sum(1 for s, _ in failures if s in mutants), len(mutants)))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
