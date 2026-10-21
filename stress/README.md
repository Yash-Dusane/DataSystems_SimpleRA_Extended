# Stress Testing

These scripts generate randomized test data, run `src/server`, and validate results for:
- Phase 1 graph operators (`LOAD GRAPH`, `DEGREE`, `PATH`)
- Phase 2 table operators (`SETBUFFER`, `SORT`, `JOIN`, `GROUP BY`)

## Run

Build once:
`cd src && make clean && make`

Phase 2:
`python3 stress/stress_phase2.py --size small --seed 0`

Hash JOIN scalability only (no SORT/GROUP BY):
`python3 stress/stress_hash_join_scale.py --rows-a 200000 --rows-b 150000 --buffers 10 --trials 1`

GROUP BY scalability only (MIN, COUNT(column), large SUM):
`python3 stress/stress_group_by_scale.py --rows 30000 --groups-g 150 --groups-z 101 --buffers 10 --trials 1`

Larger run example:
`python3 stress/stress_hash_join_scale.py --rows-a 1000000 --rows-b 750000 --buffers 10 --trials 1`

Phase 1:
`python3 stress/stress_phase1.py --size small --trials 10 --seed 0`

## Sizes

Use `--size small|medium|large|very_large`.

Phase 2 defaults (rows + buffers + trials) scale by `--size`; override with `--buffers` / `--trials` if needed.
