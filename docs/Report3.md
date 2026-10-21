# SimpleRA Phase 3 - Report

## 1. What We Built

In Phase 3, we implemented transaction-level concurrency control as a **simulation** over SimpleRA's existing page-based storage engine. The new command:

`TRANSACTION <input_file>`

parses a schedule from `../data/<input_file>`, applies **Strict Two-Phase Locking (2PL)** with the **wait-die** deadlock prevention protocol at **page granularity**, and writes an execution log to:

`../data/<input_base>_output.txt`

For example: `test1.txt` -> `test1_output.txt`.

This implementation preserves the original project flow:

`Query -> Syntactic Parser -> Semantic Parser -> Executor`

and keeps page access within the existing `Cursor` -> `BufferManager` -> `Page` flow.

## 2. Concurrency Control Technique Chosen

We implemented **Strict 2PL + wait-die** because:

- It naturally fits page-level locking and the existing cursor/buffer-manager page access API.
- Strictness is straightforward to enforce because locks are released only at `COMMIT` or `ABORT`.
- wait-die prevents deadlocks without requiring a deadlock detector.

## 3. Implementation Details

### 3.1 New Command: TRANSACTION

- **Syntax**: `TRANSACTION schedule.txt`
- **Input location**: `../data/schedule.txt`
- **Output location**: `../data/schedule_output.txt`
- The schedule supports:
  - `BEGIN T1`
  - `READ T1 table_name page_number`
  - `WRITE T1 table_name page_number`
  - `COMMIT T1`

Semantic validation is performed before execution:

- If a referenced table is not currently loaded: `SEMANTIC ERROR: Relation doesn't exist`
- If a referenced page index is out of bounds: `SEMANTIC ERROR: Page doesn't exist`

### 3.2 LockManager

We introduced a `LockManager` that stores all lock metadata in memory at page granularity.

For each page, represented internally as `table|page`, the lock manager stores:

- Current lock type: `NONE`, `SHARED`, or `EXCLUSIVE`
- Owning transaction or transactions

For each transaction, the implementation stores:

- Timestamp assigned at first `BEGIN`
- Locks currently held by the transaction
- Lock acquisition order, used for deterministic unlock logging

Supported requests:

- Shared lock for `READ`
- Exclusive lock for `WRITE`
- Lock upgrade from `SHARED` to `EXCLUSIVE` when the transaction is the only shared owner

### 3.3 Wait-Die Protocol

On a lock conflict:

- If the requester is older than all conflicting lock owners, it waits.
- Otherwise, it dies: the system logs `ABORT` and `ROLLBACK`, releases all locks held by that transaction, and records it for restart.

The log includes the transaction that caused the wait/die decision. For example:

`T3 dies (younger than T1, cannot wait)`

### 3.4 Strict 2PL Enforcement

Locks are released only on:

- `COMMIT`, for normal completion
- `ABORT`, after a wait-die decision

No lock is released immediately after a read or write. This preserves strict 2PL behavior.

### 3.5 Restart Behavior

Aborted transactions are not restarted immediately at their original `COMMIT` operation. Instead:

- The transaction is marked aborted and its locks are released.
- The original schedule continues for all non-aborted transactions.
- After the non-aborted transactions finish, aborted transactions restart in timestamp order.
- On restart, the transaction's original `READ`/`WRITE`/`COMMIT` sequence is replayed.

### 3.6 Page Access and Simulation

Concurrency control is enforced before any transaction operation touches a page. After a lock is granted:

- `READ` uses `Cursor` to access the target page.
- `WRITE` is logged as a simulated write after acquiring an exclusive lock.
- Page contents are not physically modified for Phase 3.

Operations that arrive for a transaction while it is waiting are delayed and replayed in their original order after the waiting operation succeeds, so per-transaction schedule order is preserved.

## 4. Assumptions

- Concurrency control is simulated only; page contents are not logically modified.
- Page numbers in schedules refer to SimpleRA internal page indices: `0 .. blockCount - 1`.
- Tables referenced in the schedule are already loaded into the system before `TRANSACTION` is issued.
- Transaction timestamps are assigned at the first `BEGIN` in schedule order and do not change across restarts.
- Output format is a readable execution log including lock requests, grants, waits/dies, abort/rollback, restarts, commits, and unlocks.
- Transaction schedules are assumed valid, as clarified in the doubts document.
- Concurrency metadata is reset for each `TRANSACTION` command.

## 5. Contributions

- Harpreet Singh (2022101048): Strict 2PL design, wait-die logic, report support
- Kunal Angadi (2022111005): `TRANSACTION` command integration, semantic validation, testing
- Yash Dusane (2022102078): restart handling, edge-case testing
