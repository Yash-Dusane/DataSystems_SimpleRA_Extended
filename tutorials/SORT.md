# `SORT` (Phase 2 External Sort)

## Goal
`SORT` reorders a relation according to one or more keys using an **external, stable, k-way multi-pass merge sort**.

Only the **temp pages** are affected during the session. The original CSV in `data/` is only updated if you `EXPORT` the table after sorting.

## Syntax
`SORT <table> BY <col1>, <col2>, ... IN <ASC|DESC>, <ASC|DESC>, ... [TOP X] [BOTTOM Y]`

## Where it is implemented
- Parser/semantic/executor: `src/executors/sort.cpp`
- External sort helper: `src/phase2_external_sort.cpp`

## Core idea (part-by-part)
### 1) Key parsing
- Read the column list between `BY` and `IN`.
- Read the strategy list after `IN`.
- Enforce: `#columns == #strategies`.

### 2) TOP/BOTTOM partial sorting
- `TOP X`: sort only the first `X` rows.
- `BOTTOM Y`: sort only the last `Y` rows.
- If both are present, the ranges are independent and must not overlap.
- Rows outside these ranges keep their original relative order.

### 3) Stability (ties keep input order)
Stability is achieved by:
- Creating initial runs by reading input rows in order and using a handwritten stable merge sort.
- Using a stable k-way merge: when keys compare equal, the earlier input stream is chosen first.

### 4) External sort (k-way merge)
Implemented in `externalSortRangeToTempTable(...)`:
1. **Run generation**
   - Read up to `K` pages worth of rows into memory.
   - Sort them with a handwritten stable merge sort by the given key list.
   - Write them out as a temp “run” table.
2. **Multi-pass merging**
   - Merge up to `K-1` runs at a time into a new run table (1 output stream + `K-1` input streams).
   - Repeat until a single run remains.

### 5) Applying the sort to the base relation
`executeSORT()`:
- Builds a snapshot of the relation from `sourceFileName` to ensure each `SORT` starts from the original table file unless it was exported.
- Sorts the `TOP` and/or `BOTTOM` segments into temp sorted tables.
- Streams the final output into a temp output table’s pages.
- Swaps temp output pages into `<table>_Page*` filenames and updates the in-memory table metadata.

## Quick example
`SORT A BY b IN DESC TOP 3`
- Reorders only the first 3 rows of `A` by `b` descending.
- Leaves the rest exactly as-is.

