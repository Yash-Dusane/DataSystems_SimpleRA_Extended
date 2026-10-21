# Verifier (Math): `GROUP BY` (Phase 2)

## Spec + clarifications to satisfy
From `DS26_Project_Phase2.pdf` + `questions.md` / `answers.md`:
- Command can produce **multiple result tables** in one query.
  - Each grouping attribute is processed **independently**.
  - For `g` grouping attributes: there are `g` result tables and `g` return aggregates.
- Each result has exactly **two columns**:
  - the grouping attribute
  - the corresponding return aggregate value
- `HAVING` supports:
  1) `Agg(attr) op number`
  2) `Agg(attr) op Agg(attr)`
- Aggregates: `MAX`, `MIN`, `COUNT`, `SUM`, `AVG`, including `COUNT(*)`.
- `AVG` is stored as an integer by **rounding up** (ceil).
- If a particular grouping attribute yields **no qualifying groups**, that result table is **not created**.
- Expected to use buffers.

Relevant clarifications:
- Q1: expected to use the available buffers (avoid “just a map over rows” without respecting the model).
- Q2: `AVG` should be rounded up (stored back as an integer).

## Where this is implemented
- Parsing/semantics/execution: `src/executors/group_by.cpp:1`
- Aggregate term parsing + output naming: `src/phase2_parse_utils.cpp:1`
- Sorting for grouping: `src/phase2_external_sort.cpp:1`

## Formal model / spec
### Single grouping attribute
Fix:
- an input bag of rows `T`,
- a group column index `g`,
- a HAVING expression `H(G)` over the group,
- a return aggregate `Ret(G)`.

For each group value `v`, define the group bag:
`Gv = { r ∈ T : r[g] = v }`.

Define aggregate functions on a group `Gv`:
- `COUNT(*) = |Gv|`
- `SUM(c) = Σ_{r∈Gv} r[c]`
- `MIN(c) = min_{r∈Gv} r[c]`
- `MAX(c) = max_{r∈Gv} r[c]`
- `AVG(c) = ceil(SUM(c)/COUNT(*))` (per Q2; integer output)

The output relation (one row per qualifying group) is:
`Out = { (v, Ret(Gv)) : v is a group value and H(Gv) is true }`.

### Multiple grouping attributes in one query
If the query lists `g` grouping attributes, the spec says they are processed **independently**, producing `g` output tables.
So we apply the above definition once per grouping attribute.

## Theorems (what we prove)
### Theorem G1 (Functional correctness per grouping attribute)
For one grouping attribute, `executeSingleGroupBy` produces exactly `Out` as defined above (and does not create the result table when `Out` is empty).

### Theorem G2 (Multiple independent outputs)
For a query listing multiple grouping attributes, the executor produces each output table independently according to Theorem G1.

## Proofs
### Proof of G2
The parsed query stores equal-length lists:
`resultNames[i]`, `groupCols[i]`, `returnAggs[i]`.
Execution iterates `i=0..g−1` and calls the single-attribute procedure once per index.
Thus each result is exactly the single-attribute semantics applied to that attribute, independently.

### Proof of G1
Fix one call `executeSingleGroupBy(input, resultName, groupColumn, having..., returnAgg)` (`src/executors/group_by.cpp:1`).

**Step 1: Reduction to contiguous groups.**
The procedure computes:
`sorted = externalSortRangeToTempTable(..., keys=[groupColumn ASC])`.
By the correctness of external sort (see `verifier/SORT.md`), all rows with the same group value are contiguous in `sorted`.

**Step 2: Streaming aggregate invariant.**
The scan maintains:
- `groupRows` and aggregate states for the HAVING/RETURN terms.

Invariant I: after reading any prefix of the current group, the stored states equal the mathematical aggregates over exactly that prefix.
This holds by induction because `updateAggState` updates `(count,sum,min,max)` exactly as the definitions require.

**Step 3: Group finalization equals the spec.**
When a group ends, Invariant I implies the stored states equal aggregates over the full group bag `Gv`.
The code evaluates `H(Gv)` and, iff true, writes `(v, Ret(Gv))`.
Therefore the emitted set of rows is exactly `Out`.

**Step 4: AVG rounding (ceil).**
For `SUM=S ≥ 0` and `COUNT=n>0`, the code uses:
`ceil(S/n) = (S + n − 1) // n`,
which is a standard integer identity, matching Q2.

**Step 5: Empty-result rule.**
If no group emits a row (`Out` is empty), the result table is unloaded/deleted and not inserted into the catalogue, so it “does not exist” as required.

## Cost math (block I/O level)
Let:
- `B` = blocks in the input table
- `K` = `BLOCK_COUNT`
- For external sort: `R = ceil(B/K)`, `P = ceil(log_{K-1}(R))`

For one grouping attribute:
- external sort cost ≈ `2B * (1 + P)`
- one scan to aggregate groups: read `B` blocks
- write output: `B(out)`

Total per grouping attribute ≈ `2B*(1+P) + B + B(out)`.

If there are `g` grouping attributes, total cost is about `g` times that (each is processed independently per spec).

## Notes / limitations (disproof conditions)
- `HAVING` comparisons in the current code route through `evaluateBinOp(int,int,op)` after casting aggregates to `int`. If an aggregate value exceeds 32-bit signed range, the comparison can overflow; the proof holds under the implicit assignment-style bound “aggregate values fit in `int`”.
