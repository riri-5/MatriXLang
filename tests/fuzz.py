# MatriXLang fuzzer (random testing, inspired by Csmith)
#
# idea: we don't need to know the answer for a program in advance if we can
# compute it in more than one independent way and check they agree.
#
# for every random program we get 3 answers:
#   1. MatriXLang interpreter      (node build/matrixlang.js prog.mx --run)
#   2. MatriXLang C++ backend      (generate C++, compile with clang, run)
#   3. NumPy                       (the same program translated to Python, our trusted reference)
#
#   interpreter vs C++    -> must match exactly (same machine, same float operations)
#   interpreter vs NumPy  -> must match within a small tolerance (print() only shows 6 digits,
#                            and NumPy may add numbers in a different order)
#
# we also throw randomly broken programs at the compiler. it must never crash:
# it either runs the program or rejects it with a normal error message.
#
# how to run:
#   python tests/fuzz.py --programs 300 --mutants 500 --seed 7
#
# failing programs are saved in build/fuzz/failures/ and a report is written to
# build/fuzz_report.md

import argparse
import math
import os
import random
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "build", "matrixlang.js")
WORK_DIR = os.path.join(ROOT, "build", "fuzz")
REPORT_PATH = os.path.join(ROOT, "build", "fuzz_report.md")
IS_WINDOWS = os.name == "nt"


# ------------------------------------------------------------------
# random program generator
#
# every function returns TWO strings: the MatriXLang code and the same
# thing written as Python/NumPy. that way the reference answer is built
# completely separately from our compiler.
# ------------------------------------------------------------------

class ProgramGenerator:

    def __init__(self, rng):
        self.rng = rng
        self.shapes = {}  # variable name -> (rows, cols)

    def number(self):
        r = self.rng.random()
        if r < 0.6:
            text = str(self.rng.randint(-9, 9))
        elif r < 0.85:
            text = str(self.rng.choice([0.5, 0.25, 1.5, 2.5, 0.125]))
        else:
            # long and tiny decimals, these test that literals survive code generation
            text = repr(self.rng.choice([
                round(self.rng.uniform(-10, 10), self.rng.randint(5, 15)),
                self.rng.uniform(1e-12, 1e-6),
            ]))
        return text, text

    def literal(self, rows, cols):
        mx_rows, py_rows = [], []
        for _ in range(rows):
            nums = [self.number() for _ in range(cols)]
            mx_rows.append("[" + ",".join(n[0] for n in nums) + "]")
            py_rows.append("[" + ",".join(n[1] for n in nums) + "]")
        return ("[" + ",".join(mx_rows) + "]",
                "np.array([" + ",".join(py_rows) + "], dtype=float)")

    def scalar(self, depth):
        if depth <= 0 or self.rng.random() < 0.6:
            mx, py = self.number()
            if mx.startswith("-"):
                mx, py = "(" + mx + ")", "(" + py + ")"
            return mx, py
        op = self.rng.choice(["+", "-", "*"])
        a, b = self.scalar(depth - 1), self.scalar(depth - 1)
        return "(" + a[0] + " " + op + " " + b[0] + ")", "(" + a[1] + " " + op + " " + b[1] + ")"

    def nonzero_scalar(self):
        s = self.rng.choice(["2", "4", "0.5", "8", "(-2)"])
        return s, s

    def matrix(self, rows, cols, depth):
        # random expression whose result is a rows x cols matrix
        options = ["literal"]
        same_shape = [name for name, shape in self.shapes.items() if shape == (rows, cols)]
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
            name = self.rng.choice(same_shape)
            return name, "env['" + name + "']"

        if choice in ("add", "sub"):
            op = "+" if choice == "add" else "-"
            a, b = self.matrix(rows, cols, d), self.matrix(rows, cols, d)
            return "(" + a[0] + " " + op + " " + b[0] + ")", "(" + a[1] + " " + op + " " + b[1] + ")"

        if choice == "scale":
            s, m = self.scalar(1), self.matrix(rows, cols, d)
            if self.rng.random() < 0.5:
                return "(" + s[0] + " * " + m[0] + ")", "(" + s[1] + " * " + m[1] + ")"
            return "(" + m[0] + " * " + s[0] + ")", "(" + m[1] + " * " + s[1] + ")"

        if choice == "div":
            m, s = self.matrix(rows, cols, d), self.nonzero_scalar()
            return "(" + m[0] + " / " + s[0] + ")", "(" + m[1] + " / " + s[1] + ")"

        if choice == "matmul":
            inner = self.rng.randint(1, 4)
            a, b = self.matrix(rows, inner, d), self.matrix(inner, cols, d)
            return "(" + a[0] + " * " + b[0] + ")", "(" + a[1] + " @ " + b[1] + ")"

        if choice == "transpose":
            m = self.matrix(cols, rows, d)
            return "transpose(" + m[0] + ")", "(" + m[1] + ").T"

        if choice == "negate":
            m = self.matrix(rows, cols, d)
            return "-" + m[0], "(-" + m[1] + ")"

        if choice == "broadcast":
            op = self.rng.choice(["+", "-"])
            m, s = self.matrix(rows, cols, d), self.scalar(1)
            if self.rng.random() < 0.5:
                return "(" + m[0] + " " + op + " " + s[0] + ")", "(" + m[1] + " " + op + " " + s[1] + ")"
            return "(" + s[0] + " " + op + " " + m[0] + ")", "(" + s[1] + " " + op + " " + m[1] + ")"

        # builtin
        if rows == cols and self.rng.random() < 0.4:
            return "identity(%d)" % rows, "np.eye(%d)" % rows
        fn = self.rng.choice(["zeros", "ones"])
        return "%s(%d,%d)" % (fn, rows, cols), "np.%s((%d,%d))" % (fn, rows, cols)

    def program(self):
        mx_lines = []
        py_lines = []  # python statements; print() becomes out.append(...)

        for _ in range(self.rng.randint(2, 7)):
            r = self.rng.random()
            if r < 0.45 or not self.shapes:
                name = "m" + str(len(self.shapes))
                shape = (self.rng.randint(1, 4), self.rng.randint(1, 4))
                mx, py = self.matrix(shape[0], shape[1], 3)
                mx_lines.append("matrix " + name + " = " + mx + ";")
                py_lines.append("env['" + name + "'] = " + py)
                self.shapes[name] = shape
            elif r < 0.65:
                name = self.rng.choice(list(self.shapes))
                mx, py = self.matrix(self.shapes[name][0], self.shapes[name][1], 3)
                mx_lines.append(name + " = " + mx + ";")
                py_lines.append("env['" + name + "'] = " + py)
            elif r < 0.75:
                mx, py = self.scalar(2)
                mx_lines.append("print(" + mx + ");")
                py_lines.append("out.append(" + py + ")")
            else:
                shape = (self.rng.randint(1, 4), self.rng.randint(1, 4))
                mx, py = self.matrix(shape[0], shape[1], 3)
                mx_lines.append("print(" + mx + ");")
                py_lines.append("out.append(" + py + ")")

        for name in self.shapes:
            mx_lines.append("print(" + name + ");")
            py_lines.append("out.append(env['" + name + "'])")

        return "\n".join(mx_lines) + "\n", "\n".join(py_lines) + "\n"


