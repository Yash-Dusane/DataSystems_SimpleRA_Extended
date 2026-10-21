#include "global.h"

#include "phase2_parse_utils.h"

#include <queue>

static bool parseBinaryOperatorToken(const string &tok, BinaryOperator &out)
{
    if (tok == "<")
        out = LESS_THAN;
    else if (tok == ">")
        out = GREATER_THAN;
    else if (tok == ">=" || tok == "=>")
        out = GEQ;
    else if (tok == "<=" || tok == "=<")
        out = LEQ;
    else if (tok == "==")
        out = EQUAL;
    else if (tok == "!=")
        out = NOT_EQUAL;
    else
        return false;
    return true;
}

static bool isNonNegativeInteger(const string &s)
{
    if (s.empty())
        return false;
    for (char c : s)
        if (c < '0' || c > '9')
            return false;
    return true;
}

/**
 * @brief
 * SYNTAX:
 * Result <- JOIN <table1>, <table2> ON <join-condition> [WHERE <selection-condition>] [PROJECT <attribute-list>]
 *
 * join-condition:
 *  - <A.a> == <B.b>
 *  - <A.a> (+|-) <B.b> == number
 */
bool syntacticParseJOIN()
{
    logger.log("syntacticParseJOIN");
    if (tokenizedQuery.size() < 9 || tokenizedQuery[1] != "<-" || tokenizedQuery[2] != "JOIN")
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }
    if (tokenizedQuery[5] != "ON")
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    parsedQuery.queryType = JOIN;
    parsedQuery.joinResultRelationName = tokenizedQuery[0];
    parsedQuery.joinFirstRelationName = tokenizedQuery[3];
    parsedQuery.joinSecondRelationName = tokenizedQuery[4];
    parsedQuery.joinHasWhereClause = false;
    parsedQuery.joinHasProjectClause = false;
    parsedQuery.joinProjectColumns.clear();
    parsedQuery.joinConditionType = NO_JOIN_COND;
    parsedQuery.joinArithmeticOperator = NO_ARITH_OP;
    parsedQuery.joinArithmeticConstant = 0;

    int condStart = 6;
    if (condStart + 2 >= (int)tokenizedQuery.size())
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    // Attribute equality: A.a == B.b
    if (tokenizedQuery[condStart + 1] == "==")
    {
        parsedQuery.joinConditionType = JOIN_ATTR_EQ;
        parsedQuery.joinFirstColumnName = tokenizedQuery[condStart];
        parsedQuery.joinSecondColumnName = tokenizedQuery[condStart + 2];
        condStart += 3;
    }
    // Arithmetic: A.a (+|-) B.b == number
    else if (tokenizedQuery[condStart + 1] == "+" || tokenizedQuery[condStart + 1] == "-")
    {
        if (condStart + 4 >= (int)tokenizedQuery.size() || tokenizedQuery[condStart + 3] != "==")
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }
        if (!isNonNegativeInteger(tokenizedQuery[condStart + 4]))
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }

        parsedQuery.joinConditionType = JOIN_ARITH_EQ;
        parsedQuery.joinFirstColumnName = tokenizedQuery[condStart];
        parsedQuery.joinSecondColumnName = tokenizedQuery[condStart + 2];
        parsedQuery.joinArithmeticOperator = (tokenizedQuery[condStart + 1] == "+") ? PLUS : MINUS;
        parsedQuery.joinArithmeticConstant = stoi(tokenizedQuery[condStart + 4]);
        condStart += 5;
    }
    else
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    // Optional clauses
    for (int i = condStart; i < (int)tokenizedQuery.size();)
    {
        if (tokenizedQuery[i] == "WHERE")
        {
            if (parsedQuery.joinHasWhereClause || i + 3 >= (int)tokenizedQuery.size())
            {
                cout << "SYNTAX ERROR" << endl;
                return false;
            }
            BinaryOperator op;
            if (!parseBinaryOperatorToken(tokenizedQuery[i + 2], op))
            {
                cout << "SYNTAX ERROR" << endl;
                return false;
            }
            // WHERE constant may be negative (sign prefix handled here)
            {
                string &numTok = tokenizedQuery[i + 3];
                bool neg = !numTok.empty() && numTok[0] == '-';
                string digits = neg ? numTok.substr(1) : numTok;
                if (!isNonNegativeInteger(digits))
                {
                    cout << "SYNTAX ERROR" << endl;
                    return false;
                }
            }
            parsedQuery.joinHasWhereClause = true;
            parsedQuery.joinWhereColumnName = tokenizedQuery[i + 1];
            parsedQuery.joinWhereBinaryOperator = op;
            parsedQuery.joinWhereIntLiteral = stoi(tokenizedQuery[i + 3]);
            i += 4;
        }
        else if (tokenizedQuery[i] == "PROJECT")
        {
            if (parsedQuery.joinHasProjectClause || i + 1 >= (int)tokenizedQuery.size())
            {
                cout << "SYNTAX ERROR" << endl;
                return false;
            }
            parsedQuery.joinHasProjectClause = true;
            for (int j = i + 1; j < (int)tokenizedQuery.size(); j++)
                parsedQuery.joinProjectColumns.emplace_back(tokenizedQuery[j]);
            break;
        }
        else
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }
    }

    return true;
}

