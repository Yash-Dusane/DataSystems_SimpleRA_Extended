Here is the markdown formatted version of the document you provided, with all the necessary citations included.

Project Phase 2 

**Data Systems Spring 2026** **Due Date:** 23:59 Hrs, 12th March, 2026 

1 Instructions 

* Teams remain the same as in Phase 1. 


* This phase builds upon the codebase created in Phase 1. 


* All commits for this phase are to be done in the main branch of the repository, which is added to your team on GitHub Classroom. You may create separate branches, but you are supposed to merge them into the main branch by the due date. 


* No personal queries will be entertained. All queries regarding the project must be posted in the doubts document. Only doubts clarified in the doubts document will be considered for grading. 


* The late days form must be filled on the day of submission if any commit is made past the due date in the main branch. You must provide the correct number of late days taken. 


* Plagiarism checks will be conducted on all submissions. Copied code, whether partially or fully, will be heavily penalized. 


* Not following any of the instructions will result in a penalty. 



Important Constraints and Implementation Notes 

* The deliverables in this phase take tables as input, and not graphs. 


* The overall program flow must not be modified. The reference flow can be found in docs/flow.png. 


* The cursor and bufferManager classes must be used for handling reading and writing to the pages. 


* Private variables of any class must not be changed to public. Access should be provided only through appropriate getter and setter methods. 


* The values of BLOCK SIZE and BLOCK COUNT must remain unchanged. 


* The implemented commands should run in a reasonable amount of time. 


* Any additional changes or extensions must not affect the correctness or behavior of already implemented table/graph queries. 



---

2 SETBUFFER K 

* Sets the number of buffers (number of blocks that can be stored in main memory) for the session. 


* If the command is not run at the beginning of the session, the default number will be 10. 



Syntax 

`SETBUFFER K` 

* 2 < K < 10. 


* Upon successful execution of this command, all further commands must not use more than K blocks. 


* The command can be called multiple times during a session. Each execution of the command should modify the buffer size. 



---

3 SORT 

* Extend SimpleRA to implement external sorting using the K-way Multi-Phase Merge Sort Algorithm detailed in section 18.2, Elmasri and Navathe. 


* The command must perform in-place sorting. It should not perform an export, i.e., when you sort a table, the original CSV file of the table in the/data directory stays the same.  It is updated only if you export the sorted table. Only the temp pages are affected by sorting. 


* The tables may contain duplicate rows. 



Syntax 

`SORT <table-name> BY <col1>, <col2>, <col3> IN <ASC DESC>, <ASC|DESC>, <ASC|DESC> [TOP X] [BOTTOM Y]` 

* The order of sort keys specified in the query determines the priority of comparison. This means that the first column has the highest priority. If two rows are equal on this column, the next column is used for comparison, and so on. 


* If two rows are equal on all sort keys, their original order in the input table must be preserved. 


* Any number of columns can be included. 



TOP and BOTTOM Clauses 

* The brackets around TOP/BOTTOM are only to indicate that these clauses are optional in the syntax description. The actual query will not contain brackets. 


* TOP X means only the first X rows of the table must be sorted. 


* BOTTOM Y means only the last Y rows of the table must be sorted. 


* When both TOP X and BOTTOM Y are specified, the TOP X rows and the BOTTOM Y rows must be sorted independently. The ranges defined by TOP X and BOTTOM Y will never overlap. 


* Rows that do not fall within the TOP X or BOTTOM Y range must remain in their original order in the table. 


* The values X and Y will always be less than or equal to the total number of rows in the input table. 



Error Handling 

* If the specified table does not exist, an error must be thrown - SEMANTIC ERROR: Relation doesn't exist 


* If any column mentioned in the query does not exist in the specified table, an error must be thrown SEMANTIC ERROR: Column doesn't exist in relation 



Examples 

**Example 1** 

*Table A* The following table: 

| EmpID | Name | Dept |
| --- | --- | --- |
| 1 | Alice | 10 |
| 2 | Bob | 20 |
| 3 | Carol | 10 |
| 4 | Dave | 25 |

*Query* `SORT A BY Dept IN ASC` 

*Result (Table A after query)* The following table: 

| EmpID | Name | Dept |
| --- | --- | --- |
| 1 | Alice | 10 |
| 3 | Carol | 10 |
| 2 | Bob | 20 |
| 4 | Dave | 25 |

*Explanation* Table A is sorted in ascending order of Dept while preserving the original order of rows with equal Dept values. 

**Example 2** 

*Table A* The following table: 

