# SimpleRA Phase 2 – Questions

## Please add your questions here

1. For implementation of **GROUP BY**, can we use a `map` to store the distinct values of the group-by column and the corresponding aggregation result while iterating through the rows of the table?

2. The `AVG` aggregate in **GROUP BY** can result in a floating-point value. Do we need to round it off, since the result will be stored in a table again?

3. For implementing `SETBUFFER`, are we allowed to change the `BLOCK_COUNT` constant, since the default value should be **10** as specified in the document?

4. Will `SETBUFFER` run only at the **start of the session**, or can it be executed **at any time during the session**?

5. If the new value of **#Blocks in main memory** is set to less than the current number of blocks in main memory, what should happen to the remaining pages?  
   Do we follow any particular algorithm for dropping extra blocks?

6. Should we **modify the existing `SORT` implementation**, or implement another functionality with the same name?

7. Follow-up on **Question 5**:  
   Just to confirm — does `SETBUFFER` simply update the number of buffers/blocks **without any disk I/O**?

8. For **Hash Join implementation**, can we assume that the resulting table partition obtained in one pass using a hash function can fit each partition in **k−1 blocks**?  
   Or are we expected to perform **multi-pass partitioning with multiple hash functions** until one partition fits within **k−1 blocks**?

9. Follow-up on **Question 8** regarding partitions that exceed the `BLOCK_COUNT` limit:  
   How should these be handled? Should we:
   - Rehash the entire partition again to reduce partition size, or
   - Load the partition block by block and perform the join?

   The textbook algorithm assumes that the partition fits in main memory, but the answer to **Q8** suggests the partition can exceed the **k−1 block limit**.

---

**Note from TAs:**  
Q8 has been clarified. Please **disregard Q9**.

---

10. When sorting each page, can we use **temporary pages**, or should we store the data in an **in-memory data structure**?

11. For the sorting algorithm, if there are only **2 blocks available**, should we:
   - Sort within the page, or
   - Raise an exception?

   Also, can we assume that the sorting method (`ASC` | `DESC`) will always be provided for all columns, or should the default be **ASC**?

12. Do we have to use the **buffer manager** for reading the **temporary runs** during `SORT` as well?

13. If I sort a table **A** (all rows ascending on a given key), and then sort **A** descending on the same key with **TOP 3**, am I sorting:
   - The **original table A again**, or
   - The **previously sorted table**?

   Essentially, does sorting operate on the **main memory table each time**, or on the **disk table**?

14. Clarification to **Question 13**:  
   I did not mean the main memory table. I meant the **temporary representation stored in temp pages** from the previous sort.

   If the table is **A**, and I store a sorted permutation **A′** in temp pages, will the next sort operate on **A′** or on **A** again?

   - **Adding onto Questions 13 and 14:** Suppose I sort a table **A** but I don't export it out to main memory yet (let this resultant table be **A′** stored in pages). Now, suppose I'm asked to sort **A** again. This would require me to read **A**, blockify it again separately, and sort those pages of **A**. **Is this correct?** (This is implied by your answer to the above 2 questions — I must delete the old pages of **A**, i.e., **A′** must be purged, since they have been superseded by a newer version as a result of this SORT command.) Please note that the original requirements do **not** mention this; all they say is the following, which implies that we can re-sort the already sorted pages and we don't have to re-load the table just to sort.

     > The command must perform in-place sorting. It should not perform an export, i.e., when you sort a table, the original CSV file of the table in the `/data` directory stays the same. It is updated only if you export the sorted table. Only the temp pages are affected by sorting.

15. Are we supposed to handle other datatypes as well? In the example of sorting in the document there are string fields?

16. Is the trailing semicolon that's a part of the JOIN query supposed to be there, or is it just a typo?

17. Is there a particular order of output groups produced by **GROUP BY** that we're supposed to follow? If yes, what is it? (The doc doesn't specify this.)

18. For sorting, can we use inbuilt functions to sort individual parts?

