import argparse
import os
import random
import subprocess
import sys
import importlib.util
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = REPO_ROOT / "src"
DATA_DIR = REPO_ROOT / "data"
SERVER_BIN = SRC_DIR / "server"


def run_server(commands: list[str]) -> list[str]:
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

    lines = []
    for ln in proc.stdout.splitlines():
        ln = ln.strip()
        if not ln:
            continue
        if ln.startswith(">"):
            # prompt or prompt+inline output; keep content after prompt if present
            ln = ln[1:].strip()
            if not ln:
                continue
        lines.append(ln)
    return lines


def ensure_removed(path: Path) -> None:
    try:
        path.unlink()
    except FileNotFoundError:
        return


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", choices=["small", "medium", "large", "very_large"], default="small")
    ap.add_argument("--trials", type=int, default=10)
    ap.add_argument("--seed", type=int, default=0)
    args = ap.parse_args()

    if not SERVER_BIN.exists():
        print(f"Missing {SERVER_BIN}. Build first: cd src && make", file=sys.stderr)
        return 2

    # Reuse the existing Phase1 oracle/generator from repo root.
    stress_py = REPO_ROOT / "stress.py"
    spec = importlib.util.spec_from_file_location("phase1_stress_module", stress_py)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Failed to import {stress_py}")
    phase1 = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(phase1)  # type: ignore[attr-defined]

    size_map = {
        "small": dict(nodes=200, edges=800),
        "medium": dict(nodes=2000, edges=8000),
        "large": dict(nodes=10000, edges=50000),
        "very_large": dict(nodes=30000, edges=150000),
    }
    cfg = size_map[args.size]

    random.seed(args.seed)

    # Generate one directed + one undirected graph for the size category.
    tag = f"{args.size}_{args.seed}_{os.getpid()}"
    gD = f"STRESS_D_{tag}"
    gU = f"STRESS_U_{tag}"
    res = f"STRESS_RES_{tag}"

    oracle_d = phase1.generate_data(gD, cfg["nodes"], cfg["edges"], True)
    oracle_u = phase1.generate_data(gU, cfg["nodes"], cfg["edges"], False)

    # Build a command batch:
    # - load both graphs
    # - run DEGREE and PATH checks
    cmds: list[str] = []
    cmds.append(f"LOAD GRAPH {gD} D")
    cmds.append(f"LOAD GRAPH {gU} U")

    expected: list[str] = []

    # DEGREE trials
    for i in range(args.trials):
        nid = random.randint(0, cfg["nodes"] - 1)
        cmds.append(f"DEGREE {gD} {nid}")
        expected.append(str(oracle_d.get_degree(nid)))

        nid2 = random.randint(0, cfg["nodes"] - 1)
        cmds.append(f"DEGREE {gU} {nid2}")
        expected.append(str(oracle_u.get_degree(nid2)))

    # PATH trials (no WHERE clause to keep parsing simple & stable)
    for i in range(max(3, args.trials // 2)):
        u = random.randint(0, cfg["nodes"] - 1)
        v = random.randint(0, cfg["nodes"] - 1)
        cmds.append(f"{res} <- PATH {gD} {u} {v}")
        expected.append(str(oracle_d.get_shortest_path(u, v, [])))

        u2 = random.randint(0, cfg["nodes"] - 1)
        v2 = random.randint(0, cfg["nodes"] - 1)
        cmds.append(f"{res} <- PATH {gU} {u2} {v2}")
        expected.append(str(oracle_u.get_shortest_path(u2, v2, [])))

    cmds.append("QUIT")

    out_lines = run_server(cmds)

    # Filter to only the DEGREE/PATH single-line outputs (skip load lines)
    got = []
    for ln in out_lines:
        if ln.startswith("Loaded Graph"):
            continue
        got.append(ln)

    # Compare in order.
    if len(got) < len(expected):
        raise AssertionError(f"Too few outputs: got={len(got)} expected={len(expected)}")

    for i, exp in enumerate(expected):
        if got[i] != exp:
            raise AssertionError(f"Mismatch at {i}: got={got[i]!r} expected={exp!r}")

    # Cleanup generated CSV files so stress runs don't dirty the working tree.
    for suffix in ["Nodes_D", "Edges_D", "Nodes_U", "Edges_U"]:
        ensure_removed(DATA_DIR / f"{gD}_{suffix}.csv")
        ensure_removed(DATA_DIR / f"{gU}_{suffix}.csv")
        ensure_removed(DATA_DIR / f"{res}_{suffix}.csv")

    print(f"Phase1 stress OK: size={args.size} trials={args.trials} seed={args.seed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