| EmpID | Name | Dept | Salary |
| --- | --- | --- | --- |
| 1 | Alice | 15 | 50000 |
| 2 | Bob | 20 | 100000 |
| 3 | Carol | 16 | 50000 |
| 4 | Dave | 25 | 20000 |
| 5 | Elizabeth | 15 | 80000 |

*Query* `SORT A BY Salary, Dept IN DESC, ASC TOP 3` 

*Result (Table A after query)* The following table: 

| EmpID | Name | Dept | Salary |
| --- | --- | --- | --- |
| 2 | Bob | 20 | 100000 |
| 1 | Alice | 15 | 50000 |
| 3 | Carol | 16 | 50000 |
| 4 | Dave | 25 | 20000 |
| 5 | Elizabeth | 15 | 80000 |

*Explanation* The first 3 rows according to (Salary DESC, Dept ASC) are sorted while the remaining rows retain their original relative order. It is first sorted in decreasing order of Salary, and ascending order of Dept when two rows have equal Salary. 

---

4 HASH JOIN 

* Extend SimpleRA to implement partition-hash join using the algorithm detailed in section 18.4.4, Elmasri and Navathe. 


* The command must use hashing as the primary join mechanism and must perform an equi-join. 


* Nested-loop or sort-merge joins are not permitted. 


* In all the examples given below, X. y refers to column y in table X 



Syntax 

`Result <- JOIN <table1>, <table2> ON <join-condition> [WHERE <selection-condition>] [PROJECT <attribute-list>];` 

* Result should store the joined output of table1 and table2. 


* The two input tables will always be different. 


* Both tables will have distinct column names. 


* The name of the resultant table will not match the name of any table already loaded in the system. 


* Brackets around WHERE and PROJECT are used only to indicate that these clauses are optional in the syntax description and will not appear in the actual query. 



Explanation of Syntax 

**ON ** The join condition may be one of the following two types: 

* Attribute Equality 


* Form: A.a==B.1 


* Rows are joined when the value of column a in table A is equal to the value of column b in table B. 


* Example: A.id==B.id 




* Arithmetic Expression 


* Form: A.a arith B.b== number 


* The arithmetic expression is evaluated first, and the result is then compared with number. 


* arith can be + or 


* number satisfies 0<= number <=10000. 


* Example: A.marks + B.bonus ==80 





**WHERE ** 

* The selection condition specifies which rows must be included in the Result table. 


* At most one WHERE clause may appear in a query. 


* There is one type of condition: A. a op number. op can be ==, !=, >, >=, <, <=. 


* There will only be one condition per WHERE clause in a query. 


* Example: WHERE A.salary > 50000 This condition filters the result to include only those rows where the value of salary in table A is greater than 50000. 



**PROJECT ** 

* The PROJECT  clause specifies which columns must be included in the Result table. 


* At most one PROJECT clause may appear in a query. 


* 
 may include any number of columns from either of the two input tables. 


* The order of the columns in the Result table must follow the order specified in the PROJECT clause.. 


* In case there is no PROJECT clause, then all columns from both the tables should be returned. The order of the columns must be columns from the first table followed by columns from the second table and they should be named the same as the columns from the respective tables. 


* Example: PROJECT A.name, B.salary This projects only the name column from table A and the salary column from table B into the Result table. 



Error Handling 

* If the specified table does not exist, an error must be thrown - SEMANTIC ERROR: Relation doesn't exist 


* If any column mentioned in the query does not exist in the specified table, an error must be thrown SEMANTIC ERROR: Column doesn't exist in relation 


* If no rows in the two tables satisfy the join condition or the WHERE condition, the resultant table will be empty, in which case the resultant table should not be created. 



Examples 

**Example 1** 

*EMPLOYEE* The following table: 

| EmpID | Name | Dept |
| --- | --- | --- |
| 1 | Alice | 10 |
| 2 | Bob | 20 |
| 3 | Carol | 10 |

*SALARY* The following table: 

| EID | Salary |
| --- | --- |
| 1 | 50000 |
| 2 | 60000 |
| 3 | 55000 |

*Query* 
`Result <- JOIN EMPLOYEE, SALARY ON EMPLOYEE. EmpID == SALARY.EID WHERE EMPLOYEE. Dept == 10 PROJECT EMPLOYEE. [cite_start]Name, SALARY.Salary;` 

*Result* The following table: 

| Name | Salary |
| --- | --- |
| Alice | 50000 |
| Carol | 55000 |

*Explanation* Tables are joined on matching EmpID and EID values, rows with Dept =10 are selected, and only Name and Salary columns are projected. 