bool semanticParseJOIN()
{
    logger.log("semanticParseJOIN");

    if (tableCatalogue.isTable(parsedQuery.joinResultRelationName))
    {
        cout << "SEMANTIC ERROR: Resultant relation already exists" << endl;
        return false;
    }

    if (!tableCatalogue.isTable(parsedQuery.joinFirstRelationName) || !tableCatalogue.isTable(parsedQuery.joinSecondRelationName))
    {
        cout << "SEMANTIC ERROR: Relation doesn't exist" << endl;
        return false;
    }

    if (parsedQuery.joinFirstRelationName == parsedQuery.joinSecondRelationName)
    {
        cout << "SEMANTIC ERROR" << endl;
        return false;
    }

    Table *A = tableCatalogue.getTable(parsedQuery.joinFirstRelationName);
    Table *B = tableCatalogue.getTable(parsedQuery.joinSecondRelationName);

    auto validateQualified = [&](const string &q, const string &err) -> bool
    {
        string rel, col;
        if (!splitQualifiedColumn(q, rel, col))
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }
        if (rel != parsedQuery.joinFirstRelationName && rel != parsedQuery.joinSecondRelationName)
        {
            cout << "SEMANTIC ERROR: Column doesn't exist in relation" << endl;
            return false;
        }
        if (rel == parsedQuery.joinFirstRelationName && !A->isColumn(col))
        {
            cout << err << endl;
            return false;
        }
        if (rel == parsedQuery.joinSecondRelationName && !B->isColumn(col))
        {
            cout << err << endl;
            return false;
        }
        return true;
    };

    if (!validateQualified(parsedQuery.joinFirstColumnName, "SEMANTIC ERROR: Column doesn't exist in relation"))
        return false;
    if (!validateQualified(parsedQuery.joinSecondColumnName, "SEMANTIC ERROR: Column doesn't exist in relation"))
        return false;

    // Enforce that joinFirstColumnName refers to table1 and joinSecondColumnName refers to table2.
    {
        string rel, col;
        splitQualifiedColumn(parsedQuery.joinFirstColumnName, rel, col);
        if (rel != parsedQuery.joinFirstRelationName)
        {
            cout << "SEMANTIC ERROR: Column doesn't exist in relation" << endl;
            return false;
        }
        splitQualifiedColumn(parsedQuery.joinSecondColumnName, rel, col);
        if (rel != parsedQuery.joinSecondRelationName)
        {
            cout << "SEMANTIC ERROR: Column doesn't exist in relation" << endl;
            return false;
        }
    }

    if (parsedQuery.joinConditionType == JOIN_ARITH_EQ)
    {
        // keep the literal unrestricted (table values can exceed 10000)
    }

    if (parsedQuery.joinHasWhereClause)
    {
        if (!validateQualified(parsedQuery.joinWhereColumnName, "SEMANTIC ERROR: Column doesn't exist in relation"))
            return false;
        string rel, col;
        splitQualifiedColumn(parsedQuery.joinWhereColumnName, rel, col);
        if (rel != parsedQuery.joinFirstRelationName)
        {
            cout << "SEMANTIC ERROR: Column doesn't exist in relation" << endl;
            return false;
        }
    }

    if (parsedQuery.joinHasProjectClause)
    {
        for (auto &p : parsedQuery.joinProjectColumns)
            if (!validateQualified(p, "SEMANTIC ERROR: Column doesn't exist in relation"))
                return false;
    }

    return true;
}

struct ProjectAttr
{
    bool fromA;
    int columnIndex;
    string columnName;
};

static long long joinKeyInASpace(const vector<int> &row, bool isA, int aColIdx, int bColIdx, JoinConditionType condType, ArithmeticOperator arithOp, int constant)
{
    if (isA)
        return row[aColIdx];
    if (condType == JOIN_ATTR_EQ)
        return row[bColIdx];
    // ARITH: A.a (+|-) B.b == constant => A.a == constant -/+ B.b
    long long b = row[bColIdx];
    if (arithOp == PLUS)
        return (long long)constant - b;
    return (long long)constant + b;
}

