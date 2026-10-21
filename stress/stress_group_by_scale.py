import argparse
import csv
import os
import subprocess
import sys
import time
from collections import defaultdict
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = REPO_ROOT / "data"
SRC_DIR = REPO_ROOT / "src"
SERVER_BIN = SRC_DIR / "server"
INT_MAX = 2_147_483_647


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


def eval_binop(a: int, op: str, b: int) -> bool:
    if op == "==":
        return a == b
    if op == "!=":
        return a != b
    if op == "<":
        return a < b
    if op == "<=":
        return a <= b
    if op == ">":
        return a > b
    if op == ">=":
        return a >= b
    raise ValueError(op)


def ceil_avg(values: list[int]) -> int:
    if not values:
        return 0
    total = sum(values)
    if total >= 0:
        return (total + len(values) - 1) // len(values)
    return total // len(values)


def eval_agg(rows: list[list[int]], func: str, col_idx: int | None) -> int:
    if func == "COUNT" and col_idx is None:
        return len(rows)

    values = [row[col_idx] for row in rows]
    if func == "MIN":
        return min(values)
    if func == "MAX":
        return max(values)
    if func == "SUM":
        return sum(values)
    if func == "COUNT":
        return len(values)
    if func == "AVG":
        return ceil_avg(values)
    raise ValueError(func)


def expected_group_by(
    rows: list[list[int]],
    group_idx: int,
    having_lhs: tuple[str, int | None],
    having_op: str,
    having_rhs: tuple[str, int | None] | int,
    return_agg: tuple[str, int | None],
) -> list[list[int]]:
    groups: dict[int, list[list[int]]] = defaultdict(list)
    for row in rows:
        groups[row[group_idx]].append(row)

    out: list[list[int]] = []
    for group_val, grows in groups.items():
        lhs_val = eval_agg(grows, having_lhs[0], having_lhs[1])
        if isinstance(having_rhs, int):
            rhs_val = having_rhs
        else:
            rhs_val = eval_agg(grows, having_rhs[0], having_rhs[1])

        if eval_binop(lhs_val, having_op, rhs_val):
            ret_val = eval_agg(grows, return_agg[0], return_agg[1])
            out.append([group_val, ret_val])

    out.sort()
    return out


def generate_table(tag: str, rows_count: int, groups_g: int, groups_z: int) -> tuple[str, list[list[int]]]:
    name = f"STRESS_GB_SCALE_{tag}"
    rows: list[list[int]] = []

    for i in range(rows_count):
        g = i % groups_g
        z = (i * 17 + 3) % groups_z

        x = 2200 + ((i * 13 + g * 19) % 1600)
        if g % 4 in (0, 1):
            x += 500
        if z % 5 == 0:
            x += 120

        y = 2300 + ((i * 11 + z * 23) % 1500)
        if g % 4 in (2, 3):
            y += 460
        if z % 5 in (1, 2):
            y += 90

        # Keep SUM(BIG) close to the 32-bit boundary without exceeding it.
        big = 9_500_000 + (g % 97) * 173 + (i % 29)
        rows.append([g, z, x, y, big])

    write_csv(DATA_DIR / f"{name}.csv", ["G", "Z", "X", "Y", "BIG"], rows)
    return name, rows


def verify_result(path: Path, expected_header: list[str], expected_rows: list[list[int]], label: str) -> None:
    header, rows = read_csv_rows(path)
    if header != expected_header:
        raise AssertionError(f"Unexpected header for {label}: got {header}, expected {expected_header}")
    if sorted(rows) != expected_rows:
        raise AssertionError(
            f"{label} mismatch: got {len(rows)} rows, expected {len(expected_rows)} rows"
        )


