#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$ROOT_DIR/src"
INPUT_DIR="$ROOT_DIR/Phase 2 Testing/inputs"
OUTPUT_DIR="$ROOT_DIR/Phase 2 Testing/outputs"
SERVER_BIN="$SRC_DIR/server"

if [[ ! -d "$INPUT_DIR" || ! -d "$OUTPUT_DIR" ]]; then
  echo "Missing Phase 2 Testing inputs/outputs folders."
  exit 2
fi

echo "Building src/server..."
make -C "$SRC_DIR" -j4

echo "Staging Phase 2 CSV inputs into data/ ..."
cp -f "$INPUT_DIR"/*.csv "$ROOT_DIR/data/"

echo "Running Phase 2 checks..."
python3 - "$ROOT_DIR" "$SRC_DIR" "$OUTPUT_DIR" <<'PY'
from __future__ import annotations

import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(sys.argv[1]).resolve()
SRC = Path(sys.argv[2]).resolve()
OUT = Path(sys.argv[3]).resolve()
SERVER = SRC / "server"


@dataclass(frozen=True)
class Check:
    name: str
    index: int
    expected: Path


@dataclass(frozen=True)
class Case:
    name: str
    commands: list[str]
    checks: list[Check]


def run_server(commands: list[str]) -> list[str]:
    proc = subprocess.run(
        [str(SERVER)],
        cwd=str(SRC),
        input="\n".join(commands + ["QUIT"]) + "\n",
        text=True,
        capture_output=True,
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"server exited with {proc.returncode}\n"
            f"--- STDERR ---\n{proc.stderr}\n"
            f"--- STDOUT ---\n{proc.stdout[:1500]}\n"
        )

    # server prompt appears as "\n> " before each command.
    parts = proc.stdout.split("\n> ")
    outputs = parts[1 : 1 + len(commands)]
    if len(outputs) != len(commands):
        raise RuntimeError(
            f"failed to split output per command: commands={len(commands)} outputs={len(outputs)}"
        )
    return outputs


def normalize_common(text: str) -> str:
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    text = text.replace("’", "'").replace("‘", "'")
    return text.rstrip("\n")


def normalize_csv(text: str) -> str:
    text = normalize_common(text)
    lines = []
    for line in text.split("\n"):
        line = line.rstrip()
        # Avoid false failures due to "a, b, c" vs "a,b,c".
        line = re.sub(r"\s*,\s*", ",", line)
        lines.append(line)
    return "\n".join(lines)


def first_diff(expected: str, got: str) -> tuple[int, str, str]:
    exp_lines = expected.splitlines()
    got_lines = got.splitlines()
    upto = min(len(exp_lines), len(got_lines))
    for i in range(upto):
        if exp_lines[i] != got_lines[i]:
            return (i + 1, exp_lines[i], got_lines[i])
    if len(exp_lines) != len(got_lines):
        i = upto + 1
        e = exp_lines[upto] if len(exp_lines) > upto else "<no line>"
        g = got_lines[upto] if len(got_lines) > upto else "<no line>"
        return (i, e, g)
    return (0, "", "")


cases: list[Case] = [
    Case(
        "SORT_10",
        ["LOAD 10_5", "SORT 10_5 BY C IN ASC", "PRINT 10_5"],
        [Check("SORT_10", 3, OUT / "S_10.csv")],
    ),
    Case(
        "SORT_500",
        [
            "LOAD 500_20",
            "SORT 500_20 BY C, P, S IN DESC, ASC, DESC TOP 100",
            "PRINT 500_20",
        ],
        [Check("SORT_500", 3, OUT / "S_500.csv")],
    ),
    Case(
        "SORT_1000",
        [
            "LOAD 1000_2",
            "SORT 1000_2 BY A, B IN DESC, ASC TOP 500 BOTTOM 500",
            "PRINT 1000_2",
        ],
        [Check("SORT_1000", 3, OUT / "S_1000.csv")],
    ),
    Case(
        "SORT_10000",
        [
            "LOAD 10000_30",
            "SORT 10000_30 BY A, AB, AC, AD IN ASC, ASC, DESC, ASC",
            "PRINT 10000_30",
        ],
        [Check("SORT_10000", 3, OUT / "S_10000.csv")],
    ),
    Case(
        "JOIN_1",
        [
            "LOAD 10_5_new",
            "LOAD 500_20_new",
            "SETBUFFER 3",
            "Result1 <- JOIN 10_5_new, 500_20_new ON 10_5_new.A1 == 500_20_new.A2",
            "PRINT Result1",
        ],
        [Check("JOIN_1", 5, OUT / "Result1.csv")],
    ),
    Case(
        "JOIN_2",
        [
            "LOAD 1000_2_new",
            "LOAD 10000_30_new",
            "SETBUFFER 5",
            "Result2 <- JOIN 1000_2_new, 10000_30_new ON 1000_2_new.A3 + 10000_30_new.B4 == 500",
            "PRINT Result2",
        ],
        [Check("JOIN_2", 5, OUT / "Result2.csv")],
    ),
    Case(
        "JOIN_3",
        [
            "LOAD 10_5_new",
            "LOAD 1000_2_new",
            "SETBUFFER 3",
            "Result3 <- JOIN 10_5_new, 1000_2_new ON 10_5_new.B1 == 1000_2_new.B3 WHERE 10_5_new.C1 != 0",
            "PRINT Result3",
        ],
        [Check("JOIN_3", 5, OUT / "Result3.csv")],
    ),
    Case(
        "JOIN_4",
        [
            "LOAD 500_20_new",
            "LOAD 10000_30_new",
            "SETBUFFER 8",
            "Result4 <- JOIN 500_20_new, 10000_30_new ON 500_20_new.D2 == 10000_30_new.D4 PROJECT 500_20_new.A2, 10000_30_new.E4, 500_20_new.C2",
            "PRINT Result4",
        ],
        [Check("JOIN_4", 5, OUT / "Result4.csv")],
    ),
    Case(
        "JOIN_5",
        [
            "LOAD 10_5_new",
            "LOAD 1000000_15_new",
            "SETBUFFER 10",
            "Result5 <- JOIN 10_5_new, 1000000_15_new ON 10_5_new.A1 - 1000000_15_new.A5 == 20 WHERE 10_5_new.C1 <= 5000 PROJECT 10_5_new.B1, 1000000_15_new.A5",
            "PRINT Result5",
        ],
        [Check("JOIN_5", 5, OUT / "Result5.csv")],
    ),
    Case(
        "GB_10",
        [
            "LOAD 10_5",
            "Result1 <- GROUP BY A FROM 10_5 HAVING AVG(B) == 44 RETURN MIN(E)",
            "PRINT Result1",
        ],
        [Check("GB_10", 3, OUT / "GB_10.csv")],
    ),
    Case(
        "GB_500",
        [
            "LOAD 500_20",
            "Result2, Result3 <- GROUP BY G, S FROM 500_20 HAVING MAX(M) > 0 RETURN MAX(H), SUM(R)",
            "PRINT Result2",
            "PRINT Result3",
        ],
        [
            Check("GB_500_1", 3, OUT / "GB_500_1.csv"),
            Check("GB_500_2", 4, OUT / "GB_500_2.csv"),
        ],
    ),
    Case(
        "GB_1000_ERR",
        ["Result4 <- GROUP BY B FROM 1000_2 HAVING MIN(B) > 1300 RETURN AVG(A)"],
        [Check("GB_1000_ERR", 1, OUT / "GB_1000.txt")],
    ),
    Case(
        "GB_10000",
        [
            "LOAD 10000_30",
            "Result5, Result6, Result7, Result8 <- GROUP BY A, AA, AB, AC FROM 10000_30 HAVING MIN(M) > 240 RETURN MIN(H), MAX(C), COUNT(*), SUM(H)",
            "PRINT Result5",
            "PRINT Result6",
            "PRINT Result7",
            "PRINT Result8",
        ],
        [
            Check("GB_10000_1", 3, OUT / "GB_10000_1.csv"),
            Check("GB_10000_2", 4, OUT / "GB_10000_2.csv"),
            Check("GB_10000_3", 5, OUT / "GB_10000_3.csv"),
            Check("GB_10000_4", 6, OUT / "GB_10000_4.csv"),
        ],
    ),
    Case(
        "GB_BIG",
        [
            "LOAD 1000000_15",
            "Result10, Result11 <- GROUP BY O, C FROM 1000000_15 HAVING MAX(F) <= MAX(B) RETURN COUNT(*), SUM(H)",
            "PRINT Result10",
            "PRINT Result11",
        ],
        [
            Check("GB_BIG_1", 3, OUT / "GB_Big_1.csv"),
            Check("GB_BIG_2", 4, OUT / "GB_Big_2.csv"),
        ],
    ),
    Case(
        "MIXED_1",
        [
            "LOAD 500_20",
            "LOAD join_file",
            "SETBUFFER 6",
            "SORT 500_20 BY A, P, B IN DESC, ASC, DESC BOTTOM 225",
            "EXPORT 500_20",
            "SETBUFFER 4",
            "SORT 500_20 BY A, D, G IN DESC, ASC, DESC",
            "RES1 <- JOIN 500_20, join_file ON 500_20.A == join_file.AAA WHERE join_file.BBB >= 2 PROJECT 500_20.B, join_file.CCC",
            "PRINT RES1",
        ],
        [Check("MIXED_1", 9, OUT / "mixed1_result.csv")],
    ),
    Case(
        "MIXED_2",
        [
            "LOAD 500_20",
            "SETBUFFER 10",
            "Result2 <- GROUP BY G FROM 500_20 HAVING MAX(GROUPBY) > 0 RETURN MAX(H)",
            "LOAD 10_5_2",
            "Result2 <- JOIN 500_20, 10_5_2 ON 500_20.C == 10_5_2.U",
            "SETBUFFER 4",
            "SORT Result2 BY A IN ASC",
            "PRINT Result2",
        ],
        [Check("MIXED_2", 8, OUT / "mixed2_result.csv")],
    ),
    Case(
        "MIXED_3",
        [
            "LOAD 500_20",
            "LOAD join_file2",
            "RES3 <- JOIN 500_20, join_file2 ON 500_20.A == join_file2.AAA",
            "SETBUFFER 3",
            "SORT RES3 BY S, D, AAA IN ASC, ASC, ASC TOP 5",
            "SETBUFFER 8",
            "RES4 <- GROUP BY CCC FROM RES3 HAVING AVG(C) > 1000 RETURN MAX(BBB)",
            "RES4 <- GROUP BY CCC FROM RES3 HAVING AVG(C) <= 500 RETURN MAX(BBB)",
            "SORT RES4 BY CCC, MAXBBB IN DESC, ASC",
            "PRINT RES4",
        ],
        [Check("MIXED_3", 10, OUT / "mixed3_result.csv")],
    ),
    Case(
        "ERR_SORT_1",
        ["SORT A BY Dept IN ASC"],
        [Check("ERR_SORT_1", 1, OUT / "error_sort_1.txt")],
    ),
    Case(
        "ERR_SORT_2",
        ["LOAD 10_5", "SORT 10_5 BY XYZ IN DESC"],
        [Check("ERR_SORT_2", 2, OUT / "error_sort_2.txt")],
    ),
    Case(
        "ERR_JOIN",
        [
            "LOAD 10_5",
            "LOAD 500_20",
            "Result <- JOIN 10_5, 500_20 ON 10_5.EmpID == 500_20.EID WHERE 10_5.Dept == 10 PROJECT 10_5.Name, 500_20.Salary",
        ],
        [Check("ERR_JOIN", 3, OUT / "error_join.txt")],
    ),
    Case(
        "ERR_G_1",
        [
            "LOAD 10_5",
            "LOAD 500_20",
            "Result1, Result2, Result3 <- GROUP BY A, B, S FROM 10_5 HAVING AVG(A) > AVG(B) RETURN MAX(A), MIN(B), SUM(C)",
        ],
        [Check("ERR_G_1", 3, OUT / "error_g_1.txt")],
    ),
    Case(
        "ERR_G_2",
        [
            "LOAD 10_5",
            "LOAD 500_20",
            "Result1, Result2, Result3 <- GROUP BY A, B, C FROM 10_5 HAVING AVG(X) > AVG(B) RETURN MAX(A), MIN(B), SUM(C)",
        ],
        [Check("ERR_G_2", 3, OUT / "error_g_2.txt")],
    ),
    Case(
        "ERR_G_3",
        [
            "LOAD 10_5",
            "LOAD 500_20",
            "Result1, Result2, Result3 <- GROUP BY A, B, C FROM 10_5 HAVING AVG(A) > AVG(B) RETURN MAX(A), MIN(B), SUM(X)",
        ],
        [Check("ERR_G_3", 3, OUT / "error_g_3.txt")],
    ),
]

passed = 0
failed = 0
total_checks = sum(len(c.checks) for c in cases)

for i, case in enumerate(cases, start=1):
    print(f"\n[CASE {i}/{len(cases)}] {case.name}", flush=True)
    try:
        outputs = run_server(case.commands)
    except Exception as e:
        failed += len(case.checks)
        print(f"[FAIL] {case.name}: runner error: {e}", flush=True)
        continue

    for chk in case.checks:
        expected_raw = chk.expected.read_text(encoding="utf-8")
        got_raw = outputs[chk.index - 1]

        if chk.expected.suffix.lower() == ".csv":
            expected = normalize_csv(expected_raw)
            got = normalize_csv(got_raw)
        else:
            expected = normalize_common(expected_raw)
            got = normalize_common(got_raw)

        if expected == got:
            passed += 1
            print(f"[PASS] {chk.name}", flush=True)
        else:
            failed += 1
            line_no, exp_line, got_line = first_diff(expected, got)
            print(f"[FAIL] {chk.name}", flush=True)
            print(f"  expected file: {chk.expected.name}", flush=True)
            print(
                f"  lines expected/got: {len(expected.splitlines())}/{len(got.splitlines())}",
                flush=True,
            )
            if line_no:
                print(f"  first diff line: {line_no}", flush=True)
                print(f"  expected: {exp_line}", flush=True)
                print(f"  got     : {got_line}", flush=True)

print("\n========== SUMMARY ==========", flush=True)
print(f"Total checks : {total_checks}", flush=True)
print(f"Passed       : {passed}", flush=True)
print(f"Failed       : {failed}", flush=True)
sys.exit(1 if failed else 0)
PY