**Example 2** 

*Table A* The following table: 

| IDA | Value A |
| --- | --- |
| 1 | 10 |
| 2 | 20 |
| 3 | 30 |

*Table B* The following table: 

| IDB | ValueB |
| --- | --- |
| 1 | 12 |
| 2 | 25 |
| 3 | 40 |

*Query* `Result <- JOIN A, B ON A. ValueA B. ValueB == 5` 

*Result* The following table: 

| IDA | ValueA | IDB | ValueB |
| --- | --- | --- | --- |
| 3 | 30 | 2 | 25 |

*Explanation* Tables are joined where A.ValueA B.ValueB = 5. 

**Example 3** 

*Table A* The following table: 

| IDA | Score A |
| --- | --- |
| 1 | 5 |
| 2 | 8 |
| 3 | 10 |

*Table B* The following table: 

| IDB | ScoreB |
| --- | --- |
| 1 | 3 |
| 2 | 4 |
| 3 | 6 |

*Query* `Result <- JOIN A, B ON A.ScoreA B.ScoreB == 12 WHERE A. IDA >= 2 PROJECT A. IDA, A. ScoreA, B.ScoreB;` 

*Result* The following table: 

| IDA | ScoreA | ScoreB |
| --- | --- | --- |
| 2 | 8 | 4 |

*Explanation* 
12. rows with A.IDA > 2 Tables are joined using the condition A.Score A + B.ScoreB are selected, and only IDA, ScoreA, and ScoreB are displayed as a result of the PROJECT condition. 

---

5 GROUP BY 

Extend SimpleRA to implement GROUP BY aggregate operations with a HAVING clause. 

Syntax 

`Result1, Result2 <- GROUP BY <group-attr1>, <group-attr2> FROM <table-name> HAVING <aggregate-expression> RETURN <return-aggregate1>, <return-aggregate2>;` 

* Note: Line breaks in the syntax description are used only for readability in the document. In the actual query, the entire command will appear on a single line without line breaks. 


* This is a multiple grouping operation where a single command can produce multiple return tables, each based on a different grouping attribute and its corresponding return aggregate. 


* Each grouping attribute in the GROUP BY clause is processed independently, and each produces its own resultant table using the matching aggregate from the RETURN clause. 


* The first aggregate in RETURN is applied to the groups formed by the first GROUP BY attribute and produces the first resultant table, the second aggregate in RETURN is applied to the second GROUP BY attribute and produces the second resultant table, and so on. 


* Example: For the query `R1, R2 GROUP BY A, B FROM T HAVING E > 20 RETURN COUNT(C), SUM(D);` R1 is created by grouping table T on attribute A, filtering by the HAVING clause and including COUNT(C) as the result column. R2 is created by grouping table T on attribute B, filtering by the HAVING clause and including SUM(D) as the result column. 




* Hence, each resultant table contains exactly two columns: the corresponding grouping attribute and the corresponding return aggregate. 


* The number of grouping attributes will be less than 5. 


* The names of the resultant tables will not match the name of any table already loaded in the system. 


* If no group of a particular grouping attribute satisfies the HAVING condition, the corresponding resultant table will be empty, in which case the resultant table should not be created. 



Explanation of Syntax 

**HAVING ** The aggregate expression may be one of the following two types: 

* Constant value comparison 


* Form: aggregate-function op number 


* Compares the result of an aggregate function with a constant value. 


* Example: HAVING MAX (Salary) > 10000 This condition filters the groups which have a maximum salary strictly greater than 10000. 




* Aggregate function expression 


* Form: aggregate-function op aggregate-function 


* Compares the results of two aggregate functions. 


* Example: HAVING MAX (Salary) > MAX (Expenses) This condition filters the groups which have a maximum salary strictly greater than their maximum expenses. 





For both conditions: 

* The aggregate functions can be: MAX, MIN, COUNT, SUM, AVG. Each aggregate function is applied on a specific column of a table. 


* op can be ==, !=, >, >=, <, or <=. 


* 0<= number <=10000. 


* There will only be one condition per HAVING clause in a query. 



**RETURN ** 

* The RETURN clause specifies one or more aggregate functions whose results must be produced for each group. 


* The name of each column in the resultant table is formed by appending the return aggregate function name to the attribute name. For example, RETURN MAX (Salary) will result in a column in the resultant table called MAXSalary. 


* However, there is one exception: if the return attribute is COUNT(*), the corresponding column in the resultant table will be called COUNT. 



Error Handling 

