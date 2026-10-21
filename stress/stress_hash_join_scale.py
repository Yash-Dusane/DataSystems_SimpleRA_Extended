import argparse
import csv
import os
import subprocess
import sys
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = REPO_ROOT / "data"
SRC_DIR = REPO_ROOT / "src"
SERVER_BIN = SRC_DIR / "server"


def write_csv(path: Path, header: list[str], rows: list[list[int]]) -> None:
    with path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)


def run_server(commands: list[str]) -> str:
    proc = subprocess.run(
        [str(SERVER_BIN)],
        cwd=str(SRC_DIR),
        input="\n".join(commands) + "\n",
        text=True,
        capture_output=True,
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"server exited with {proc.returncode}\nSTDERR:\n{proc.stderr}\nSTDOUT:\n{proc.stdout}"
        )
    return proc.stdout


def read_csv_rows(path: Path) -> tuple[list[str], list[list[int]]]:
    with path.open("r", newline="") as f:
        r = csv.reader(f)
        header = [h.strip() for h in next(r)]
        rows = [[int(x.strip()) for x in row] for row in r if row]
    return header, rows


def remove_if_exists(path: Path) -> None:
    try:
        path.unlink()
    except FileNotFoundError:
        return


def generate_tables(tag: str, rows_a: int, rows_b: int) -> tuple[str, str]:
    a_name = f"STRESS_HJ_A_{tag}"
    b_name = f"STRESS_HJ_B_{tag}"

    a_rows: list[list[int]] = []
    b_rows: list[list[int]] = []

    for i in range(rows_a):
        # A0=id, A1=whereCol, A2=joinKey, A3=payload
        a_rows.append([i, i % 100, i, (i * 31 + 7) % 100000])

    for i in range(rows_b):
        # B0=joinKey, B1/B2 payload
        b_rows.append([i, (i * 17 + 13) % 100000, (i * 19 + 5) % 100000])

    write_csv(DATA_DIR / f"{a_name}.csv", ["A0", "A1", "A2", "A3"], a_rows)
    write_csv(DATA_DIR / f"{b_name}.csv", ["B0", "B1", "B2"], b_rows)
    return a_name, b_name


def verify_attr_join_result(path: Path, rows_a: int, rows_b: int, where_threshold: int) -> tuple[int, int]:
    header, rows = read_csv_rows(path)
    if header != ["A0", "B1", "A2"]:
        raise AssertionError(f"Unexpected header for attr join result: {header}")

    expected_count = sum(1 for k in range(min(rows_a, rows_b)) if (k % 100) >= where_threshold)
    seen_keys: set[int] = set()

    for row in rows:
        if len(row) != 3:
            raise AssertionError(f"Malformed attr join row: {row}")
        a0, b1, a2 = row
        if a0 != a2:
            raise AssertionError(f"A0 and A2 should match in projection, got row={row}")

        k = a2
        if k < 0 or k >= min(rows_a, rows_b):
            raise AssertionError(f"Join key out of expected range in attr join result: {k}")
        if (k % 100) < where_threshold:
            raise AssertionError(f"WHERE filter violated for key {k}")

        expected_b1 = (k * 17 + 13) % 100000
        if b1 != expected_b1:
            raise AssertionError(f"Incorrect B1 for key {k}: got {b1}, expected {expected_b1}")

        if k in seen_keys:
            raise AssertionError(f"Duplicate attr join key encountered: {k}")
        seen_keys.add(k)

    got_count = len(rows)
    if got_count != expected_count:
        raise AssertionError(f"Attr join row count mismatch: got {got_count}, expected {expected_count}")

    return got_count, expected_count


def verify_arith_plus_result(path: Path, rows_a: int, rows_b: int, constant: int) -> tuple[int, int]:
    header, rows = read_csv_rows(path)
    if header != ["A0", "B0", "A2"]:
        raise AssertionError(f"Unexpected header for arithmetic join result: {header}")

    max_key = min(rows_a - 1, constant)
    min_key = max(0, constant - (rows_b - 1))
    expected_count = max(0, max_key - min_key + 1)

    seen_keys: set[int] = set()

    for row in rows:
        if len(row) != 3:
            raise AssertionError(f"Malformed arithmetic join row: {row}")
        a0, b0, a2 = row

        if a0 != a2:
            raise AssertionError(f"A0 and A2 should match in projection, got row={row}")
        if a2 + b0 != constant:
            raise AssertionError(f"Arithmetic join condition violated in row={row}")
        if a2 < min_key or a2 > max_key:
            raise AssertionError(f"Arithmetic key out of expected range: {a2}")
        if b0 < 0 or b0 >= rows_b:
            raise AssertionError(f"Arithmetic B0 out of expected range: {b0}")
        if a2 in seen_keys:
            raise AssertionError(f"Duplicate arithmetic join key encountered: {a2}")
        seen_keys.add(a2)

    got_count = len(rows)
    if got_count != expected_count:
        raise AssertionError(f"Arithmetic join row count mismatch: got {got_count}, expected {expected_count}")

    return got_count, expected_count


