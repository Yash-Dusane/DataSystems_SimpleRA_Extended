#include "global.h"

#include "phase2_external_sort.h"
#include "phase2_parse_utils.h"

#include <limits>

struct AggState
{
    long long count = 0;
    long long sum = 0;
    int mn = 0;
    int mx = 0;
    bool initialized = false;
};

static void updateAggState(AggState &st, int v)
{
    st.count++;
    st.sum += v;
    if (!st.initialized)
    {
        st.mn = v;
        st.mx = v;
        st.initialized = true;
        return;
    }
    st.mn = min(st.mn, v);
    st.mx = max(st.mx, v);
}

static long long evalAgg(const AggregateTerm &term, const AggState &st, long long totalRowsInGroup)
{
    switch (term.function)
    {
    case AGG_MAX:
        return st.mx;
    case AGG_MIN:
        return st.mn;
    case AGG_SUM:
        return st.sum;
    case AGG_COUNT:
        return term.isStar ? totalRowsInGroup : st.count;
    case AGG_AVG:
        if (st.count == 0)
            return 0;
        // round up (ceil) for non-negative values as per Phase2 Q&A
        if (st.sum >= 0)
            return (st.sum + st.count - 1) / st.count;
        // fallback for negative sums
        return st.sum / st.count;
    default:
        return 0;
    }
}

static bool evalHaving(long long lhs, long long rhs, BinaryOperator op)
{
    switch (op)
    {
    case LESS_THAN:
        return lhs < rhs;
    case GREATER_THAN:
        return lhs > rhs;
    case LEQ:
        return lhs <= rhs;
    case GEQ:
        return lhs >= rhs;
    case EQUAL:
        return lhs == rhs;
    case NOT_EQUAL:
        return lhs != rhs;
    default:
        return false;
    }
}

static bool safeLongLongToInt(long long value, int &out)
{
    if (value < (long long)std::numeric_limits<int>::min() || value > (long long)std::numeric_limits<int>::max())
        return false;
    out = (int)value;
    return true;
}

/**
 * @brief
 * SYNTAX:
 * Result1, Result2 <- GROUP BY <group-attr1>, <group-attr2>
 * FROM <table-name> HAVING <aggregate-expression> RETURN <return-aggregate1>, <return-aggregate2>
 */
bool syntacticParseGROUP_BY(int arrowPos)
{
    logger.log("syntacticParseGROUP_BY");
    if (arrowPos < 1 || arrowPos + 1 >= (int)tokenizedQuery.size() || tokenizedQuery[arrowPos] != "<-")
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }
    if (arrowPos + 2 >= (int)tokenizedQuery.size() || tokenizedQuery[arrowPos + 1] != "GROUP" || tokenizedQuery[arrowPos + 2] != "BY")
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    vector<string> results;
    for (int i = 0; i < arrowPos; i++)
        results.emplace_back(tokenizedQuery[i]);

    int fromPos = -1;
    for (int i = arrowPos + 3; i < (int)tokenizedQuery.size(); i++)
        if (tokenizedQuery[i] == "FROM")
        {
            fromPos = i;
            break;
        }
    if (fromPos == -1 || fromPos + 1 >= (int)tokenizedQuery.size())
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    vector<string> groupCols;
    for (int i = arrowPos + 3; i < fromPos; i++)
        groupCols.emplace_back(tokenizedQuery[i]);
    if (groupCols.empty() || groupCols.size() >= 5)
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    int havingPos = fromPos + 2;
    if (havingPos >= (int)tokenizedQuery.size() || tokenizedQuery[havingPos] != "HAVING")
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    int returnPos = -1;
    for (int i = havingPos + 1; i < (int)tokenizedQuery.size(); i++)
        if (tokenizedQuery[i] == "RETURN")
        {
            returnPos = i;
            break;
        }
    if (returnPos == -1)
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    // HAVING expression must be 3 tokens: agg op (number|agg)
    if (returnPos - (havingPos + 1) != 3)
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    AggregateTerm lhsAgg, rhsAgg;
    if (!parseAggregateTermToken(tokenizedQuery[havingPos + 1], lhsAgg))
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    string opTok = tokenizedQuery[havingPos + 2];
    BinaryOperator op = NO_BINOP_CLAUSE;
    if (opTok == "<")
        op = LESS_THAN;
    else if (opTok == ">")
        op = GREATER_THAN;
    else if (opTok == ">=" || opTok == "=>")
        op = GEQ;
    else if (opTok == "<=" || opTok == "=<")
        op = LEQ;
    else if (opTok == "==")
        op = EQUAL;
    else if (opTok == "!=")
        op = NOT_EQUAL;
    else
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    string rhsTok = tokenizedQuery[havingPos + 3];
    regex numeric("^[0-9]+$");

    parsedQuery.queryType = GROUP_BY;
    parsedQuery.groupByResultRelationNames = results;
    parsedQuery.groupByColumnNames = groupCols;
    parsedQuery.groupByRelationName = tokenizedQuery[fromPos + 1];
    parsedQuery.groupByHavingLhs = lhsAgg;
    parsedQuery.groupByHavingBinaryOperator = op;

    if (regex_match(rhsTok, numeric))
    {
        parsedQuery.groupByHavingType = HAVING_AGG_CONST;
        parsedQuery.groupByHavingRhsConst = stoi(rhsTok);
        parsedQuery.groupByHavingRhsAgg = AggregateTerm();
    }
    else
    {
        if (!parseAggregateTermToken(rhsTok, rhsAgg))
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }
        parsedQuery.groupByHavingType = HAVING_AGG_AGG;
        parsedQuery.groupByHavingRhsAgg = rhsAgg;
        parsedQuery.groupByHavingRhsConst = 0;
    }

    parsedQuery.groupByReturnAggregates.clear();
    for (int i = returnPos + 1; i < (int)tokenizedQuery.size(); i++)
    {
        AggregateTerm t;
        if (!parseAggregateTermToken(tokenizedQuery[i], t))
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }
        parsedQuery.groupByReturnAggregates.emplace_back(t);
    }

    if (parsedQuery.groupByReturnAggregates.empty())
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    if (parsedQuery.groupByResultRelationNames.size() != parsedQuery.groupByColumnNames.size() ||
        parsedQuery.groupByReturnAggregates.size() != parsedQuery.groupByColumnNames.size())
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    return true;
}

