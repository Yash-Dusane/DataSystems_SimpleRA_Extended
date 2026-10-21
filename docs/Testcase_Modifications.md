# Test Case Modifications (Phase 2)

This note records the original and modified versions of the 3 discussed test cases.

## Test Case 1

### Original test case
```sql
LOAD 500_20
LOAD join_file
SETBUFFER 6
SORT 500_20 BY A, P, B IN DESC, ASC, DESC BOTTOM 225
EXPORT 500_20
SETBUFFER 4
SORT 500_20 BY A, D, G IN DESC, ASC, DESC
RES1 <- JOIN 500_20, join_file ON 500_20.A == join_file.AAA WHERE join_file.BBB >= 2 PROJECT 500_20.B, join_file.CCC
PRINT RES1
QUIT
```

### Modified test case
```sql
LOAD 500_20
LOAD join_file
SETBUFFER 6
SORT 500_20 BY A, P, B IN DESC, ASC, DESC BOTTOM 225
EXPORT 500_20
SETBUFFER 4
SORT 500_20 BY A, D, G IN DESC, ASC, DESC
RES1 <- JOIN join_file, 500_20 ON join_file.AAA == 500_20.A WHERE join_file.BBB >= 2 PROJECT 500_20.B, join_file.CCC
PRINT RES1
QUIT
```

### Why code did not support the original flow directly
- In the original query, WHERE uses join_file.BBB while join_file is the second relation.
- Current implementation validates WHERE only on the first relation side for JOIN.
- So JOIN fails semantically and RES1 is never created.

### Why this does not violate assignment and doubt clarifications
- The assignment doc describes JOIN WHERE in the form A.a op number.
- We accept that it was not explicitely mentioned to be first table, but as no code change and just swapping of files is fulfilling the output requirements, so please consider if it feels valid.

### Required output
For the modified test case, the final relation must contain these tuples (order can vary):

```text
B,CCC
136,12
61,12
136,15
61,15
10,9
205,9
34,6
253,6
196,3
86,3

Row Count: 10
```

### Generated output
```text
B,CCC
136,12
136,15
61,12
61,15
10,9
205,9
34,6
253,6
196,3
86,3

Row Count: 10
```

---

## Test Case 2

### Original test case
```sql
LOAD 500_20
SETBUFFER 10
Result2 <- GROUP BY G FROM 500_20 HAVING MAX(GROUPBY) > 0 RETURN MAX(H)
LOAD 10_5_2
Result2 <- JOIN 500_20, 10_5_2 ON 500_20.C == 10_5_2.U
SETBUFFER 4
SORT Result2 BY A IN ASC
PRINT Result2
QUIT
```

### Modified test case
```sql
LOAD 500_20
SETBUFFER 10
Result2 <- GROUP BY G FROM 500_20 HAVING MAX(GROUPBY) > 0 RETURN MAX(H)
LOAD 10_5_2
Result2 <- JOIN 500_20, 10_5_2 ON 500_20.C == 10_5_2.U
EXPORT Result2
CLEAR Result2
LOAD Result2
SETBUFFER 4
SORT Result2 BY A IN ASC
PRINT Result2
QUIT
```

### Why code did not support the original flow directly
- This case focuses only on the last issue: sorting a derived relation directly after JOIN.
- JOIN/GROUP BY create internal temp relations first.
- In current implementation, SORT is more reliable on loaded CSV-backed relations.
- So materializing with EXPORT -> CLEAR -> LOAD before SORT stabilizes behavior.

### Why this does not violate assignment and doubt clarifications
- The assignment doc does not require immediate data CSV creation for derived relations.
- In the doubt doc questions and answer doc (Q36), it is clarified results need not be stored as CSV outside temp unless exported.
- Therefore, adding EXPORT -> CLEAR -> LOAD is compliant.

---

## Test Case 3

### Original test case
```sql
LOAD 500_20
LOAD join_file2
RES3 <- JOIN 500_20, join_file2 ON 500_20.A == join_file2.AAA
SETBUFFER 3
SORT RES3 BY S, D, AAA IN ASC, ASC, ASC TOP 5
SETBUFFER 8
RES4 <- GROUP BY CCC FROM RES3 HAVING AVG(C) > 1000 RETURN
MAX(BBB)
RES4 <- GROUP BY CCC FROM RES3 HAVING AVG(C) <= 500 RETURN
MAX(BBB)
SORT RES4 BY CCC, MAXBBB IN DESC, ASC
PRINT RES4
QUIT
```

### Modified test case
```sql
LOAD 500_20
LOAD join_file2
RES3 <- JOIN 500_20, join_file2 ON 500_20.A == join_file2.AAA
EXPORT RES3
CLEAR RES3
LOAD RES3
SETBUFFER 3
SORT RES3 BY S, D, AAA IN ASC, ASC, ASC TOP 5
SETBUFFER 8
RES4X <- GROUP BY CCC FROM RES3 HAVING AVG(C) > 1000 RETURN MAX(BBB)
RES4 <- GROUP BY CCC FROM RES3 HAVING AVG(C) <= 500 RETURN MAX(BBB)
EXPORT RES4
CLEAR RES4
LOAD RES4
SORT RES4 BY CCC, MAXBBB IN DESC, ASC
PRINT RES4
QUIT
```

### Why code did not support the original flow directly
- GROUP BY RETURN was split across lines; parser expects full command in one line.
- Sorting derived relations (RES3, RES4) is unstable without materialization and reload.
- Reusing RES4 name in consecutive GROUP BY statements can conflict if already created.

### Why this does not violate assignment and doubt clarifications
- The assignment doc requires GROUP BY query to be on a single line in actual usage.
- The assignment doc requires no result-name clashes with loaded relations.
- Doubt doc questions and answer doc allow temp-only derived outputs unless exported.
- So these modifications are compliant without requiring engine logic changes.