def mutate(source, rng):
    # damage a valid program a little: delete, insert or replace random tokens
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


# ------------------------------------------------------------------
# NumPy reference
# ------------------------------------------------------------------

def numpy_answer(py_code):
    # runs the python version of the program and returns the printed values
    env, out = {}, []
    exec(py_code, {"np": np, "env": env, "out": out})
    rows = []
    for value in out:
        value = np.atleast_2d(np.asarray(value, dtype=float))
        for row in value:
            rows.append([float(x) for x in row])
    return rows


def close_enough(printed_rows, reference_rows):
    if len(printed_rows) != len(reference_rows):
        return False
    for printed, reference in zip(printed_rows, reference_rows):
        if len(printed) != len(reference):
            return False
        scale = max([1.0] + [abs(x) for x in reference])
        for a, b in zip(printed, reference):
            # print() shows 6 significant digits, so allow ~1e-5 relative error,
            # plus a little absolute room for values that cancel out to almost zero
            if not math.isclose(a, b, rel_tol=1e-5, abs_tol=1e-9 * scale):
                return False
    return True


# ------------------------------------------------------------------
# running the compiler
# ------------------------------------------------------------------

def run(cmd, timeout=120):
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout,
                            shell=IS_WINDOWS and cmd[0] == "em++")
    result.stdout = result.stdout.replace("\r\n", "\n")
    return result


def pick_cpp_compiler():
    # on windows the old MinGW runtime prints 1e-006 instead of 1e-06,
    # so there we always use clang from Emscripten to compare fairly
    if IS_WINDOWS and shutil.which("em++"):
        return "em++"
    for cxx in ("clang++", "g++", "em++"):
        if shutil.which(cxx):
            return cxx
    sys.exit("need a C++ compiler (clang++, g++ or em++)")


