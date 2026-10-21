#include "global.h"

#include "phase2_external_sort.h"

#include <cstdio>
#include <fstream>

static bool copyFileContents(const string &src, const string &dst)
{
    ifstream in(src, ios::binary);
    ofstream out(dst, ios::binary | ios::trunc);
    if (!in.good() || !out.good())
        return false;
    out << in.rdbuf();
    return out.good();
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
 * SORT <table-name> BY <col1>, <col2>, ... IN <ASC|DESC>, <ASC|DESC>, ...
 *   [TOP X] [BOTTOM Y]
 */
bool syntacticParseSORT()
{
    logger.log("syntacticParseSORT");
    // Minimum valid query is: SORT <table> BY <col> IN <ASC|DESC>  (6 tokens)
    if (tokenizedQuery.size() < 6 || tokenizedQuery[0] != "SORT")
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    if (tokenizedQuery[2] != "BY")
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    int inPos = -1;
    for (int i = 0; i < (int)tokenizedQuery.size(); i++)
        if (tokenizedQuery[i] == "IN")
            inPos = i;
    if (inPos == -1)
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    if (inPos <= 3)
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    parsedQuery.queryType = SORT;
    parsedQuery.sortRelationName = tokenizedQuery[1];
    parsedQuery.sortColumnNames.clear();
    parsedQuery.sortColumnStrategies.clear();
    parsedQuery.sortHasTopClause = false;
    parsedQuery.sortTopCount = 0;
    parsedQuery.sortHasBottomClause = false;
    parsedQuery.sortBottomCount = 0;

    // columns: BY ... IN
    for (int i = 3; i < inPos; i++)
        parsedQuery.sortColumnNames.emplace_back(tokenizedQuery[i]);

    // strategies: IN ... [TOP/BOTTOM]
    int clausePos = (int)tokenizedQuery.size();
    for (int i = inPos + 1; i < (int)tokenizedQuery.size(); i++)
    {
        if (tokenizedQuery[i] == "TOP" || tokenizedQuery[i] == "BOTTOM")
        {
            clausePos = i;
            break;
        }
    }

    for (int i = inPos + 1; i < clausePos; i++)
    {
        if (tokenizedQuery[i] == "ASC")
            parsedQuery.sortColumnStrategies.emplace_back(ASC);
        else if (tokenizedQuery[i] == "DESC")
            parsedQuery.sortColumnStrategies.emplace_back(DESC);
        else
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }
    }

    if (parsedQuery.sortColumnNames.empty() || parsedQuery.sortColumnNames.size() != parsedQuery.sortColumnStrategies.size())
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    for (int i = clausePos; i < (int)tokenizedQuery.size();)
    {
        string kw = tokenizedQuery[i];
        if (kw != "TOP" && kw != "BOTTOM")
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }
        if (i + 1 >= (int)tokenizedQuery.size() || !isNonNegativeInteger(tokenizedQuery[i + 1]))
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }

        long long val = stoll(tokenizedQuery[i + 1]);
        if (kw == "TOP")
        {
            parsedQuery.sortHasTopClause = true;
            parsedQuery.sortTopCount = val;
        }
        else
        {
            parsedQuery.sortHasBottomClause = true;
            parsedQuery.sortBottomCount = val;
        }
        i += 2;
    }

    return true;
}

bool semanticParseSORT()
{
    logger.log("semanticParseSORT");
    if (!tableCatalogue.isTable(parsedQuery.sortRelationName))
    {
        cout << "SEMANTIC ERROR: Relation doesn't exist" << endl;
        return false;
    }

    for (auto &col : parsedQuery.sortColumnNames)
    {
        if (!tableCatalogue.isColumnFromTable(col, parsedQuery.sortRelationName))
        {
            cout << "SEMANTIC ERROR: Column doesn't exist in relation" << endl;
            return false;
        }
    }

    Table *table = tableCatalogue.getTable(parsedQuery.sortRelationName);
    long long n = table->rowCount;

    if (parsedQuery.sortHasTopClause && (parsedQuery.sortTopCount < 0 || parsedQuery.sortTopCount > n))
    {
        cout << "SEMANTIC ERROR" << endl;
        return false;
    }
    if (parsedQuery.sortHasBottomClause && (parsedQuery.sortBottomCount < 0 || parsedQuery.sortBottomCount > n))
    {
        cout << "SEMANTIC ERROR" << endl;
        return false;
    }
    if (parsedQuery.sortHasTopClause && parsedQuery.sortHasBottomClause)
    {
        if (parsedQuery.sortTopCount + parsedQuery.sortBottomCount > n)
        {
            cout << "SEMANTIC ERROR" << endl;
            return false;
        }
    }
    return true;
}

static int SORT_TMP_COUNTER = 0;

