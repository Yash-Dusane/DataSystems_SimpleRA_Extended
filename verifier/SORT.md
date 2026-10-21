# Verifier (Math): `SORT` (Phase 2)

## Spec + clarifications to satisfy
From `DS26_Project_Phase2.pdf` + `questions.md` / `answers.md`:
- Must implement **external sorting** via **k-way multi-pass merge sort**.
- Must be **stable**: if all sort keys tie, preserve original input order.
- Supports multiple keys with independent `ASC` / `DESC`.
- Supports optional `TOP X` / `BOTTOM Y`:
  - only those ranges are sorted
  - ranges must not overlap
  - rows outside remain in original relative order
- Sorting affects only **temp pages**; original `data/<T>.csv` is unchanged unless `EXPORT` is used.
- Every `SORT` should start from the **original table stored in `/data`**, unless the sorted result has been **exported**.
- Use **temporary pages** and always use the **buffer manager** to read temp runs.
- Assume sufficient buffers; no special exception behavior required.

Relevant clarifications:
- Q6: allowed to modify the existing `SORT`.
- Q10: use **temporary pages** (not purely in-memory structures for runs).
- Q11: assume sufficient buffers; no exception required; `ASC/DESC` provided for all columns.
- Q12: use buffer manager even for temp runs.
- Q13/Q14: each `SORT` starts from the original `/data` table unless explicitly exported.

## Where this is implemented
- Command parsing/semantics/execution: `src/executors/sort.cpp:1`
- External k-way merge helper: `src/phase2_external_sort.cpp:1`
- Buffer manager page I/O via cursors: `src/cursor.cpp:1` → `src/bufferManager.cpp:1`

## Formal model / spec
### Tables and row order
For `SORT`, a table is modeled as a **finite sequence** of rows:
`R = [r0, r1, ..., r(n−1)]`, where each row is a fixed-length tuple of integers.

Define the lexicographic key extractor for a key-list `K = [(c1,dir1),...,(cm,dirm)]` as:
`key_K(r) = (v1, v2, ..., vm)` where `vi = r[ci]` for `ASC` and `vi = -r[ci]` for `DESC` (any monotone transformation works; this one matches the Python oracle used in stress).

Define `stableSort(R,K)` as the unique sequence `R'` such that:
1) `R'` is a permutation of `R` (same multiset of rows),
2) `R'` is non-decreasing by `key_K`,
3) if `key_K(ri) = key_K(rj)` and `i < j`, then `ri` appears before `rj` in `R'` (stability).

### TOP/BOTTOM semantics (from the doc)
Let `x` = TOP, `y` = BOTTOM (each may be absent).
- If neither TOP nor BOTTOM: output is `stableSort(R,K)`.
- Else output is:
  `stableSort(R[0:x],K) ++ R[x:n−y] ++ stableSort(R[n−y:n],K)`
and the constraint `x + y ≤ n` must hold.

### Restart semantics (answers.md Q13/Q14)
Let `Disk(T)` be the CSV currently in `/data` for relation `T`.
Then each `SORT T ...` must read from `Disk(T)` (not from previous temp pages), unless a prior `EXPORT T` updated `Disk(T)`.

## Theorems (what we prove)
### Theorem So1 (External sort produces stableSort)
For any input range `R[start : start+N]` and key-list `K`,
`externalSortRangeToTempTable(..., start, N, K)` returns a temp table whose row-sequence equals `stableSort(R[start:start+N], K)`.

### Theorem So2 (TOP/BOTTOM correctness)
`executeSORT` produces exactly the TOP/BOTTOM semantics described above, and rejects overlaps (`x+y>n`) with `SEMANTIC ERROR`.

### Theorem So3 (Restart correctness)
If `SORT` is run multiple times on `T` without an intervening `EXPORT T`, each `SORT` reads from the same disk snapshot `Disk(T)`.
If an `EXPORT T` occurs, the next `SORT` reads from the exported `Disk(T)`, matching the clarification “unless exported”.