* If the specified table does not exist, an error must be thrown - SEMANTIC ERROR: Relation doesn't exist 


* If any of the grouping attributes mentioned in the query do not exist in the specified table, an error must be thrown SEMANTIC ERROR: Group By column doesn't exist in relation 


* If any of the having attributes mentioned in the query do not exist in the specified table, an error must be thrown - SEMANTIC ERROR: Having column doesn't exist in relation 


* If any of the return attributes mentioned in the query do not exist in the specified table, an error must be thrown SEMANTIC ERROR: Return column doesn't exist in relation 



Examples 

*EMPLOYEE* The following table: 

| EmployeeID | DepartmentID | RoleID | Salary | Expenses | YearsOfExperience |
| --- | --- | --- | --- | --- | --- |
| 1 | 1 | 1 | 80000 | 20000 | 5 |
| 2 | 2 | 2 | 60000 | 150000 | 5 |
| 3 | 2 | 3 | 20000 | 5000 | 1 |
| 4 | 3 | 3 | 60000 | 12000 | 5 |
| 5 | 1 | 4 | 25000 | 8000 | 2 |
| 6 | 4 | 1 | 25000 | 7000 | 3 |

**Example 1** 

*Query* `Result <- GROUP BY DepartmentID FROM EMPLOYEE HAVING AVG(Salary) > 30000 RETURN COUNT(*)` 

*Result* The following table: 

| Department ID | COUNT |
| --- | --- |
| 1 | 2 |
| 2 | 2 |
| 3 | 1 |

*Explanation* The average salary of departments 1, 2, 3, and 4 is 52500, 40000, 60000, and 25000 respectively. Out of these, only departments 1, 2, and 3 have average salaries of over 30000, and the number of rows (for COUNT(*)) that are in each of these departments are 2, 2, and 1 respectively. 

**Example 2** 

*Query* `Result1, Result2, Result3 <- GROUP BY DepartmentID, RoleID, YearsOfExperience FROM EMPLOYEE HAVING AVG (Salary) > AVG (Expenses) RETURN MAX (Salary), MIN(YearsOfExperience), SUM (Expenses)` 

*Result* 

*Result 1* The following table: 

| DepartmentID | MAXSalary |
| --- | --- |
| 1 | 80000 |
| 3 | 60000 |
| 4 | 25000 |

*Result2* The following table: 

| RoleID | MIN YearsOfExperience |
| --- | --- |
| 1 | 3 |
| 3 | 1 |
| 4 | 2 |

*Result3* The following table: 

| YearsOfExperience | SUMExpenses |
| --- | --- |
| 1 | 5000 |
| 2 | 8000 |
| 4 | 7000 |
| 5 | 182000 |

*Explanation* 

* Result1: The average salary of departments 1, 2, 3, and 4 is 52500, 40000, 60000, and 25000 respectively. The average expenses of departments 1, 2, 3, and 4 is 14000, 77500, 12000, and 7000 respectively. Out of these, only departments 1, 3, and 4 have their average salary higher than their average expenses. The maximum salary within these departments is 80000, 60000, and 25000 for departments 1, 3, and 4 respectively. 


* Result2: The average salary of roles 1, 2, 3, and 4 is 52500, 60000, 40000, and 25000 respectively. The average expenses of roles 1, 2, 3, and 4 is 13500, 150000, 8500, and 8000 respectively. Out of these, only roles 1, 3, and 4 have their average salary higher than their average expenses. The minimum years of experience within these departments is 3, 1, and 2 for departments 1, 3, and 4 respectively. 


* Result3: The average salary of people with 1, 2, 3, and 5 years of experience is 20000, 25000, 25000, and 66667 respectively. The average expenses of people with 1, 2, 3, and 5 years of experience is 5000, 8000, 7000, and 60667 respectively. Out of these, all the groups have their average salary higher than their average expenses. The sum of expenses within these groups is 5000, 8000, 7000, and 182000 for groups with 1, 2, 3, and 5 years of experience respectively. 



---

6 Report 

Submit a Report2.md in the docs directory including: 

* Detailed explanation of implementation for each command (logic, block access, error handling). 


* Assumptions (under a separate Assumptions heading). 


* Contribution of each team member. 



---

7 Evaluation 

* Automated scripts will be run to assess correctness, performance, and adherence to the specified constraints. 


* An in-person viva to assess understanding of the implemented code. You will be evaluated on the optimizations used to minimize block accesses while ensuring that memory constraints are not violated. 



---

Would you like me to help draft any of the query implementations, or explain the external sorting logic mentioned in the document?