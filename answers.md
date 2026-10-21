# SimpleRA Phase 2 – Answers

1. You are expected to use the available buffers for this.

2. Round up.

3. **YES.** Treat it like an environment variable.

4. Any time during a session.

5. The number of buffers that can be stored in main memory is equal to the number of blocks that can be stored in main memory.  
   This is **2 in the default implementation**.

   This does **not affect pages that are already loaded** — they can remain as they are.

6. You can modify the existing `SORT`.

7. Yes.

8. Follow the algorithm in **Section 18.4.4** as mentioned in the document.

   - You **cannot assume that each partition will fit into a block during the partitioning phase**.
   - However, during the **joining phase**, you may assume that this condition holds.

   ![Partition Join Algorithm](attachment:648c1621-c24d-47bd-8af0-57bcea291782:image.png)

9. Updated the answer to **Question 8**.  
   The previous response misunderstood the question.

10. Use **temporary pages**.

11. Assume there will be **sufficient buffers to perform the sort**, so **no need to raise an exception**.

    `ASC` / `DESC` will be provided for **all columns**.

12. Yes. Always use the **buffer manager** to read pages in the **temp folder**.

13. As mentioned in the document:

    - Sorting **only affects the temp pages**.
    - It **does not modify the original CSV file** in the `/data` directory.

    Therefore, every `SORT` command **always starts from the original table stored in `/data`**, unless the sorted result has been explicitly **exported**.

14. That is correct. However, as you mentioned, since the wording can be misinterpreted, both approaches will be considered correct — i.e., re-sorting on the temp pages and re-sorting on the original CSV. Please mention what you have done in the report.

15. No.

16. It is a typo in the hash join and group by queries. However, if you have already implemented it with a semicolon, that will be considered correct too.

17. No.

18. No — everything needs to be implemented from scratch.

19. Yes.

20. No.

21. Anything works.

22. Yes, you are absolutely right. Please go ahead with using the current (flawed) implementation. We will **not** be conducting any page/block size checks during evaluation.

23. Use Cursor/Buffer Manager.

24. Yes.

25. Yes.

26. You can follow what the textbook suggests.

27. Always positive.

28. Modify `sort.cpp` according to your implementation. `SORT` is unimplemented in SimpleRA by default (only parsers are present, which you will need to modify according to the requirements).

29. No.

30. Yes.

31. Up to **10⁶ rows**, with expected completion within **5 minutes** per query.

32. Refer to the previous answer.

33. For all possible errors not mentioned explicitly in the document, you may output `SYNTAX ERROR`.

34. Say you have `A.csv` in the data folder. You load it into pages in the temp folder called `1, 2, 3` (for example). When you sort **A**, the sort must be in-place — i.e., the sorted table must be stored in pages `1, 2, 3` itself. Now if you sort **A** again, you may either choose to sort the original `A.csv` (which will not be modified by the first sorting) **or** you may sort the temp pages (`1, 2, 3`) of **A**. In either case, the output of the sorting must again be stored in pages `1, 2, 3` only.

35. As mentioned in the answer to **Question 24**, for the internal sorting algorithm you can use an in-memory data structure, but for the rest of the algorithm, you need to use temp pages.

36. It does not need to be stored as a CSV outside of temp unless it is exported.

37. Yes.

38. Nothing needs to be printed.

39. No.