## Proofs
### Proof of So1
We prove correctness of `externalSortRangeToTempTable` (`src/phase2_external_sort.cpp:1`) by run-generation and merge invariants.

**Lemma L1 (Run generation produces stable sorted runs).**
In each run, the algorithm reads a contiguous chunk of the input range in original order, then applies a handwritten stable merge sort using the key comparator.
Therefore each run’s output sequence equals `stableSort(chunk,K)`.
So within a run, relative order of key-ties matches original input order.

**Lemma L2 (k-way merge preserves stability across runs).**
During merging, the priority queue orders items by key, and for key-ties it orders by `streamIndex` (smaller streamIndex wins).
Since run streams are merged in increasing base order, `streamIndex` corresponds to earlier runs, which correspond to earlier positions in the original input.
Thus, among equal-key rows originating from different runs, rows from earlier runs are emitted first—exactly the stability requirement for the concatenated original sequence.

**Lemma L3 (One merge pass outputs stableSort of the concatenated input runs).**
Given sorted input runs, repeated “pop-min then push-next-from-same-stream” is the standard k-way merge algorithm.
By L2 tie-breaking, the merge is stable.
Therefore, one pass outputs `stableSort(concat(runs),K)`.

Applying L1 to initial runs and L3 repeatedly over merge passes yields that the final single run equals `stableSort(range,K)`.
Hence So1 holds.

### Proof of So2
`executeSORT` (`src/executors/sort.cpp:1`) constructs a source snapshot table `src` from `table->sourceFileName`, and defines:
- `topCount = x` (or `n` if no TOP/BOTTOM),
- `bottomCount = y`,
- `bottomStart = n − y`.

It computes:
- `sortedTop = externalSortRangeToTempTable(..., 0, x, K)` if `x>0`,
- `sortedBottom = externalSortRangeToTempTable(..., n−y, y, K)` if `y>0`.

Then it emits exactly `n` rows in index order `i=0..n−1`:
- if `i < x`, emit next from `sortedTop`,
- else if `i ≥ n−y`, emit next from `sortedBottom`,
- else emit the original row from `src`.

Therefore the emitted sequence is:
`stableSort(R[0:x],K) ++ R[x:n−y] ++ stableSort(R[n−y:n],K)`.

The non-overlap requirement `x+y ≤ n` is checked in `semanticParseSORT` (`src/executors/sort.cpp:1`), so overlapping ranges are rejected.
Thus So2 holds.

### Proof of So3
The key observation is that `executeSORT` snapshots from `table->sourceFileName` by copying the file contents into a fresh temp table.
The `SORT` executor does not modify `table->sourceFileName`; it only swaps temp pages for the in-memory representation.
Therefore, without `EXPORT`, repeated `SORT` operations snapshot from the same disk file `Disk(T)`.
After `EXPORT T`, `Table::makePermanent()` overwrites `/data/T.csv`, so `Disk(T)` changes; subsequent sorts use that updated disk snapshot.
This matches Q13/Q14 exactly.

## Cost math (block I/O level)
Let:
- `B` = number of blocks (pages) in the sorted range
- `K` = `BLOCK_COUNT` buffers
- `R = ceil(B / K)` initial runs
- Fan-in per pass = `K - 1`
- `P = ceil(log_{K-1}(R))` merge passes

Then the classic external-sort I/O is approximately:
- Run generation: read `B` + write `B` = `2B`
- Each merge pass: read `B` + write `B` = `2B`

Total ≈ `2B * (1 + P)` block reads/writes (excluding final “swap pages” renames which do not add block reads).

For `TOP`/`BOTTOM`, substitute `B` with the blocks covered by that segment.

## Notes / limitations (disproof conditions)
- All comparisons are performed on `int` values; if table values exceed the representable range of `int`, the model “integers” is violated by the language type, so no full proof is possible without additional bounds.