def run_trial(rows_a: int, rows_b: int, buffers: int, seed: int, trial: int, keep_files: bool) -> None:
    tag = f"joinscale_{seed}_{trial}_{os.getpid()}"
    a_name, b_name = generate_tables(tag, rows_a, rows_b)

    res_attr = f"STRESS_HJ_ATTR_{tag}"
    res_plus = f"STRESS_HJ_PLUS_{tag}"

    where_threshold = 50
    plus_constant = rows_b - 1

    remove_if_exists(DATA_DIR / f"{res_attr}.csv")
    remove_if_exists(DATA_DIR / f"{res_plus}.csv")

    commands = [
        f"SETBUFFER {buffers}",
        f"LOAD {a_name}",
        f"LOAD {b_name}",
        f"{res_attr} <- JOIN {a_name}, {b_name} ON {a_name}.A2 == {b_name}.B0 WHERE {a_name}.A1 >= {where_threshold} PROJECT {a_name}.A0, {b_name}.B1, {a_name}.A2;",
        f"EXPORT {res_attr}",
        f"{res_plus} <- JOIN {a_name}, {b_name} ON {a_name}.A2 + {b_name}.B0 == {plus_constant} PROJECT {a_name}.A0, {b_name}.B0, {a_name}.A2;",
        f"EXPORT {res_plus}",
        "QUIT",
    ]

    t0 = time.perf_counter()
    run_server(commands)
    elapsed = time.perf_counter() - t0

    attr_got, attr_expected = verify_attr_join_result(DATA_DIR / f"{res_attr}.csv", rows_a, rows_b, where_threshold)
    plus_got, plus_expected = verify_arith_plus_result(DATA_DIR / f"{res_plus}.csv", rows_a, rows_b, plus_constant)

    print(
        "[OK] trial={} rowsA={} rowsB={} buffers={} elapsed={:.2f}s attr_rows={}/{} plus_rows={}/{}".format(
            trial,
            rows_a,
            rows_b,
            buffers,
            elapsed,
            attr_got,
            attr_expected,
            plus_got,
            plus_expected,
        )
    )

    if keep_files:
        return

    for name in [a_name, b_name, res_attr, res_plus]:
        remove_if_exists(DATA_DIR / f"{name}.csv")


def main() -> int:
    ap = argparse.ArgumentParser(description="Scalable verifier for Phase 2 hash join implementation.")
    ap.add_argument("--rows-a", type=int, default=200000, help="Rows in left table A")
    ap.add_argument("--rows-b", type=int, default=150000, help="Rows in right table B")
    ap.add_argument("--buffers", type=int, default=10, help="SETBUFFER value (2..10 for current parser)")
    ap.add_argument("--trials", type=int, default=1, help="Number of trials")
    ap.add_argument("--seed", type=int, default=0, help="Random seed tag component")
    ap.add_argument("--keep-files", action="store_true", help="Keep generated CSVs for inspection")
    args = ap.parse_args()

    if args.rows_a <= 0 or args.rows_b <= 0:
        print("rows-a and rows-b must be positive", file=sys.stderr)
        return 2
    if args.buffers < 2 or args.buffers > 10:
        print("buffers must be in [2, 10] for this build", file=sys.stderr)
        return 2
    if args.trials <= 0:
        print("trials must be positive", file=sys.stderr)
        return 2
    if not SERVER_BIN.exists():
        print(f"Missing {SERVER_BIN}. Build first: cd src && make", file=sys.stderr)
        return 2

    DATA_DIR.mkdir(exist_ok=True)

    for t in range(args.trials):
        run_trial(
            rows_a=args.rows_a,
            rows_b=args.rows_b,
            buffers=args.buffers,
            seed=args.seed,
            trial=t,
            keep_files=args.keep_files,
        )

    print(
        "Hash-join scale verification OK: rowsA={} rowsB={} trials={} buffers={}".format(
            args.rows_a,
            args.rows_b,
            args.trials,
            args.buffers,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
