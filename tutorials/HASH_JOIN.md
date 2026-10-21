# `JOIN` (Phase 2 Partition Hash Join)

## Goal
Implements a **partition hash join** (equi-join only) with optional `WHERE` (filter) and `PROJECT` (projection).

## Syntax
`Result <- JOIN <A>, <B> ON <join-condition> [WHERE <A.col op number>] [PROJECT <attr-list>]`

Join conditions supported:
1. **Attribute equality**
   - `A.a == B.b`
2. **Arithmetic equality**
   - `A.a + B.b == number`
   - `A.a - B.b == number`

## Where it is implemented
- Parser/semantic/executor: `src/executors/join.cpp`

## How it works (part-by-part)
### 1) Parsing
- Reads `A` and `B` relation names.
- Parses the `ON` condition as either:
  - `A.a == B.b`, or
  - `A.a (+|-) B.b == constant`.
- Optionally parses:
  - `WHERE A.x <op> constant`
  - `PROJECT A.c1, B.c2, ...`

### 2) Semantic validation
- Both input tables must exist and must be different.
- Qualified columns must belong to the correct relation.
- All referenced columns must exist.

### 3) Partition phase (hash partitioning)
- Number of partitions: `P = BLOCK_COUNT - 1` (at least 1).
- Stream tuples from:
  - `A` (after applying `WHERE` if present)
  - `B`
- Compute a partition index from the join key and write tuples into partition tables:
  - `__hj_<Result>_A_p<i>`
  - `__hj_<Result>_B_p<i>`

Arithmetic join partitioning uses a key in **A’s join-key space**:
- `A.a + B.b == c` partitions `B` by `(c - B.b)`
- `A.a - B.b == c` partitions `B` by `(c + B.b)`

### 4) Join phase (build + probe per partition)
For each partition `i`:
1. Choose the smaller partition (by blocks) as the build side.
2. Build an in-memory hash table from the build partition.
3. Probe using the other partition.
4. Emit joined rows:
   - If `PROJECT` exists: emit only requested attributes, in the given order.
   - Else: emit all columns from `A` then all columns from `B`.

### 5) Empty-result rule
If no rows match, the result table is **not created** (it is not inserted into the catalogue).

