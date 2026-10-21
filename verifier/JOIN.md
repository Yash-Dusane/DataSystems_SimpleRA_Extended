# Verifier (Math): `JOIN` (Phase 2 Hash Join)

## Spec + clarifications to satisfy
From `DS26_Project_Phase2.pdf` + `questions.md` / `answers.md`:
- Join mechanism must be **partition hash join** (primary join mechanism).
- Join is an **equi-join**:
  1) attribute equality: `A.a == B.b`
  2) arithmetic equality: `A.a (+|-) B.b == number`
- Optional `WHERE` clause:
  - at most one WHERE
  - form: `A.x op number` (only from table A)
- Optional `PROJECT` clause:
  - at most one PROJECT
  - attributes can come from either table
  - output order follows PROJECT list
- If no rows satisfy join/where: **result table must not be created**
- Q8 clarification (`answers.md`): follow the partition join algorithm (cannot assume partitions fit during partitioning; may assume build-side fits during joining). Note in `questions.md`: Q9 is to be disregarded.

## Where this is implemented
- Parsing/semantics/execution: `src/executors/join.cpp:1`
- Qualified-column parsing: `src/phase2_parse_utils.cpp:1`

## Formal model / spec
### Bag semantics
For `JOIN`, the natural model is **bag / multiset semantics** (duplicates preserved), because nothing in the Phase 2 spec states duplicate elimination.

Let `A` and `B` be bags of rows.
Let `⊕` denote row concatenation.
Let `cond(a,b)` be either:
1) attribute equality: `a[aCol] = b[bCol]`, or
2) arithmetic equality:
   - PLUS: `a[aCol] + b[bCol] = c`
   - MINUS: `a[aCol] − b[bCol] = c`

Let the optional WHERE predicate be `pred(a)` of the form `a[wCol] op k` and **only on A** (per spec).

Define the join result bag as:
`A ⋈ B = { a ⊕ b : a∈A, b∈B, pred(a) ∧ cond(a,b) }`
and if PROJECT is present, apply projection to each output row, preserving multiplicity.

## Theorems (what we prove)
### Theorem J1 (Functional correctness)
If the command parses/semantically validates, the executor outputs exactly the bag defined above (with optional PROJECT), and it does not create a result table when the bag is empty.

## Proofs
We prove J1 by decomposing the implementation into partitioning + per-partition hash join, and showing equivalence to the specification.

### Lemma J1.1 (Key normalization is correct)
Define the “A-space key” used in the implementation (`joinKeyInASpace` in `src/executors/join.cpp:1`):
- for A rows: `KA(a) = a[aCol]`
- for B rows:
  - ATTR: `KB(b) = b[bCol]`
  - PLUS: `KB(b) = c − b[bCol]`
  - MINUS: `KB(b) = c + b[bCol]`

Claim: for all rows `a∈A`, `b∈B`, `cond(a,b)` holds **iff** `KA(a) = KB(b)`.

Proof:
- ATTR: `cond(a,b)` is exactly `a[aCol] = b[bCol]`, so `KA(a)=KB(b)`.
- PLUS: `a[aCol] + b[bCol] = c  ⇔  a[aCol] = c − b[bCol]  ⇔  KA(a)=KB(b)`.
- MINUS: `a[aCol] − b[bCol] = c  ⇔  a[aCol] = c + b[bCol]  ⇔  KA(a)=KB(b)`.

### Lemma J1.2 (Partitioning preserves joinability)
Let `P = BLOCK_COUNT − 1` (clamped to ≥1) and `p(x) = (x mod P)` (with the standard non-negative correction used in the code).
The partitioning phase writes each eligible A row to partition `p(KA(a))` and each B row to partition `p(KB(b))`.

By Lemma J1.1, for any matching pair `(a,b)` we have `KA(a)=KB(b)`, hence `p(KA(a)) = p(KB(b))`.
Therefore every matching pair appears together in the **same** partition index.

Conversely, if two rows land in different partition indices, they cannot satisfy `KA=KB`, hence cannot satisfy `cond`.

### Lemma J1.3 (WHERE correctness)
If WHERE is present, the implementation filters A rows *before* partitioning and joining (`partitionRelation` checks `hasWhere` only for `isA=true`).
Thus:
- no row failing `pred(a)` can appear in any partition,
- every emitted join row satisfies `pred(a)` by construction.

This matches the spec “WHERE applies only to A”.

### Lemma J1.4 (Per-partition hash join emits exactly matching pairs)
Fix a partition index `i`.
The executor:
1) chooses a build side (smaller by blockCount),
2) builds a hash table mapping `K = KA(row)` or `KB(row)` to the list of build rows with that key,
3) scans the probe side; for each probe row with key `K`, emits `(buildRow, probeRow)` for every buildRow in the bucket.

By Lemma J1.1, emitting for equal normalized keys is equivalent to emitting for `cond(a,b)`.
All emitted pairs are matching (no false positives), and all matching pairs within the partition are emitted (no false negatives), including duplicates (because buckets store all rows).

### Lemma J1.5 (Union across partitions equals full join)
By Lemma J1.2, every matching pair appears in exactly one partition index (the one determined by its normalized key).
By Lemma J1.4, that partition emits it.
So the multiset union over partitions equals the full join bag.

### Lemma J1.6 (PROJECT correctness)
If PROJECT is absent, the executor outputs `a ⊕ b` (all columns of A then all columns of B).
If PROJECT is present, it outputs exactly the listed attributes in order by selecting the corresponding column values from the appropriate side.
This is a standard projection homomorphism on rows and preserves multiplicity.

### Lemma J1.7 (Empty-result rule)
If no rows are emitted, the executor unloads and deletes the result table object and does not insert it into the catalogue (`src/executors/join.cpp:1`).
Therefore `JOIN` does not create a result relation when the join bag is empty, matching the requirement.

Combining Lemmas J1.2–J1.7 proves Theorem J1.

## Cost math (block I/O level)
Let:
- `B(A')` = blocks of A after WHERE filtering (if any)
- `B(B)` = blocks of B
- `B(out)` = output blocks

Partition phase:
- Read + write both relations into partitions: `2 * (B(A') + B(B))`

Join phase:
- Read partitions once more to build/probe: `B(A') + B(B)`
- Write output: `B(out)`

Total ≈ `3 * (B(A') + B(B)) + B(out)` block reads/writes.

## Notes / limitations (disproof conditions)
- The proof is about **functional output correctness**. Resource assumptions (e.g., “build-side partition fits in k−1 blocks”) affect whether the join finishes within memory/time, not whether the algorithm is logically correct when it runs to completion.
