# Verifier (Math): `SETBUFFER` (Phase 2)

## Spec + clarifications to satisfy
From `questions.md` / `answers.md`:
- Q3: allowed to change the buffer limit (treat like an **environment variable**).
- Q4: can be executed **any time** during the session.
- Q5: if reduced below currently loaded pages, already-loaded pages may remain.
- Q7: update is just metadata (no disk I/O required at `SETBUFFER` time).
- Doc: default buffer size is **10** when `SETBUFFER` is not run at session start.

## Where this is implemented
- Default: `src/server.cpp:1` sets `BLOCK_COUNT = 10`.
- Parsing + semantics + execution: `src/executors/setbuffer.cpp:1`.
- Enforcement: `src/bufferManager.cpp:1` uses `BLOCK_COUNT` inside FIFO replacement in `insertIntoPool`.

## Formal model / spec
Let the program state be a tuple:
`State = (BLOCK_COUNT, Pool, Disk, Catalogue, ...)`
where `Pool` is the in-memory queue (FIFO) of cached pages and `Disk` is the on-disk `/data/temp` pages.

The intended denotation of the command `SETBUFFER k` is the state transformer:
`⟦SETBUFFER k⟧(State) = State'`
such that:
1) `State'.BLOCK_COUNT = k`
2) all other components are unchanged **immediately** (in particular: no required disk I/O and no forced evictions).

Additionally, per spec/tests in this repo, the accepted domain is:
`k ∈ {2,3,4,5,6,7,8,9,10}`.

## Theorems (what we prove)
### Theorem S1 (Semantic correctness)
If `semanticParseSETBUFFER()` accepts `k`, then executing `SETBUFFER k` produces a state where:
- `BLOCK_COUNT` equals `k`,
- no disk pages are read/written *because of* `SETBUFFER`,
- no pages are immediately evicted from the pool *because of* `SETBUFFER`.

### Theorem S2 (Deferred enforcement)
After `SETBUFFER k`, every subsequent buffer miss insertion maintains:
`|Pool| ≤ k`
by evicting FIFO pages **at insertion time** (not at `SETBUFFER` time).

## Proofs
### Proof of S1
1) **Domain / constraint adherence.**
   `semanticParseSETBUFFER()` enforces `2 ≤ k ≤ 10` and rejects outside values with `SEMANTIC ERROR` (`src/executors/setbuffer.cpp:1`).
2) **State update.**
   `executeSETBUFFER()` assigns `BLOCK_COUNT = k` (`src/executors/setbuffer.cpp:1`), therefore `State'.BLOCK_COUNT = k`.
3) **No forced pool eviction / no forced disk I/O.**
   `executeSETBUFFER()` calls `bufferManager.setMaxPages(k)`.
   `BufferManager::setMaxPages` is intentionally a no-op:
   it does not touch the pool deque and does not touch disk (`src/bufferManager.cpp:1`).
   Hence all non-`BLOCK_COUNT` components of `State` remain unchanged immediately after `SETBUFFER`.
This exactly matches the formal denotation above.

### Proof of S2
Consider `insertIntoPool(table, pageIndex)` in `BufferManager` (`src/bufferManager.cpp:1`).
Let `m = |Pool|` just before insertion of a new page.
The code executes:
`while (m ≥ BLOCK_COUNT) pop_front(); push_back(newPage);`
After the loop terminates, we have `m < BLOCK_COUNT`; then after `push_back`:
`|Pool'| = m + 1 ≤ BLOCK_COUNT`.
Since `BLOCK_COUNT = k` after `SETBUFFER k` (S1), we obtain `|Pool'| ≤ k`.
Thus enforcement is *deferred* to the moment of insertion, consistent with Q5/Q7.

## Notes / limitations (disproof conditions)
- If a different official spec allowed `k > 10`, then this implementation would violate that spec, because it rejects `k = 11` (see `Phase 2 Testing/tests/SETBUFFER_invalid.ra`).

So the new limit is respected without requiring a bulk “drop” algorithm at `SETBUFFER` time.