def run_trial(rows_count: int, groups_g: int, groups_z: int, buffers: int, seed: int, trial: int, keep_files: bool) -> None:
    tag = f"groupbyscale_{seed}_{trial}_{os.getpid()}"
    table_name, rows = generate_table(tag, rows_count, groups_g, groups_z)

    res_min = f"STRESS_GB_MIN_{tag}"
    res_count = f"STRESS_GB_COUNT_{tag}"
    res_sum = f"STRESS_GB_SUM_{tag}"

    for name in [res_min, res_count, res_sum]:
        remove_if_exists(DATA_DIR / f"{name}.csv")

    expected_min = expected_group_by(
        rows=rows,
        group_idx=0,
        having_lhs=("AVG", 2),
        having_op=">",
        having_rhs=("AVG", 3),
        return_agg=("MIN", 2),
    )
    expected_count = expected_group_by(
        rows=rows,
        group_idx=1,
        having_lhs=("AVG", 2),
        having_op=">",
        having_rhs=("AVG", 3),
        return_agg=("COUNT", 2),
    )
    expected_sum = expected_group_by(
        rows=rows,
        group_idx=0,
        having_lhs=("MAX", 4),
        having_op=">=",
        having_rhs=0,
        return_agg=("SUM", 4),
    )

    if not expected_min or not expected_count:
        raise AssertionError("Generator produced an uninformative trial with empty MIN/COUNT results")
    max_sum = max(row[1] for row in expected_sum)
    if max_sum >= INT_MAX:
        raise AssertionError(f"Expected SUM crossed INT_MAX boundary: {max_sum}")

    commands = [
        f"SETBUFFER {buffers}",
        f"LOAD {table_name}",
        f"{res_min}, {res_count} <- GROUP BY G, Z FROM {table_name} HAVING AVG(X) > AVG(Y) RETURN MIN(X), COUNT(X);",
        f"EXPORT {res_min}",
        f"EXPORT {res_count}",
        f"{res_sum} <- GROUP BY G FROM {table_name} HAVING MAX(BIG) >= 0 RETURN SUM(BIG);",
        f"EXPORT {res_sum}",
        "QUIT",
    ]

    t0 = time.perf_counter()
    run_server(commands)
    elapsed = time.perf_counter() - t0

    verify_result(DATA_DIR / f"{res_min}.csv", ["G", "MINX"], expected_min, "MIN result")
    verify_result(DATA_DIR / f"{res_count}.csv", ["Z", "COUNTX"], expected_count, "COUNT(column) result")
    verify_result(DATA_DIR / f"{res_sum}.csv", ["G", "SUMBIG"], expected_sum, "large SUM result")

    print(
        "[OK] trial={} rows={} groupsG={} groupsZ={} buffers={} elapsed={:.2f}s min_groups={} count_groups={} max_sum={}".format(
            trial,
            rows_count,
            groups_g,
            groups_z,
            buffers,
            elapsed,
            len(expected_min),
            len(expected_count),
            max_sum,
        )
    )

    if keep_files:
        return

    for name in [table_name, res_min, res_count, res_sum]:
        remove_if_exists(DATA_DIR / f"{name}.csv")


def main() -> int:
    ap = argparse.ArgumentParser(description="Scalable verifier for Phase 2 GROUP BY implementation.")
    ap.add_argument("--rows", type=int, default=30000, help="Rows in generated table")
    ap.add_argument("--groups-g", type=int, default=150, help="Distinct values for grouping column G")
    ap.add_argument("--groups-z", type=int, default=101, help="Distinct values for grouping column Z")
    ap.add_argument("--buffers", type=int, default=10, help="SETBUFFER value (2..10 for current parser)")
    ap.add_argument("--trials", type=int, default=1, help="Number of trials")
    ap.add_argument("--seed", type=int, default=0, help="Seed tag component")
    ap.add_argument("--keep-files", action="store_true", help="Keep generated CSVs for inspection")
    args = ap.parse_args()

    if args.rows <= 0 or args.groups_g <= 0 or args.groups_z <= 0:
        print("rows, groups-g, and groups-z must be positive", file=sys.stderr)
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
            rows_count=args.rows,
            groups_g=args.groups_g,
            groups_z=args.groups_z,
            buffers=args.buffers,
            seed=args.seed,
            trial=t,
            keep_files=args.keep_files,
        )

    print(
        "Group-by scale verification OK: rows={} groupsG={} groupsZ={} trials={} buffers={}".format(
            args.rows,
            args.groups_g,
            args.groups_z,
            args.trials,
            args.buffers,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())