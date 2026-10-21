#!/usr/bin/env python3
from __future__ import annotations

import argparse
import difflib
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parent
SRC_DIR = REPO_ROOT / "src"
SERVER_BIN = SRC_DIR / "server"
OUTPUTS_DIR = HERE / "outputs"


def _normalize_for_compare(s: str) -> str:
    # Match the common “diff -u” style used by TAs: ignore line-ending differences
    # and whether the final newline exists, but preserve all other whitespace.
    s = s.replace("\r\n", "\n").replace("\r", "\n")
    return s.rstrip("\n")


def _unified_diff(expected: str, got: str, expected_label: str, got_label: str) -> str:
    return "".join(
        difflib.unified_diff(
            expected.splitlines(True),
            got.splitlines(True),
            fromfile=expected_label,
            tofile=got_label,
        )
    )


def run_server(commands: list[str]) -> list[str]:
    proc = subprocess.run(
        [str(SERVER_BIN)],
        cwd=str(SRC_DIR),
        input="\n".join(commands + ["QUIT"]) + "\n",
        text=True,
        capture_output=True,
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"server exited with {proc.returncode}\n"
            f"--- STDERR ---\n{proc.stderr}\n"
            f"--- STDOUT ---\n{proc.stdout}\n"
        )

    # The server prints a prompt before each command: "\n> " (no trailing newline).
    # Anything printed by the command appears on the same line as the prompt.
    parts = proc.stdout.split("\n> ")
    # parts[0] is always text before the first prompt (usually empty).
    outputs = parts[1 : 1 + len(commands)]
    if len(outputs) != len(commands):
        raise RuntimeError(
            f"Failed to segment output into per-command chunks.\n"
            f"commands={len(commands)} outputs={len(outputs)} parts={len(parts)}\n"
            f"--- RAW STDOUT ---\n{proc.stdout}\n"
        )
    return outputs


@dataclass(frozen=True)
class OutputTest:
    name: str
    commands: list[str]
    check_command_index: int
    expected_file: Path


