# Verifier: Phase 1 Graph Functionality (Regression Safety)

Phase 2 requirements explicitly state the new deliverables operate on **tables** (not graphs). This file records that Phase 1 graph commands remain operational and unchanged in behavior.

## Covered commands
- `LOAD GRAPH`
- `PRINT GRAPH`
- `EXPORT GRAPH`
- `DEGREE`
- `PATH`

## Evidence (sample run)
`samples/phase1_basic.ra`:
```
LOAD GRAPH UA U
DEGREE UA 1
RES <- PATH UA 1 2
```

Expected output (filtered) is stored in `samples/phase1_basic.expected.txt`.

## Implementation notes (why Phase 2 doesn't break Phase 1)
- Phase 2 commands are routed via new query types (`SETBUFFER`, `GROUP_BY`) and updated non-assignment parsing for `SORT`.
- Phase 1 graph commands still use the same data model (`Graph`, `GraphCatalogue`, node/edge tables). A few Phase 1 executor fixes were made to match the official Phase 1 testing output files and correct edge-cases.

### Phase 1 testing alignment fixes
These are correctness/formatting fixes for Phase 1 only; Phase 2 table features are unaffected.

- `LOAD GRAPH`: output formatting aligned with Phase 1 expected output files.
- `PRINT GRAPH`: prints node/edge rows from the original `/data/*.csv` sources to preserve the course’s expected formatting and row order.
- `DEGREE`: node-existence check no longer assumes the nodes file is sorted.
- `PATH`: result graph directedness matches the input graph; result graph CSV output is written using lines sourced from the input CSVs (preserves formatting) and nodes/edges are written in path order.

## Small complexity math (sanity)
These are sanity-level asymptotics, not strict graded I/O counts.

### DEGREE
Undirected:
- edges table is sorted by source; scan edges where `src == node` ⇒ reads the edge blocks spanning that range.

Directed:
- out-degree: range scan on sorted edges by `src`
- in-degree: full scan over all edges checking `dst == node`

### PATH
Uses a Dijkstra-style traversal (priority queue) with adjacency found by scanning the source-sorted edge table ranges.
