# MatriXLang test runner
#
# how to run:
#   python tests/run_tests.py                  -> run every test on the interpreter
#   python tests/run_tests.py --differential   -> also compile every valid program to C++,
#                                                build + run it, and check both outputs match
#
# how tests are written (tests/cases/):
#   name.mx + name.out  -> program must print exactly what is in name.out
#   name.mx + name.err  -> compiler must fail, and the error must contain this text
#                          (if the .err file starts with "=" the error must match exactly)
#
# at the end we write a report to build/test_report.md and exit with 1 if anything failed,
# so this can be used in CI

import os
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES_DIR = os.path.join(ROOT, "tests", "cases")
CLI = os.path.join(ROOT, "build", "matrixlang.js")
BUILD_DIR = os.path.join(ROOT, "build", "difftest")
REPORT_PATH = os.path.join(ROOT, "build", "test_report.md")


def run(cmd):
    # em++ is a .bat file on windows, so it needs shell=True there
    use_shell = os.name == "nt" and cmd[0] == "em++"
    result = subprocess.run(cmd, capture_output=True, text=True, shell=use_shell)
    # normalize windows line endings so comparisons are fair
    result.stdout = result.stdout.replace("\r\n", "\n")
    result.stderr = result.stderr.replace("\r\n", "\n")
    return result


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read().replace("\r\n", "\n")


def check_valid_program(name, mx_file, out_file, differential):
    expected = read(out_file)

    # step 1: run it on the interpreter
    result = run(["node", CLI, mx_file, "--run"])
    if result.returncode != 0:
        return "compiler rejected it:\n" + result.stderr
    if result.stdout != expected:
        return "wrong output\nexpected:\n" + expected + "got:\n" + result.stdout

    if not differential:
        return None

    # step 2: generate C++ for the same program
    cpp_file = os.path.join(BUILD_DIR, name + ".cpp")
    js_file = os.path.join(BUILD_DIR, name + ".js")
    gen = run(["node", CLI, mx_file, "-o", cpp_file])
    if gen.returncode != 0:
        return "C++ generation failed:\n" + gen.stderr

    # step 3: compile the generated C++ with clang (em++) and run it
    build = run(["em++", "-std=c++17", "-O1", cpp_file, "-o", js_file])
    if build.returncode != 0:
        return "generated C++ does not compile:\n" + build.stderr

    native = run(["node", js_file])
    if native.stdout != expected:
        return "C++ backend gives a different answer\ninterpreter:\n" + expected + "C++:\n" + native.stdout

    return None


def check_invalid_program(mx_file, err_file):
    raw = read(err_file)
    exact = raw.startswith("=")
    expected = raw[1:] if exact else raw.strip()

    result = run(["node", CLI, mx_file, "--run"])
    if result.returncode == 0:
        return "compiler accepted a program that should fail"

    if exact and result.stderr != expected:
        return "error message is not exactly right\nexpected:\n" + expected + "got:\n" + result.stderr
    if not exact and expected not in result.stderr:
        return "expected an error containing: " + expected + "\ngot:\n" + result.stderr

    return None


def write_report(results, differential, seconds):
    passed = [r for r in results if r["error"] is None]
    failed = [r for r in results if r["error"] is not None]

    lines = []
    lines.append("# MatriXLang test report")
    lines.append("")
    lines.append("- mode: " + ("interpreter + C++ backend (differential)" if differential else "interpreter"))
    lines.append("- result: **%d / %d passed**" % (len(passed), len(results)))
    lines.append("- time: %.1f s" % seconds)
    lines.append("")
    lines.append("| test | kind | result |")
    lines.append("|---|---|---|")
    for r in results:
        lines.append("| %s | %s | %s |" % (r["name"], r["kind"], "pass" if r["error"] is None else "**FAIL**"))

    if failed:
        lines.append("")
        lines.append("## Failures")
        for r in failed:
            lines.append("")
            lines.append("### " + r["name"])
            lines.append("```")
            lines.append(r["error"].strip())
            lines.append("```")

    os.makedirs(os.path.dirname(REPORT_PATH), exist_ok=True)
    with open(REPORT_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def main():
    differential = "--differential" in sys.argv

    if not os.path.exists(CLI):
        print("build/matrixlang.js not found, run `make cli` first")
        sys.exit(1)
    if differential and not shutil.which("em++"):
        print("--differential needs em++ (Emscripten) on your PATH")
        sys.exit(1)
    os.makedirs(BUILD_DIR, exist_ok=True)

    names = sorted(f[:-3] for f in os.listdir(CASES_DIR) if f.endswith(".mx"))
    results = []
    start = time.time()

    for name in names:
        mx_file = os.path.join(CASES_DIR, name + ".mx")
        out_file = os.path.join(CASES_DIR, name + ".out")
        err_file = os.path.join(CASES_DIR, name + ".err")

        if os.path.exists(out_file):
            kind = "valid"
            error = check_valid_program(name, mx_file, out_file, differential)
        elif os.path.exists(err_file):
            kind = "invalid"
            error = check_invalid_program(mx_file, err_file)
        else:
            kind = "?"
            error = "no .out or .err file for this test"

        results.append({"name": name, "kind": kind, "error": error})
        print(("  ok    " if error is None else "  FAIL  ") + name)

    seconds = time.time() - start
    write_report(results, differential, seconds)

    failed = [r for r in results if r["error"] is not None]
    for r in failed:
        print("\nFAIL " + r["name"] + "\n" + r["error"])

    print("\n%d/%d tests passed%s  (report: build/test_report.md)"
          % (len(results) - len(failed), len(results),
             " (interpreter + C++ backend)" if differential else ""))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