static int partitionIndex(long long key, int partitions)
{
    if (partitions <= 1)
        return 0;
    long long mod = key % partitions;
    if (mod < 0)
        mod += partitions;
    return (int)mod;
}

static void partitionRelation(
    Table *src,
    bool isA,
    const vector<Table *> &partTables,
    int aColIdx,
    int bColIdx,
    JoinConditionType condType,
    ArithmeticOperator arithOp,
    int constant,
    bool hasWhere,
    int whereColIdx,
    BinaryOperator whereOp,
    int whereVal)
{
    int P = (int)partTables.size();
    if (P == 0)
        return;

    uint maxRows = partTables[0]->maxRowsPerBlock;
    int colCount = partTables[0]->columnCount;
    vector<int> blank(colCount, 0);
    vector<vector<vector<int>>> buffers(P, vector<vector<int>>(maxRows, blank));
    vector<uint> bufCounts(P, 0);

    Cursor cursor(src->tableName, 0);
    vector<int> row = cursor.getNext();
    while (!row.empty())
    {
        if (isA && hasWhere)
        {
            if (!evaluateBinOp(row[whereColIdx], whereVal, whereOp))
            {
                row = cursor.getNext();
                continue;
            }
        }

        long long key = joinKeyInASpace(row, isA, aColIdx, bColIdx, condType, arithOp, constant);
        int idx = partitionIndex(key, P);
        Table *pt = partTables[idx];

        buffers[idx][bufCounts[idx]++] = row;
        pt->rowCount++;
        if (bufCounts[idx] == pt->maxRowsPerBlock)
        {
            bufferManager.writePage(pt->tableName, pt->blockCount, buffers[idx], (int)bufCounts[idx]);
            pt->rowsPerBlockCount.emplace_back(bufCounts[idx]);
            pt->blockCount++;
            bufCounts[idx] = 0;
        }

        row = cursor.getNext();
    }

    for (int i = 0; i < P; i++)
    {
        if (bufCounts[i])
        {
            Table *pt = partTables[i];
            bufferManager.writePage(pt->tableName, pt->blockCount, buffers[i], (int)bufCounts[i]);
            pt->rowsPerBlockCount.emplace_back(bufCounts[i]);
            pt->blockCount++;
            bufCounts[i] = 0;
        }
    }
}

