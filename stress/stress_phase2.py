import argparse
import csv
import math
import os
import random
import subprocess
import sys
from collections import Counter
from dataclasses import dataclass
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


def read_csv_ints(path: Path) -> tuple[list[str], list[list[int]]]:
    with path.open("r", newline="") as f:
        r = csv.reader(f)
        header = next(r)
        rows = [[int(x.strip()) for x in row] for row in r if row]
    return header, rows


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
        raise RuntimeError(f"server exited with {proc.returncode}\nSTDERR:\n{proc.stderr}\nSTDOUT:\n{proc.stdout}")
    return proc.stdout


def key_tuple(row: list[int], key_cols: list[int], key_dirs: list[str]) -> tuple:
    out = []
    for idx, direction in zip(key_cols, key_dirs):
        v = row[idx]
        out.append(v if direction == "ASC" else -v)
    return tuple(out)


def stable_segment_sort(rows: list[list[int]], key_cols: list[int], key_dirs: list[str]) -> list[list[int]]:
    return sorted(rows, key=lambda r: key_tuple(r, key_cols, key_dirs))


def apply_sort_with_top_bottom(
    rows: list[list[int]],
    key_cols: list[int],
    key_dirs: list[str],
    top: int | None,
    bottom: int | None,
) -> list[list[int]]:
    n = len(rows)
    top_n = top if top is not None else 0
    bottom_n = bottom if bottom is not None else 0
    if top is None and bottom is None:
        top_n = n
    if top_n + bottom_n > n:
        raise ValueError("TOP + BOTTOM overlap")

    out = []
    if top_n:
        out.extend(stable_segment_sort(rows[:top_n], key_cols, key_dirs))
    out.extend(rows[top_n : n - bottom_n])
    if bottom_n:
        out.extend(stable_segment_sort(rows[n - bottom_n :], key_cols, key_dirs))
    return out


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


@dataclass
class JoinSpec:
    kind: str  # "attr" | "arith"
    a_col: int
    b_col: int
    arith_op: str | None = None  # "+" | "-"
    constant: int | None = None
    where_col: int | None = None
    where_op: str | None = None
    where_val: int | None = None
    project: list[tuple[str, int]] | None = None  # [("A", idx), ("B", idx)]


def expected_join(
    rows_a: list[list[int]],
    rows_b: list[list[int]],
    spec: JoinSpec,
) -> list[list[int]]:
    if spec.where_col is not None:
        rows_a = [ra for ra in rows_a if eval_binop(ra[spec.where_col], spec.where_op, spec.where_val)]

    out: list[list[int]] = []

    def emit(ra: list[int], rb: list[int]) -> None:
        if spec.project:
            out.append([ra[i] if t == "A" else rb[i] for (t, i) in spec.project])
        else:
            out.append(list(ra) + list(rb))

    if spec.kind == "attr":
        ht: dict[int, list[list[int]]] = {}
        for rb in rows_b:
            ht.setdefault(rb[spec.b_col], []).append(rb)
        for ra in rows_a:
            for rb in ht.get(ra[spec.a_col], []):
                emit(ra, rb)
        return out

    # Arithmetic join:
    #  - PLUS:  A.a + B.b == c  =>  B.b == c - A.a
    #  - MINUS: A.a - B.b == c  =>  B.b == A.a - c
    ht2: dict[int, list[list[int]]] = {}
    for rb in rows_b:
        ht2.setdefault(rb[spec.b_col], []).append(rb)

    if spec.arith_op == "+":
        for ra in rows_a:
            need = spec.constant - ra[spec.a_col]
            for rb in ht2.get(need, []):
                emit(ra, rb)
        return out

    for ra in rows_a:
        need = ra[spec.a_col] - spec.constant
        for rb in ht2.get(need, []):
            emit(ra, rb)
    return out


def agg_value(func: str, values: list[int]) -> int:
    if func == "MAX":
        return max(values)
    if func == "MIN":
        return min(values)
    if func == "SUM":
        return sum(values)
    if func == "COUNT":
        return len(values)
    if func == "AVG":
        return int(math.ceil(sum(values) / len(values))) if values else 0
    raise ValueError(func)