bool semanticParseGROUP_BY()
{
    logger.log("semanticParseGROUP_BY");

    unordered_set<string> seenResults;
    for (auto &res : parsedQuery.groupByResultRelationNames)
    {
        if (!seenResults.insert(res).second)
        {
            cout << "SEMANTIC ERROR: Resultant relation already exists" << endl;
            return false;
        }
        if (tableCatalogue.isTable(res))
        {
            cout << "SEMANTIC ERROR: Resultant relation already exists" << endl;
            return false;
        }
    }

    if (!tableCatalogue.isTable(parsedQuery.groupByRelationName))
    {
        cout << "SEMANTIC ERROR: Relation doesn't exist" << endl;
        return false;
    }

    Table *table = tableCatalogue.getTable(parsedQuery.groupByRelationName);

    for (auto &gcol : parsedQuery.groupByColumnNames)
    {
        if (!table->isColumn(gcol))
        {
            cout << "SEMANTIC ERROR: Group By column doesn't exist in relation" << endl;
            return false;
        }
    }

    auto validateAggAttr = [&](const AggregateTerm &t, const string &errMsg) -> bool
    {
        if (t.function == AGG_COUNT && t.isStar)
            return true;
        if (!table->isColumn(t.attribute))
        {
            cout << errMsg << endl;
            return false;
        }
        return true;
    };

    if (!validateAggAttr(parsedQuery.groupByHavingLhs, "SEMANTIC ERROR: Having column doesn't exist in relation"))
        return false;
    if (parsedQuery.groupByHavingType == HAVING_AGG_AGG)
    {
        if (!validateAggAttr(parsedQuery.groupByHavingRhsAgg, "SEMANTIC ERROR: Having column doesn't exist in relation"))
            return false;
    }
    else if (parsedQuery.groupByHavingType == HAVING_AGG_CONST)
    {
        // allow any integer literal (examples use values > 10000)
    }

    for (auto &ret : parsedQuery.groupByReturnAggregates)
        if (!validateAggAttr(ret, "SEMANTIC ERROR: Return column doesn't exist in relation"))
            return false;

    return true;
}