void executeJOIN()
{
    logger.log("executeJOIN");

    Table *A = tableCatalogue.getTable(parsedQuery.joinFirstRelationName);
    Table *B = tableCatalogue.getTable(parsedQuery.joinSecondRelationName);

    string aRel, aCol, bRel, bCol;
    splitQualifiedColumn(parsedQuery.joinFirstColumnName, aRel, aCol);
    splitQualifiedColumn(parsedQuery.joinSecondColumnName, bRel, bCol);
    int aColIdx = A->getColumnIndex(aCol);
    int bColIdx = B->getColumnIndex(bCol);

    bool hasWhere = parsedQuery.joinHasWhereClause;
    int whereColIdx = -1;
    if (hasWhere)
    {
        string wRel, wCol;
        splitQualifiedColumn(parsedQuery.joinWhereColumnName, wRel, wCol);
        whereColIdx = A->getColumnIndex(wCol);
    }

    vector<ProjectAttr> project;
    vector<string> resultColumns;
    if (parsedQuery.joinHasProjectClause)
    {
        for (auto &q : parsedQuery.joinProjectColumns)
        {
            string rel, col;
            splitQualifiedColumn(q, rel, col);
            if (rel == parsedQuery.joinFirstRelationName)
                project.push_back(ProjectAttr{true, A->getColumnIndex(col), col});
            else
                project.push_back(ProjectAttr{false, B->getColumnIndex(col), col});
            resultColumns.emplace_back(col);
        }
    }
    else
    {
        for (auto &c : A->columns)
            resultColumns.emplace_back(c);
        for (auto &c : B->columns)
            resultColumns.emplace_back(c);
    }

    int P = (int)BLOCK_COUNT - 1;
    if (P < 1)
        P = 1;

    vector<Table *> partsA, partsB;
    partsA.reserve(P);
    partsB.reserve(P);

    for (int i = 0; i < P; i++)
    {
        string pa = "__hj_" + parsedQuery.joinResultRelationName + "_A_p" + to_string(i);
        string pb = "__hj_" + parsedQuery.joinResultRelationName + "_B_p" + to_string(i);
        Table *ta = new Table(pa, A->columns);
        Table *tb = new Table(pb, B->columns);
        tableCatalogue.insertTable(ta);
        tableCatalogue.insertTable(tb);
        partsA.emplace_back(ta);
        partsB.emplace_back(tb);
    }

    partitionRelation(
        A,
        true,
        partsA,
        aColIdx,
        bColIdx,
        parsedQuery.joinConditionType,
        parsedQuery.joinArithmeticOperator,
        parsedQuery.joinArithmeticConstant,
        hasWhere,
        whereColIdx,
        parsedQuery.joinWhereBinaryOperator,
        parsedQuery.joinWhereIntLiteral);
    partitionRelation(
        B,
        false,
        partsB,
        aColIdx,
        bColIdx,
        parsedQuery.joinConditionType,
        parsedQuery.joinArithmeticOperator,
        parsedQuery.joinArithmeticConstant,
        false,
        -1,
        NO_BINOP_CLAUSE,
        0);

    Table *result = new Table(parsedQuery.joinResultRelationName, resultColumns);
    result->rowCount = 0;
    result->blockCount = 0;
    result->rowsPerBlockCount.clear();

    vector<int> blank(result->columnCount, 0);
    vector<vector<int>> outPage(result->maxRowsPerBlock, blank);
    uint outCount = 0;

    auto emit = [&](const vector<int> &rowA, const vector<int> &rowB)
    {
        vector<int> outRow;
        outRow.reserve(result->columnCount);
        if (parsedQuery.joinHasProjectClause)
        {
            for (auto &p : project)
                outRow.emplace_back(p.fromA ? rowA[p.columnIndex] : rowB[p.columnIndex]);
        }
        else
        {
            for (auto &v : rowA)
                outRow.emplace_back(v);
            for (auto &v : rowB)
                outRow.emplace_back(v);
        }

        outPage[outCount++] = outRow;
        result->rowCount++;
        if (outCount == result->maxRowsPerBlock)
        {
            bufferManager.writePage(result->tableName, result->blockCount, outPage, (int)outCount);
            result->rowsPerBlockCount.emplace_back(outCount);
            result->blockCount++;
            outCount = 0;
        }
    };

    for (int i = 0; i < P; i++)
    {
        Table *pa = partsA[i];
        Table *pb = partsB[i];
        if (pa->rowCount == 0 || pb->rowCount == 0)
            continue;

        bool buildA = pa->blockCount <= pb->blockCount;

        unordered_map<long long, vector<vector<int>>> hashTable;
        if (buildA)
        {
            Cursor c(pa->tableName, 0);
            for (vector<int> r = c.getNext(); !r.empty(); r = c.getNext())
            {
                long long key = joinKeyInASpace(r, true, aColIdx, bColIdx, parsedQuery.joinConditionType, parsedQuery.joinArithmeticOperator, parsedQuery.joinArithmeticConstant);
                hashTable[key].push_back(r);
            }

            Cursor cp(pb->tableName, 0);
            for (vector<int> rB = cp.getNext(); !rB.empty(); rB = cp.getNext())
            {
                long long key = joinKeyInASpace(rB, false, aColIdx, bColIdx, parsedQuery.joinConditionType, parsedQuery.joinArithmeticOperator, parsedQuery.joinArithmeticConstant);
                auto it = hashTable.find(key);
                if (it == hashTable.end())
                    continue;
                for (auto &rA : it->second)
                    emit(rA, rB);
            }
        }
        else
        {
            Cursor c(pb->tableName, 0);
            for (vector<int> r = c.getNext(); !r.empty(); r = c.getNext())
            {
                long long key = joinKeyInASpace(r, false, aColIdx, bColIdx, parsedQuery.joinConditionType, parsedQuery.joinArithmeticOperator, parsedQuery.joinArithmeticConstant);
                hashTable[key].push_back(r);
            }

            Cursor cp(pa->tableName, 0);
            for (vector<int> rA = cp.getNext(); !rA.empty(); rA = cp.getNext())
            {
                long long key = joinKeyInASpace(rA, true, aColIdx, bColIdx, parsedQuery.joinConditionType, parsedQuery.joinArithmeticOperator, parsedQuery.joinArithmeticConstant);
                auto it = hashTable.find(key);
                if (it == hashTable.end())
                    continue;
                for (auto &rB : it->second)
                    emit(rA, rB);
            }
        }
    }

    if (outCount)
    {
        bufferManager.writePage(result->tableName, result->blockCount, outPage, (int)outCount);
        result->rowsPerBlockCount.emplace_back(outCount);
        result->blockCount++;
    }

    // cleanup partitions
    for (int i = 0; i < P; i++)
    {
        tableCatalogue.deleteTable(partsA[i]->tableName);
        tableCatalogue.deleteTable(partsB[i]->tableName);
    }

    if (result->rowCount == 0)
    {
        result->unload();
        delete result;
        return;
    }
    tableCatalogue.insertTable(result);
    return;
}