19. Adding to **Question 15**: Will the datatypes of the table be **only `int`**? The original `LOAD TABLE` is written only for `int` type?

20. Adding to **Question 18**: In the sort phase, for each individual page, can we use inbuilt in-memory sorting algorithms?

21. Are we supposed to change the default value of `BLOCK_COUNT` from **2 to 10**, or use a new variable for `SETBUFFER`?

22. What should we assume the maximum value that any cell can hold? (Can we assume it fits in `int`?)  
    The present `LOAD TABLE` does this:  
    `this->maxRowsPerBlock = (uint)((BLOCK_SIZE * 1000) / (sizeof(int) * this->columnCount));`  
    But pages are written as ASCII text — so for a row like `145456 123456 123578`, each row takes `6×3 + 2×1 (spaces) + 1 (\n)` characters. This would mean we can store `1000/21` rows, as opposed to `maxRowsPerBlock = 1000/(4×3)`. Should we go ahead with the present (flawed) implementation of `LOAD`, which violates the page size?

23. Can we use streams to read data from intermediary pages during the merge phase of the K-Way Merge Sort implementation?

24. For sort, in the sorting phase (according to the book), we should use an internal sorting algorithm. Are we allowed to store the rows of **K blocks** in an in-memory data structure (an array) and sort them by implementing a from-scratch merge-sort/quick-sort, which we later store in blocks as a subfile?

25. In the JOIN functionality, can we assume that `WHERE` / `ON` conditions will only be between **two columns** and not multiple sets of columns?

26. For JOIN, can we build an **in-memory hash table** for the lookup on the smaller partition (similar to what the textbook suggests), or are we supposed to scan the entire partition using a cursor for each lookup?

27. For sort, will the `TOP X` and `BOTTOM Y` values always be **positive**, or can they be negative as well?

28. For sort, does `sort.cpp` need to be **modified completely**, or does the initial implementation also need to be there?

29. Can we just use `std::sort` for sorting the rows of **one block** in memory (just for building the initial sorted runs)?

30. Can we assume the comma-separated tokens are separated with exactly `'<comma><space>'`?  
    (e.g., `SORT A, B` instead of `SORT A , B` or other possibilities)

31. What is the **maximum number of rows** that will be tested upon, and the expected completion time for each query?

32. What are the **size constraints** on the tables which will be evaluated (including row count, number of attributes etc.) for each command?

33. Is an incorrect type of operator or aggregate function a **syntax error**? Is it fine to show the text `"SYNTAX ERROR"`?

34. Following up on **Question 14**: Say table **A** is loaded. After performing sort, are we sorting the original pages of table **A** (as it's mentioned to do in-place sorting)? So, if we do multiple sorts in a session on Table **A**, will we keep sorting those original pages over and over, or are we creating temp pages for each sort call — which will contain the sorted order of **A** according to the sort conditions given in that instance — but with each sort call using the original pages of **A** created when loading the table? Asking this because the answer will be different in both cases.

---

35. Following up on **Question 10** and **Question 24**, both are given conflicting answers. Can you please clarify again: are we allowed to store the rows of **K blocks** in an in-memory data structure (an array) and sort them by implementing a from-scratch merge-sort/quick-sort, or not?

---

36. Should the resultant tables of **HASH JOIN** and **GROUP BY** commands be stored as CSV files as well?

37. Similar to **Q11**, for JOIN as well, can we assume that **K** (num buffers) must be at least 3? Else, everything would be put into a single bucket, which defeats the purpose of hashing.

38. If a join is **empty**, do we have to print anything? Would this affect testing of mixed cases as given for Phase 1?

39. Will the **order** in which the rows appear in the result table after a **GROUP BY** or **JOIN** operation matter during evaluation?

40. In **GROUP BY**, the `HAVING` clause constant value comparison states the number must be `0 ≤ number ≤ 10000`. However, the example query provided uses `HAVING AVG(Salary) > 30000`. Please clarify: is the 10,000 limit a strict syntax constraint, or does the example indicate that larger constants are actually permitted?