# SimpleRA Phase 2 - Report

## 1. What We Built
In Phase 2, we implemented four table features on top of the existing page-based SimpleRA engine:

- `SETBUFFER`
- `SORT`
- `JOIN` (partition hash join)
- `GROUP BY`

The implementation follows the project flow exactly:

`Query -> Syntactic Parser -> Semantic Parser -> Executor`

and keeps all table processing page-oriented through `Cursor` and `BufferManager`.

All intermediate data is written in temp pages under `../data/temp/`.

## 2. Where The Code Is
- `SETBUFFER`: `../src/executors/setbuffer.cpp`
- `SORT`: `../src/executors/sort.cpp`
- External sort helper: `../src/phase2_external_sort.cpp`
- `JOIN`: `../src/executors/join.cpp`
- `GROUP BY`: `../src/executors/group_by.cpp`
- Shared Phase 2 parser helpers: `../src/phase2_parse_utils.cpp`

## 3. Command-Wise Implementation

### 3.1 SETBUFFER
#### Parsing and Semantics
- Syntax: `SETBUFFER K`
- Syntactic parser checks exactly 2 tokens and numeric `K`.
- Semantic parser enforces `2 <= K <= 10`.

#### Execution
- Updates global `BLOCK_COUNT` at runtime.
- Calls `BufferManager::setMaxPages(BLOCK_COUNT)`.

#### Current behavior and intent
- `SETBUFFER` can be called multiple times in the same session.
- Already-loaded pages are not force-evicted immediately when K changes.
- New page insertions naturally follow the current `BLOCK_COUNT` limit.

### 3.2 SORT
#### What syntax is supported
`SORT <table> BY <col1>, <col2>, ... IN <ASC|DESC>, <ASC|DESC>, ... [TOP X] [BOTTOM Y]`

#### Semantic checks handled
- Relation exists.
- All sort columns exist.
- `TOP` and `BOTTOM` values are valid (`0 <= value <= rowCount`).
- If both are present, `TOP + BOTTOM <= rowCount` (non-overlapping ranges).

#### Execution flow
1. Create a source snapshot table from `table->sourceFileName`.
2. Blockify that snapshot.
3. Build key specs from `BY ... IN ...`.
4. Sort only requested ranges via external sort:
   - Top segment (if present)
   - Bottom segment (if present)
5. Stitch final output:
   - sorted TOP + unchanged middle + sorted BOTTOM
6. Replace original temp pages of the relation in-place.

#### External sort details
- Run generation reads up to `BLOCK_COUNT * maxRowsPerBlock` rows at a time.
- Internal run sorting is handwritten stable merge sort.
- Merge phase is K-way style with fan-in `BLOCK_COUNT - 1` (clamped to at least 2).
- Heap tie-break by stream index preserves stability when keys are equal.

#### Important behavior
- SORT affects temp pages only.
- Original CSV in `/data` changes only after `EXPORT`.
- Every new SORT starts from `sourceFileName` snapshot logic in the executor.

### 3.3 JOIN (Partition Hash Join)
#### Supported forms
- Attribute equality: `A.a == B.b`
- Arithmetic equality:
  - `A.a + B.b == c`
  - `A.a - B.b == c`

#### Optional clauses
- `WHERE`: one clause, A-side only (`A.col op number`)
- `PROJECT`: one clause, any mix of A and B columns, output order follows project list

#### Semantic checks handled
- Result relation must not already exist.
- Both input relations must exist.
- Input relations must be different.
- Qualified columns must be valid.
- Join ON columns are enforced as table1-side then table2-side.
- WHERE column must belong to first relation.

#### Execution algorithm
1. Choose partition count: `P = BLOCK_COUNT - 1`, minimum 1.
2. Partition both relations into temp partition tables.
   - A-side WHERE filter is pushed into partition phase.
3. For each partition pair:
   - Build hash table on smaller partition.
   - Probe with larger partition.
   - Emit joined rows page by page.
4. Cleanup partition temp relations.

#### Empty result handling
- If no joined rows are produced, result relation is not created.

### 3.4 GROUP BY
#### Supported grammar
- Single or multi-result assignment:
  - `R <- GROUP BY g FROM T HAVING ... RETURN ...`
  - `R1, R2 <- GROUP BY g1, g2 FROM T HAVING ... RETURN agg1, agg2`
- Each grouping attribute is processed independently (as required).

#### Supported aggregates
- `MAX`, `MIN`, `SUM`, `COUNT`, `AVG`, `COUNT(*)`

#### HAVING forms handled
- `Agg(attr) op number`
- `Agg(attr) op Agg(attr)`

#### Semantic checks handled
- All result relation names are unique and currently unused.
- Source relation exists.
- Group-by columns exist.
- HAVING aggregate columns exist.
- RETURN aggregate columns exist.
- Number of result names, grouping attrs, and return aggregates must match.

#### Execution algorithm
For each `(resultName, groupCol, returnAgg)` pair:
1. Externally sort source relation on `groupCol`.
2. Single-pass scan grouped runs with aggregate state.
3. Evaluate HAVING once per group boundary.
4. Emit one result row per qualifying group.
5. If no groups qualify, do not create that result relation.

#### AVG behavior
- AVG is integer and rounded up for non-negative sums (as clarified in TA answers).

#### Overflow handling (current implementation)
- Aggregate values are computed in `long long` internally.
- Before writing a result value to an `int` row slot, range check is performed.
- If out of signed 32-bit range, system prints:
  `SEMANTIC ERROR: Integer overflow in GROUP BY aggregate`
  and does not create that result relation.

## 4. Cases and Edge Conditions Explicitly Handled

### General parser/runtime behavior
- Trailing semicolons are stripped globally by server token cleanup.

### SETBUFFER
- Invalid K (`<2`, `>10`, non-numeric) rejected.
- Runtime K change works between operations in same session.

### SORT
- Missing relation / missing column errors.
- Invalid TOP/BOTTOM syntax errors.
- TOP+BOTTOM overlap semantic error.
- `TOP 0` and `BOTTOM 0` supported.
- Stability preserved for equal keys.

### JOIN
- Missing relation/column errors.
- Invalid ON/WHERE/PROJECT forms rejected.
- WHERE pushed during partitioning.
- Empty join result not materialized.
- Arithmetic join normalization to A-key-space implemented.

### GROUP BY
- Duplicate result names rejected.
- Missing group/having/return columns rejected with specific semantic errors.
- Multi-group independent outputs in one query.
- Empty qualifying groups => relation not created.
- Aggregate overflow safe-fail behavior added.

## 7. Assumptions and Scope
- Table values are integer-based as per project flow and TA clarifications.
- GROUP BY aggregate outputs are assumed to fit in signed 32-bit integer for materialization.
- If not, overflow is explicitly handled as semantic error and result relation is skipped.
- For undocumented error messages not explicitly specified, generic syntax/semantic error style is followed.

## 8. Contributions
- Harpreet Singh (2022101048): SETBUFFER, SORT, HASH JOIN, GROUP BY, report
- Kunal Angadi (2022111005): SORT, verification and testing, report
- Yash Dusane (2022102078): SORT, verification and testing, report
