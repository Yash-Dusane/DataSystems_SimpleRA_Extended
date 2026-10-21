#!/usr/bin/env python3
from __future__ import annotations

import argparse
import difflib
import subprocess
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parent
SRC_DIR = REPO_ROOT / "src"
SERVER_BIN = SRC_DIR / "server"
TESTS_DIR = HERE / "tests"
OUTPUTS_DIR = HERE / "outputs"


def filter_output(raw: str) -> str:
    out_lines: list[str] = []
    for line in raw.splitlines():
        s = line.strip()
        if not s:
            continue
        if s.startswith(">"):
            s = s[1:].strip()
            if not s:
                continue
        out_lines.append(s)
    if not out_lines:
        return ""
    return "\n".join(out_lines) + "\n"


def run_server(script_path: Path) -> str:
    proc = subprocess.run(
        [str(SERVER_BIN)],
        cwd=str(SRC_DIR),
        input=script_path.read_text(encoding="utf-8"),
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
    return filter_output(proc.stdout)


def iter_tests(pattern: str | None) -> list[Path]:
    tests = sorted(TESTS_DIR.glob("*.ra"))
    if pattern:
        tests = [p for p in tests if pattern in p.name]
    return tests


def main() -> int:
    ap = argparse.ArgumentParser(description="Run Phase 2 golden tests against src/server output.")
    ap.add_argument("--pattern", help="Run only tests whose filename contains this substring.")
    ap.add_argument("--build", action="store_true", help="Build server before running tests (make -C src).")
    ap.add_argument("--update-expected", action="store_true", help="Overwrite expected outputs on mismatch.")
    args = ap.parse_args()

    if args.build:
        build = subprocess.run(["make", "-C", str(SRC_DIR)], text=True)
        if build.returncode != 0:
            return build.returncode

    if not SERVER_BIN.exists():
        print(f"Missing {SERVER_BIN}. Build first: cd src && make", file=sys.stderr)
        return 2

    OUTPUTS_DIR.mkdir(parents=True, exist_ok=True)

    tests = iter_tests(args.pattern)
    if not tests:
        print("No tests found.", file=sys.stderr)
        return 2

    failed = 0
    for test in tests:
        expected_path = OUTPUTS_DIR / f"{test.stem}.expected.txt"
        got = run_server(test)
        expected = expected_path.read_text(encoding="utf-8") if expected_path.exists() else None

        if expected is None:
            expected_path.write_text(got, encoding="utf-8")
            print(f"[NEW] {test.name} -> {expected_path.name}")
            continue

        if got != expected:
            failed += 1
            if args.update_expected:
                expected_path.write_text(got, encoding="utf-8")
                print(f"[UPDATE] {test.name}")
                continue

            diff = "".join(
                difflib.unified_diff(
                    expected.splitlines(True),
                    got.splitlines(True),
                    fromfile=str(expected_path),
                    tofile=f"{test} (got)",
                )
            )
            print(f"[FAIL] {test.name}\n{diff}")
        else:
            print(f"[OK]   {test.name}")

    if failed:
        print(f"\nFAILED: {failed}/{len(tests)} tests")
        return 1
    print(f"\nPASSED: {len(tests)}/{len(tests)} tests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