def expected_group_by(
    rows: list[list[int]],
    group_col: int,
    having_lhs: tuple[str, int | None],  # (func, col) col None => COUNT(*)
    having_op: str,
    having_rhs: tuple[str, int | None] | int,
    return_agg: tuple[str, int | None],
) -> list[list[int]]:
    groups: dict[int, list[list[int]]] = {}
    for r in rows:
        groups.setdefault(r[group_col], []).append(r)

    out: list[list[int]] = []
    for gval, grows in groups.items():
        def eval_term(term):
            if isinstance(term, int):
                return term
            fn, col = term
            if fn == "COUNT" and col is None:
                return len(grows)
            vals = [rr[col] for rr in grows]
            if fn == "COUNT":
                return len(vals)
            return agg_value(fn, vals)

        lhs = eval_term(having_lhs)
        rhs = eval_term(having_rhs)
        if eval_binop(lhs, having_op, rhs):
            fn, col = return_agg
            if fn == "COUNT" and col is None:
                ret = len(grows)
            else:
                vals = [rr[col] for rr in grows]
                ret = agg_value(fn if fn != "COUNT" else "COUNT", vals)
            out.append([gval, ret])

    out.sort(key=lambda x: x[0])
    return out


def ensure_removed(path: Path) -> None:
    try:
        path.unlink()
    except FileNotFoundError:
        return


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", choices=["small", "medium", "large", "very_large"], default="small")
    ap.add_argument("--trials", type=int, help="Number of randomized trials (defaults vary by --size).")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--buffers", type=int, help="SETBUFFER value (defaults vary by --size).")
    args = ap.parse_args()

    if not SERVER_BIN.exists():
        print(f"Missing {SERVER_BIN}. Build first: cd src && make", file=sys.stderr)
        return 2

    size_map = {
        "small": dict(sort_rows=400, sort_cols=6, join_a=800, join_b=600, gb_rows=2000, buffers=10, trials=5),
        "medium": dict(sort_rows=4000, sort_cols=8, join_a=8000, join_b=6000, gb_rows=20000, buffers=20, trials=3),
        "large": dict(sort_rows=30000, sort_cols=10, join_a=50000, join_b=40000, gb_rows=60000, buffers=50, trials=2),
        "very_large": dict(sort_rows=90000, sort_cols=10, join_a=120000, join_b=90000, gb_rows=90000, buffers=70, trials=1),
    }
    cfg = size_map[args.size]
    buffers = args.buffers if args.buffers is not None else int(cfg["buffers"])
    trials = args.trials if args.trials is not None else int(cfg["trials"])

    random.seed(args.seed)
    DATA_DIR.mkdir(exist_ok=True)

    for t in range(trials):
        tag = f"{args.size}_{args.seed}_{t}_{os.getpid()}"

        # --- SORT test table ---
        sort_name = f"STRESS_SORT_{tag}"
        sort_header = [f"C{i}" for i in range(cfg["sort_cols"])]
        sort_rows = [[random.randint(0, 1000) for _ in range(cfg["sort_cols"])] for _ in range(cfg["sort_rows"])]
        sort_path = DATA_DIR / f"{sort_name}.csv"
        write_csv(sort_path, sort_header, sort_rows)

        key_full_cols = [1, 3] if cfg["sort_cols"] >= 4 else [1]
        key_full_dirs = ["DESC", "ASC"] if len(key_full_cols) == 2 else ["DESC"]
        expected_sorted_full = stable_segment_sort(sort_rows, key_full_cols, key_full_dirs)

        key_restart_cols = [2, 0] if cfg["sort_cols"] >= 3 else [0]
        key_restart_dirs = ["ASC", "DESC"] if len(key_restart_cols) == 2 else ["ASC"]
        top = min(250, cfg["sort_rows"] // 10) if cfg["sort_rows"] >= 1000 else None
        bottom = min(120, cfg["sort_rows"] // 20) if cfg["sort_rows"] >= 1000 else None
        expected_sorted_restart = apply_sort_with_top_bottom(sort_rows, key_restart_cols, key_restart_dirs, top, bottom)

        # --- JOIN test tables ---
        a_name = f"STRESS_A_{tag}"
        b_name = f"STRESS_B_{tag}"
        a_header = [f"A{i}" for i in range(5)]
        b_header = [f"B{i}" for i in range(4)]
        rows_a: list[list[int]] = []
        rows_b: list[list[int]] = []

        # Use join keys that are balanced modulo (k-1) to keep hash partitions even.
        join_a = int(cfg["join_a"])
        join_b = int(cfg["join_b"])
        for i in range(join_a):
            # A0=id, A1=whereCol (0..99), A2=joinKey, A3/A4 random payload
            rows_a.append([i, i % 100, i, random.randint(0, 10_000), random.randint(0, 10_000)])
        for i in range(join_b):
            # B0 joinKey matches A2 for first join_b keys -> at most 1 match per key
            rows_b.append([i, random.randint(0, 10_000), random.randint(0, 10_000), random.randint(0, 100)])

        write_csv(DATA_DIR / f"{a_name}.csv", a_header, rows_a)
        write_csv(DATA_DIR / f"{b_name}.csv", b_header, rows_b)

        join_res = f"STRESS_JOIN_{tag}"
        join_spec = JoinSpec(
            kind="attr",
            a_col=2,
            b_col=0,
            where_col=1,
            where_op=">=",
            where_val=50,
            project=[("A", 0), ("B", 1), ("A", 2)],
        )
        expected_join_rows = expected_join(rows_a, rows_b, join_spec)

        # --- GROUP BY test table ---
        gb_name = f"STRESS_GB_{tag}"
        gb_header = ["G", "X", "Y", "Z"]
        gb_rows = [
            [random.randint(1, 200), random.randint(0, 100000), random.randint(0, 100000), random.randint(0, 500)]
            for _ in range(int(cfg["gb_rows"]))
        ]
        write_csv(DATA_DIR / f"{gb_name}.csv", gb_header, gb_rows)

        gb_res1 = f"STRESS_GB_R1_{tag}"
        gb_res2 = f"STRESS_GB_R2_{tag}"
        gb_empty = f"STRESS_GB_EMPTY_{tag}"

        expected_gb1 = expected_group_by(
            gb_rows,
            group_col=0,
            having_lhs=("AVG", 1),
            having_op=">",
            having_rhs=("AVG", 2),
            return_agg=("MAX", 1),
        )
        expected_gb2 = expected_group_by(
            gb_rows,
            group_col=3,
            having_lhs=("AVG", 1),
            having_op=">",
            having_rhs=("AVG", 2),
            return_agg=("COUNT", None),
        )
        expected_empty = expected_group_by(
            gb_rows,
            group_col=3,
            having_lhs=("MAX", 3),
            having_op="<",
            having_rhs=0,
            return_agg=("COUNT", None),
        )

        # Remove any stale exports from prior runs
        ensure_removed(DATA_DIR / f"{join_res}.csv")
        ensure_removed(DATA_DIR / f"{gb_res1}.csv")
        ensure_removed(DATA_DIR / f"{gb_res2}.csv")
        ensure_removed(DATA_DIR / f"{gb_empty}.csv")

        join_cmd = (
            f"{join_res} <- JOIN {a_name}, {b_name} "
            f"ON {a_name}.{a_header[join_spec.a_col]} == {b_name}.{b_header[join_spec.b_col]} "
            f"WHERE {a_name}.{a_header[join_spec.where_col]} {join_spec.where_op} {join_spec.where_val} "
            f"PROJECT {a_name}.{a_header[0]}, {b_name}.{b_header[1]}, {a_name}.{a_header[2]};"
        )

        gb_cmd = (
            f"{gb_res1}, {gb_res2} <- GROUP BY {gb_header[0]}, {gb_header[3]} "
            f"FROM {gb_name} HAVING AVG({gb_header[1]}) > AVG({gb_header[2]}) "
            f"RETURN MAX({gb_header[1]}), COUNT(*);"
        )
        gb_empty_cmd = (
            f"{gb_empty} <- GROUP BY {gb_header[3]} "
            f"FROM {gb_name} HAVING MAX({gb_header[3]}) < 0 "
            f"RETURN COUNT(*);"
        )

        # Main session: full-table SORT export + JOIN + GROUP BY.
        sort_full_cmd = (
            f"SORT {sort_name} BY "
            + ", ".join([sort_header[i] for i in key_full_cols])
            + " IN "
            + ", ".join(key_full_dirs)
        )

        commands_main = [
            f"SETBUFFER {buffers}",
            f"LOAD {sort_name}",
            sort_full_cmd,
            f"EXPORT {sort_name}",
            f"LOAD {a_name}",
            f"LOAD {b_name}",
            join_cmd,
            f"EXPORT {join_res}",
            f"LOAD {gb_name}",
            gb_cmd,
            f"EXPORT {gb_res1}",
            f"EXPORT {gb_res2}",
            gb_empty_cmd,
            f"EXPORT {gb_empty}",
            "QUIT",
        ]

        stdout = run_server(commands_main)

        # --- Verify SORT ---
        _, got_sort = read_csv_ints(DATA_DIR / f"{sort_name}.csv")
        if got_sort != expected_sorted_full:
            raise AssertionError(f"SORT mismatch on trial {t} ({args.size})")

        # --- Verify JOIN (order-insensitive, multiset) ---
        _, got_join = read_csv_ints(DATA_DIR / f"{join_res}.csv")
        if Counter(map(tuple, got_join)) != Counter(map(tuple, expected_join_rows)):
            raise AssertionError(f"JOIN mismatch on trial {t} ({args.size})")

        # --- Verify GROUP BY result 1 ---
        _, got_gb1 = read_csv_ints(DATA_DIR / f"{gb_res1}.csv")
        if sorted(got_gb1) != sorted(expected_gb1):
            raise AssertionError(f"GROUP BY mismatch on trial {t} ({args.size})")

        # --- Verify GROUP BY result 2 ---
        _, got_gb2 = read_csv_ints(DATA_DIR / f"{gb_res2}.csv")
        if sorted(got_gb2) != sorted(expected_gb2):
            raise AssertionError(f"GROUP BY mismatch on trial {t} ({args.size})")

        # --- Verify empty GROUP BY result should be absent (table not created) ---
        empty_path = DATA_DIR / f"{gb_empty}.csv"
        if expected_empty:
            raise AssertionError("expected_empty should be empty but wasn't")
        if empty_path.exists():
            raise AssertionError(f"Expected empty {gb_empty} (no table), but export file exists")
        if "SEMANTIC ERROR" not in stdout:
            raise AssertionError("Expected a semantic error for exporting missing empty GROUP BY result")

        # --- Verify SORT restart semantics (sort always starts from original unless exported) ---
        # Restore original input CSV (main session export overwrote it), then run:
        #  - SORT with one key (no export)
        #  - SORT again with TOP/BOTTOM and export
        write_csv(sort_path, sort_header, sort_rows)

        sort_restart_cmd_1 = (
            f"SORT {sort_name} BY {sort_header[key_full_cols[0]]} IN {key_full_dirs[0]}"
        )
        sort_restart_cmd_2 = (
            f"SORT {sort_name} BY "
            + ", ".join([sort_header[i] for i in key_restart_cols])
            + " IN "
            + ", ".join(key_restart_dirs)
            + (f" TOP {top}" if top is not None else "")
            + (f" BOTTOM {bottom}" if bottom is not None else "")
        )
        commands_restart = [
            f"SETBUFFER {buffers}",
            f"LOAD {sort_name}",
            sort_restart_cmd_1,
            sort_restart_cmd_2,
            f"EXPORT {sort_name}",
            "QUIT",
        ]
        run_server(commands_restart)
        _, got_restart = read_csv_ints(DATA_DIR / f"{sort_name}.csv")
        if got_restart != expected_sorted_restart:
            raise AssertionError(f"SORT restart mismatch on trial {t} ({args.size})")

        # Cleanup generated CSVs
        for name in [sort_name, a_name, b_name, join_res, gb_name, gb_res1, gb_res2, gb_empty]:
            ensure_removed(DATA_DIR / f"{name}.csv")

    print(f"Phase2 stress OK: size={args.size} trials={trials} seed={args.seed} buffers={buffers}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