static void executeSingleGroupBy(
    Table *input,
    const string &resultName,
    const string &groupColumn,
    const AggregateTerm &havingLhs,
    HavingExpressionType havingType,
    BinaryOperator havingOp,
    const AggregateTerm &havingRhsAgg,
    int havingRhsConst,
    const AggregateTerm &returnAgg)
{
    int groupIdx = input->getColumnIndex(groupColumn);

    vector<SortKeySpec> keys;
    keys.push_back(SortKeySpec{groupIdx, ASC});

    Table *sorted = externalSortRangeToTempTable("__gbsort_" + resultName, input, 0, input->rowCount, keys);

    // Determine which columns we need to maintain AggState for
    int lhsIdx = (havingLhs.function == AGG_COUNT && havingLhs.isStar) ? -1 : input->getColumnIndex(havingLhs.attribute);
    int rhsIdx = -1;
    if (havingType == HAVING_AGG_AGG)
        rhsIdx = (havingRhsAgg.function == AGG_COUNT && havingRhsAgg.isStar) ? -1 : input->getColumnIndex(havingRhsAgg.attribute);
    int retIdx = (returnAgg.function == AGG_COUNT && returnAgg.isStar) ? -1 : input->getColumnIndex(returnAgg.attribute);

    Table *result = new Table(resultName, {groupColumn, makeReturnAggregateColumnName(returnAgg)});
    result->rowCount = 0;
    result->blockCount = 0;
    result->rowsPerBlockCount.clear();

    Cursor cursor(sorted->tableName, 0);
    vector<int> row = cursor.getNext();
    if (row.empty())
    {
        tableCatalogue.deleteTable(sorted->tableName);
        result->unload();
        delete result;
        return;
    }

    int currentGroup = row[groupIdx];
    long long groupRows = 0;
    AggState lhsState, rhsState, retState;
    bool overflowDetected = false;

    // We'll stream rows into pages for the result without using Table::writeRow.
    vector<int> blank(result->columnCount, 0);
    vector<vector<int>> pageRows(result->maxRowsPerBlock, blank);
    uint pageRowCounter = 0;

    auto emitRow = [&](int groupVal, int aggVal)
    {
        vector<int> outRow = {groupVal, aggVal};
        pageRows[pageRowCounter++] = outRow;
        result->rowCount++;
        if (pageRowCounter == result->maxRowsPerBlock)
        {
            bufferManager.writePage(result->tableName, result->blockCount, pageRows, (int)pageRowCounter);
            result->rowsPerBlockCount.emplace_back(pageRowCounter);
            result->blockCount++;
            pageRowCounter = 0;
        }
    };

    auto flushGroupAndReset = [&](int groupVal)
    {
        long long lhsVal = evalAgg(havingLhs, lhsState, groupRows);
        long long rhsVal = (havingType == HAVING_AGG_CONST) ? havingRhsConst : evalAgg(havingRhsAgg, rhsState, groupRows);
        if (evalHaving(lhsVal, rhsVal, havingOp))
        {
            long long retVal = evalAgg(returnAgg, retState, groupRows);
            int retValInt = 0;
            if (!safeLongLongToInt(retVal, retValInt))
            {
                cout << "SEMANTIC ERROR: Integer overflow in GROUP BY aggregate" << endl;
                overflowDetected = true;
                return;
            }
            emitRow(groupVal, retValInt);
        }
        groupRows = 0;
        lhsState = AggState();
        rhsState = AggState();
        retState = AggState();
    };

    while (!row.empty())
    {
        int g = row[groupIdx];
        if (g != currentGroup)
        {
            flushGroupAndReset(currentGroup);
            if (overflowDetected)
                break;
            currentGroup = g;
        }

        groupRows++;
        if (lhsIdx != -1)
            updateAggState(lhsState, row[lhsIdx]);
        else
            lhsState.count++; // for COUNT(*)

        if (havingType == HAVING_AGG_AGG)
        {
            if (rhsIdx != -1)
                updateAggState(rhsState, row[rhsIdx]);
            else
                rhsState.count++;
        }

        if (retIdx != -1)
            updateAggState(retState, row[retIdx]);
        else
            retState.count++;

        row = cursor.getNext();
    }
    if (!overflowDetected)
        flushGroupAndReset(currentGroup);

    if (overflowDetected)
    {
        tableCatalogue.deleteTable(sorted->tableName);
        result->unload();
        delete result;
        return;
    }

    if (pageRowCounter)
    {
        bufferManager.writePage(result->tableName, result->blockCount, pageRows, (int)pageRowCounter);
        result->rowsPerBlockCount.emplace_back(pageRowCounter);
        result->blockCount++;
    }

    tableCatalogue.deleteTable(sorted->tableName);

    if (result->rowCount == 0)
    {
        result->unload();
        delete result;
        return;
    }
    tableCatalogue.insertTable(result);
}

void executeGROUP_BY()
{
    logger.log("executeGROUP_BY");
    Table *table = tableCatalogue.getTable(parsedQuery.groupByRelationName);

    for (size_t i = 0; i < parsedQuery.groupByColumnNames.size(); i++)
    {
        executeSingleGroupBy(
            table,
            parsedQuery.groupByResultRelationNames[i],
            parsedQuery.groupByColumnNames[i],
            parsedQuery.groupByHavingLhs,
            parsedQuery.groupByHavingType,
            parsedQuery.groupByHavingBinaryOperator,
            parsedQuery.groupByHavingRhsAgg,
            parsedQuery.groupByHavingRhsConst,
            parsedQuery.groupByReturnAggregates[i]);
    }
    return;
}
