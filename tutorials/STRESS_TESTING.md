# Stress Testing (Phase 1 + Phase 2)

## What’s included
- Phase 1 graph stress runner: `stress/stress_phase1.py`
- Phase 2 relational stress runner: `stress/stress_phase2.py`

Both scripts:
- Generate randomized input data into `data/`
- Run `src/server`
- Verify results for correctness
- Clean up the generated CSVs after each trial

## Build
`cd src && make clean && make`

## Phase 2
`python3 stress/stress_phase2.py --size small --trials 5 --seed 0 --buffers 10`

## Phase 1
`python3 stress/stress_phase1.py --size small --trials 10 --seed 0`

## Sizes
Use `--size small|medium|large|very_large`. The scripts define defaults for rows/nodes/edges per size.

