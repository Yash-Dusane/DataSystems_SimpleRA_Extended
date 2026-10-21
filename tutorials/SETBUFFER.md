# `SETBUFFER` (Phase 2)

## Goal
`SETBUFFER K` sets the maximum number of page buffers (blocks) that the engine is allowed to keep in main memory for the rest of the session.

## Syntax
`SETBUFFER K`

Constraints:
- `2 <= K <= 10`
- Can be executed any time during a session.

## Where it is implemented
- Parser/executor: `src/executors/setbuffer.cpp`
- Buffer enforcement: `src/bufferManager.cpp`

## How it works (step-by-step)
1. **Syntactic parsing**
   - Validates the query has exactly 2 tokens: `SETBUFFER` and `K`.
   - Ensures `K` is a non-negative integer token.
2. **Semantic validation**
   - Ensures `K` is in `[2, 10]`.
3. **Execution**
   - Updates the global `BLOCK_COUNT` variable.
   - Does not force-evict already-loaded pages immediately (per clarifications).
   - Future page loads go through FIFO replacement with the new `BLOCK_COUNT` limit.

## Notes
- The global page pool is FIFO: once a new page must be loaded and the pool is full, the oldest page is evicted.

