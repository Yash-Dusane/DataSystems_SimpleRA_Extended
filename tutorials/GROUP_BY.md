# `GROUP BY` (Phase 2 Aggregates + HAVING)

## Goal
Implements per-attribute grouping with aggregates and a `HAVING` filter.

One command can produce **multiple result tables**: one per grouping attribute.

## Syntax
`R1, R2, ... <- GROUP BY g1, g2, ... FROM T HAVING <agg-expr> RETURN <agg1>, <agg2>, ...`

Rules:
- Each `gi` is processed **independently**.
- `HAVING` is applied independently per grouping attribute.
- `RETURN` must contain the same number of aggregates as grouping attributes.
- Each result table has exactly **two columns**: `<group-attr>` and `<return-aggregate>`.
- If a particular result table would be empty, it is **not created**.

## Where it is implemented
- Parser/semantic/executor: `src/executors/group_by.cpp`
- External sort helper: `src/phase2_external_sort.cpp`
- Aggregate token parsing: `src/phase2_parse_utils.cpp`

## HAVING forms
1. **Aggregate vs constant**
   - `HAVING MAX(Salary) > 10000`
2. **Aggregate vs aggregate**
   - `HAVING AVG(Salary) > AVG(Expenses)`

Supported aggregates:
`MAX`, `MIN`, `SUM`, `COUNT`, `AVG`, including `COUNT(*)`.

`AVG` is stored as an integer by **rounding up** (ceil).

## Implementation breakdown
For each grouping attribute `gi`:
1. **Sort the input** by `gi` (external sort, ascending) into a temp sorted table.
2. **Single scan** over the sorted table:
   - Identify group boundaries (when `gi` changes).
   - Maintain only O(1) aggregate state for the needed columns.
3. **Evaluate HAVING** at each group boundary.
4. **Emit (groupValue, returnAggregateValue)** into the output table.
5. If the output table has 0 rows, it is deleted and not inserted into the catalogue.

## Output column naming
- `COUNT(*)` → `COUNT`
- Otherwise: `<FUNC><Attribute>` (example: `MAXSalary`, `SUMExpenses`)