def build_tests() -> list[OutputTest]:
    out = OUTPUTS_DIR

    long_where_500 = (
        "A1(N) == 1 AND B1(E) == 0 AND A2(N) == 0 AND A1(N) == 1 AND "
        "B2(E) == 0 AND A3(N) == 0 AND B3(E) == 1 AND A4(N) == 1 AND A1(N)"
    )

    return [
        # LOAD GRAPH
        OutputTest("LOAD_GRAPH_10_D", ["LOAD GRAPH 10 D"], 1, out / "check_load10.txt"),
        OutputTest("LOAD_GRAPH_100_U", ["LOAD GRAPH 100 U"], 1, out / "check_load100.txt"),
        OutputTest("LOAD_GRAPH_500_D", ["LOAD GRAPH 500 D"], 1, out / "check_load500.txt"),
        OutputTest("LOAD_GRAPH_10000_U", ["LOAD GRAPH 10000 U"], 1, out / "check_load10000.txt"),
        OutputTest("LOAD_GRAPH_1000000_D", ["LOAD GRAPH 1000000 D"], 1, out / "check_loadbig.txt"),
        # PRINT GRAPH (requires load in same session; compare only PRINT output)
        OutputTest("PRINT_GRAPH_10", ["LOAD GRAPH 10 D", "PRINT GRAPH 10"], 2, out / "check_print10.txt"),
        OutputTest("PRINT_GRAPH_100", ["LOAD GRAPH 100 U", "PRINT GRAPH 100"], 2, out / "check_print100.txt"),
        OutputTest("PRINT_GRAPH_500", ["LOAD GRAPH 500 D", "PRINT GRAPH 500"], 2, out / "check_print500.txt"),
        OutputTest("PRINT_GRAPH_10000", ["LOAD GRAPH 10000 U", "PRINT GRAPH 10000"], 2, out / "check_print10000.txt"),
        # PATH (requires load)
        OutputTest("PATH_10", ["LOAD GRAPH 10 D", "RES_10 <- PATH 10 1 10 WHERE A1(N) == 1"], 2, out / "check_path10.txt"),
        OutputTest(
            "PATH_100",
            ["LOAD GRAPH 100 U", "RES_100 <- PATH 100 1 100 WHERE A49(N) == 1 AND ANY(E)"],
            2,
            out / "check_path100.txt",
        ),
        OutputTest(
            "PATH_500",
            [f"LOAD GRAPH 500 D", f"RES_G_500_D <- PATH 500 19 467 WHERE {long_where_500}"],
            2,
            out / "check_path500.txt",
        ),
        OutputTest(
            "PATH_10000",
            ["LOAD GRAPH 10000 U", "RES_G_10000_U <- PATH 10000 1 407 WHERE B2(E)"],
            2,
            out / "check_path10000.txt",
        ),
        OutputTest(
            "PATH_1000000",
            ["LOAD GRAPH 1000000 D", "RES_1000000 <- PATH 1000000 450939 1000000 WHERE ANY(E) == 1"],
            2,
            out / "check_pathbig.txt",
        ),
        # DEGREE (requires load)
        OutputTest("DEGREE_10", ["LOAD GRAPH 10 D", "DEGREE 10 6"], 2, out / "check_degree10.txt"),
        OutputTest("DEGREE_100", ["LOAD GRAPH 100 U", "DEGREE 100 67"], 2, out / "check_degree100.txt"),
        OutputTest("DEGREE_500", ["LOAD GRAPH 500 D", "DEGREE 500 248"], 2, out / "check_degree500.txt"),
        OutputTest("DEGREE_10000", ["LOAD GRAPH 10000 U", "DEGREE 10000 9510"], 2, out / "check_degree10000.txt"),
        OutputTest("DEGREE_1000000", ["LOAD GRAPH 1000000 D", "DEGREE 1000000 252414"], 2, out / "check_degreebig.txt"),
        # Mixed test case 1 (compare only the final PRINT output)
        OutputTest(
            "MIXED_1_PRINT_RES",
            [
                "LOAD GRAPH 500 D",
                f"Res <- PATH 500 19 467 WHERE {long_where_500}",
                "Res <- PATH 500 1 3",
                "PRINT GRAPH Res",
            ],
            4,
            out / "check_print500_mixed.txt",
        ),
        # Mixed test case 2 (compare only the final DEGREE output)
        OutputTest(
            "MIXED_2_DEGREE_RES",
            [
                "LOAD GRAPH 100 U",
                "Res <- PATH 100 1 100 WHERE A49(N) == 1 AND B201(E) == 0",
                "PRINT GRAPH Res",
                "DEGREE Res 67",
            ],
            4,
            out / "check_degree_res.txt",
        ),
        # Mixed test case 3 (compare only the final LOAD GRAPH output)
        OutputTest(
            "MIXED_3_EXISTING_GRAPH_ERROR",
            ["LOAD GRAPH 10 D", "LOAD 10", "PRINT 10", "LOAD GRAPH 10 D"],
            4,
            out / "check_errorexistingload.txt",
        ),
        # Error handling (single-command checks)
        OutputTest("ERROR_LOAD", ["LOAD GRAPH DataSystems U"], 1, out / "error_load.txt"),
        OutputTest("ERROR_PRINT", ["PRINT GRAPH DataSystems"], 1, out / "error_print.txt"),
        OutputTest("ERROR_EXPORT", ["EXPORT GRAPH DS"], 1, out / "error_export.txt"),
        OutputTest("ERROR_PATH_NODE_MISSING", ["LOAD GRAPH 10 D", "Res <- PATH 10 100 1"], 2, out / "error_path.txt"),
        OutputTest("ERROR_DEGREE_GRAPH_MISSING", ["DEGREE Res 12"], 1, out / "error_degree.txt"),
        # Table smoke tests (present in this folder and used by Mixed 3 setup)
        OutputTest("LOAD_TABLE_10", ["LOAD 10"], 1, out / "check_loadtable10.txt"),
        OutputTest("PRINT_TABLE_10", ["LOAD 10", "PRINT 10"], 2, out / "check_printtable10.txt"),
    ]


def main() -> int:
    ap = argparse.ArgumentParser(description="Run SimpleRA Phase 1 PDF-aligned test cases against src/server output.")
    ap.add_argument("--pattern", help="Run only tests whose name contains this substring.")
    ap.add_argument("--build", action="store_true", help="Build server before running tests (make -C src).")
    ap.add_argument("--relaxed", action="store_true", help="Ignore trailing whitespace differences per line.")
    args = ap.parse_args()

    if args.build:
        build = subprocess.run(["make", "-C", str(SRC_DIR)], text=True)
        if build.returncode != 0:
            return build.returncode

    if not SERVER_BIN.exists():
        print(f"Missing {SERVER_BIN}. Build first: cd src && make", file=sys.stderr)
        return 2

    tests = build_tests()
    if args.pattern:
        tests = [t for t in tests if args.pattern in t.name]
    if not tests:
        print("No tests selected.", file=sys.stderr)
        return 2

    failed = 0
    for t in tests:
        outputs = run_server(t.commands)
        got_raw = outputs[t.check_command_index - 1]
        expected_raw = t.expected_file.read_text(encoding="utf-8")

        got = _normalize_for_compare(got_raw)
        expected = _normalize_for_compare(expected_raw)

        if args.relaxed:
            got = "\n".join([ln.rstrip() for ln in got.split("\n")])
            expected = "\n".join([ln.rstrip() for ln in expected.split("\n")])

        if got != expected:
            failed += 1
            diff = _unified_diff(
                expected + ("\n" if expected and not expected.endswith("\n") else ""),
                got + ("\n" if got and not got.endswith("\n") else ""),
                str(t.expected_file),
                f"{t.name} (got)",
            )
            print(f"[FAIL] {t.name}\n{diff}")
        else:
            print(f"[OK]   {t.name}")

    if failed:
        print(f"\nFAILED: {failed}/{len(tests)} tests")
        return 1
    print(f"\nPASSED: {len(tests)}/{len(tests)} tests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