def check_program(index, mx_code, py_code, cxx):
    base = os.path.join(WORK_DIR, "p%04d" % index)
    with open(base + ".mx", "w", newline="\n") as f:
        f.write(mx_code)

    # answer 1: interpreter
    interp = run(["node", CLI, base + ".mx", "--run"])
    if interp.returncode != 0:
        return "interpreter rejected a valid program\n" + interp.stderr

    # answer 2: generated C++
    gen = run(["node", CLI, base + ".mx", "-o", base + ".cpp"])
    if gen.returncode != 0:
        return "C++ generation failed\n" + gen.stderr
    if cxx == "em++":
        build = run(["em++", "-std=c++17", "-O1", base + ".cpp", "-o", base + ".js"])
        run_cmd = ["node", base + ".js"]
    else:
        build = run([cxx, "-std=c++17", "-O1", base + ".cpp", "-o", base + ".exe"])
        run_cmd = [base + ".exe"]
    if build.returncode != 0:
        return "generated C++ does not compile\n" + build.stderr[:2000]
    native = run(run_cmd)
    if native.stdout != interp.stdout:
        return ("interpreter and C++ backend disagree\n--- interpreter ---\n" + interp.stdout +
                "--- C++ ---\n" + native.stdout)

    # answer 3: NumPy
    printed = [[float(x) for x in line.split()] for line in interp.stdout.splitlines()]
    reference = numpy_answer(py_code)
    if not close_enough(printed, reference):
        shown = "\n".join(" ".join("%g" % x for x in row) for row in reference)
        return ("compiler and NumPy disagree\n--- compiler ---\n" + interp.stdout +
                "--- numpy ---\n" + shown + "\n")

    return None


def check_mutant(index, source):
    path = os.path.join(WORK_DIR, "mutant%04d.mx" % index)
    with open(path, "w", newline="\n") as f:
        f.write(source)
    result = run(["node", CLI, path, "--run"])
    if result.returncode == 0:
        return None  # the damage happened to still be a valid program, that's fine
    if result.returncode == 1 and "error" in result.stderr.lower() and "Aborted" not in result.stderr:
        return None  # clean error message, exactly what we want
    return "compiler crashed (exit code %s)\n%s" % (result.returncode, result.stderr[-1500:])


# ------------------------------------------------------------------

def write_report(args, cxx, programs, program_errors, mutants, mutant_errors, seconds):
    bad_programs = [(p, e) for p, e in zip(programs, program_errors) if e]
    bad_mutants = [(m, e) for m, e in zip(mutants, mutant_errors) if e]

    lines = [
        "# MatriXLang fuzz report",
        "",
        "- seed: %d, C++ compiler: %s, time: %.0f s" % (args.seed, cxx, seconds),
        "- random valid programs: **%d / %d** agree (interpreter = C++ backend = NumPy)"
        % (len(programs) - len(bad_programs), len(programs)),
        "- random broken programs: **%d / %d** handled without a crash"
        % (len(mutants) - len(bad_mutants), len(mutants)),
    ]
    for (source, error) in bad_programs + bad_mutants:
        lines += ["", "## " + error.splitlines()[0], "```", source.strip(), "```",
                  "```", error.strip(), "```"]

    with open(REPORT_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description="random differential testing for MatriXLang")
    parser.add_argument("--programs", type=int, default=200, help="how many random valid programs")
    parser.add_argument("--mutants", type=int, default=300, help="how many random broken programs")
    parser.add_argument("--seed", type=int, default=1, help="same seed = same programs (reproducible)")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 4, help="programs tested in parallel")
    args = parser.parse_args()

    if not os.path.exists(CLI):
        sys.exit("build/matrixlang.js not found, run `make cli` first")
    os.makedirs(os.path.join(WORK_DIR, "failures"), exist_ok=True)

    rng = random.Random(args.seed)
    programs = [ProgramGenerator(random.Random(rng.random())).program() for _ in range(args.programs)]
    mutants = [mutate(rng.choice(programs)[0], rng) for _ in range(args.mutants)]
    cxx = pick_cpp_compiler()
    print("seed %d: %d programs, %d broken programs, C++ via %s, %d parallel jobs"
          % (args.seed, len(programs), len(mutants), cxx, args.jobs))

    start = time.time()
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        program_errors = list(pool.map(lambda job: check_program(job[0], job[1][0], job[1][1], cxx),
                                       enumerate(programs)))
        mutant_errors = list(pool.map(lambda job: check_mutant(job[0], job[1]), enumerate(mutants)))
    seconds = time.time() - start

    # save every failing program so it can become a regression test
    failures = [(p[0], e) for p, e in zip(programs, program_errors) if e]
    failures += [(m, e) for m, e in zip(mutants, mutant_errors) if e]
    for n, (source, error) in enumerate(failures):
        with open(os.path.join(WORK_DIR, "failures", "fail_%03d.mx" % n), "w", newline="\n") as f:
            f.write(source)
        if n < 2:
            print("\nFAIL\n" + source.strip() + "\n" + error.strip())

    write_report(args, cxx, [p[0] for p in programs], program_errors, mutants, mutant_errors, seconds)
    print("\n%d/%d programs agree (interpreter = C++ = NumPy), %d/%d broken programs handled without a crash"
          % (program_errors.count(None), len(programs), mutant_errors.count(None), len(mutants)))
    print("report: build/fuzz_report.md")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