void executeSORT()
{
    logger.log("executeSORT");
    Table *table = tableCatalogue.getTable(parsedQuery.sortRelationName);

    int tmpId = SORT_TMP_COUNTER++;
    string srcName = "__sortsrc_" + table->tableName + "_" + to_string(tmpId);
    string outName = "__sortout_" + table->tableName + "_" + to_string(tmpId);

    // Snapshot input from sourceFileName to ensure each SORT starts from the original table file.
    Table *src = new Table(srcName, table->columns);
    tableCatalogue.insertTable(src);
    if (!copyFileContents(table->sourceFileName, src->sourceFileName))
    {
        cout << "SEMANTIC ERROR" << endl;
        tableCatalogue.deleteTable(srcName);
        return;
    }
    if (!src->blockify())
    {
        tableCatalogue.deleteTable(srcName);
        return;
    }

    long long n = src->rowCount;
    long long topCount = parsedQuery.sortHasTopClause ? parsedQuery.sortTopCount : 0;
    long long bottomCount = parsedQuery.sortHasBottomClause ? parsedQuery.sortBottomCount : 0;
    if (!parsedQuery.sortHasTopClause && !parsedQuery.sortHasBottomClause)
        topCount = n;

    vector<SortKeySpec> keys;
    keys.reserve(parsedQuery.sortColumnNames.size());
    for (size_t i = 0; i < parsedQuery.sortColumnNames.size(); i++)
    {
        int colIdx = table->getColumnIndex(parsedQuery.sortColumnNames[i]);
        keys.push_back(SortKeySpec{colIdx, parsedQuery.sortColumnStrategies[i]});
    }

    Table *sortedTop = nullptr;
    Table *sortedBottom = nullptr;

    if (topCount > 0)
        sortedTop = externalSortRangeToTempTable(outName + "__top", src, 0, topCount, keys);
    if (bottomCount > 0)
        sortedBottom = externalSortRangeToTempTable(outName + "__bottom", src, n - bottomCount, bottomCount, keys);

    // Build the final table into outName pages.
    Table out(outName, table->columns);
    out.rowCount = 0;
    out.blockCount = 0;
    out.rowsPerBlockCount.clear();

    Cursor baseCursor(srcName, 0);
    Cursor topCursor(sortedTop ? sortedTop->tableName : srcName, 0);
    Cursor bottomCursor(sortedBottom ? sortedBottom->tableName : srcName, 0);

    vector<int> blank(out.columnCount, 0);
    vector<vector<int>> pageRows(out.maxRowsPerBlock, blank);
    uint pageRowCounter = 0;

    long long bottomStart = n - bottomCount;
    for (long long i = 0; i < n; i++)
    {
        vector<int> original = baseCursor.getNext();
        vector<int> outRow;
        if (i < topCount && sortedTop)
            outRow = topCursor.getNext();
        else if (i >= bottomStart && sortedBottom)
            outRow = bottomCursor.getNext();
        else
            outRow = original;

        if (outRow.empty())
            break;

        pageRows[pageRowCounter++] = outRow;
        out.rowCount++;
        if (pageRowCounter == out.maxRowsPerBlock)
        {
            bufferManager.writePage(out.tableName, out.blockCount, pageRows, (int)pageRowCounter);
            out.rowsPerBlockCount.emplace_back(pageRowCounter);
            out.blockCount++;
            pageRowCounter = 0;
        }
    }
    if (pageRowCounter)
    {
        bufferManager.writePage(out.tableName, out.blockCount, pageRows, (int)pageRowCounter);
        out.rowsPerBlockCount.emplace_back(pageRowCounter);
        out.blockCount++;
    }

    // Swap outName pages into the original table.
    bufferManager.invalidateTable(table->tableName);
    for (uint i = 0; i < table->blockCount; i++)
        bufferManager.deleteFile(table->tableName, (int)i);

    for (uint i = 0; i < out.blockCount; i++)
    {
        string from = "../data/temp/" + outName + "_Page" + to_string(i);
        string to = "../data/temp/" + table->tableName + "_Page" + to_string(i);
        std::rename(from.c_str(), to.c_str());
    }

    // Update metadata
    table->rowCount = out.rowCount;
    table->blockCount = out.blockCount;
    table->rowsPerBlockCount = out.rowsPerBlockCount;

    bufferManager.invalidateTable(outName);

    // cleanup scratch tables
    if (sortedTop)
        tableCatalogue.deleteTable(sortedTop->tableName);
    if (sortedBottom)
        tableCatalogue.deleteTable(sortedBottom->tableName);
    tableCatalogue.deleteTable(srcName);

    // remove outName.csv created by Table(outName, ...)
    bufferManager.deleteFile("../data/temp/" + outName + ".csv");
    return;
}
