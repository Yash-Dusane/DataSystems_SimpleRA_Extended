# Phase 2 Testing (Golden)

This folder contains **deterministic** `.ra` scripts for Phase 2 features:
- `SETBUFFER`
- `SORT` (stable + TOP/BOTTOM)
- `JOIN` (attribute + arithmetic, WHERE, PROJECT, empty-result behavior)
- `GROUP BY` (multi-result semantics, AVG rounding-up, empty-result behavior)

Expected outputs are stored in `outputs/*.expected.txt` in a **filtered** form (prompts + blank lines removed), matching `samples/filter_output.py`.

## Run

Build:
`cd src && make clean && make`

Run all tests:
`python3 "Phase 2 Testing/run_tests.py"`

Run a subset:
`python3 "Phase 2 Testing/run_tests.py" --pattern SORT_`

## Stress tests (optional)

Randomized correctness checks live in `stress/`:
`python3 stress/stress_phase2.py --size small --trials 5 --seed 0`
