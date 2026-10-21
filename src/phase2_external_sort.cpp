#include "phase2_external_sort.h"

#include "global.h"
#include <queue>

static int compareRowsWithKeys(const vector<int> &a, const vector<int> &b, const vector<SortKeySpec> &keys)
{
    for (const auto &k : keys)
    {
        int av = a[k.columnIndex];
        int bv = b[k.columnIndex];
        if (av == bv)
            continue;
        bool less = av < bv;
        if (k.strategy == DESC)
            less = !less;
        return less ? -1 : 1;
    }
    return 0;
}

static void stableMergeRows(
    vector<vector<int>> &rows,
    vector<vector<int>> &buffer,
    size_t left,
    size_t mid,
    size_t right,
    const vector<SortKeySpec> &keys)
{
    size_t i = left;
    size_t j = mid;
    size_t out = left;

    while (i < mid && j < right)
    {
        if (compareRowsWithKeys(rows[i], rows[j], keys) <= 0)
            buffer[out++] = rows[i++];
        else
            buffer[out++] = rows[j++];
    }

    while (i < mid)
        buffer[out++] = rows[i++];
    while (j < right)
        buffer[out++] = rows[j++];

    for (size_t idx = left; idx < right; idx++)
        rows[idx] = std::move(buffer[idx]);
}

static void stableMergeSortRange(
    vector<vector<int>> &rows,
    vector<vector<int>> &buffer,
    size_t left,
    size_t right,
    const vector<SortKeySpec> &keys)
{
    if (right - left <= 1)
        return;

    size_t mid = left + (right - left) / 2;
    stableMergeSortRange(rows, buffer, left, mid, keys);
    stableMergeSortRange(rows, buffer, mid, right, keys);
    stableMergeRows(rows, buffer, left, mid, right, keys);
}

static void stableMergeSortRows(vector<vector<int>> &rows, const vector<SortKeySpec> &keys)
{
    if (rows.size() <= 1)
        return;

    vector<vector<int>> buffer(rows.size());
    stableMergeSortRange(rows, buffer, 0, rows.size(), keys);
}

static Table* createTempTableFromRows(const string &tableName, const vector<string> &columns, const vector<vector<int>> &rows)
{
    Table *t = new Table(tableName, columns);
    t->rowCount = 0;
    t->blockCount = 0;
    t->rowsPerBlockCount.clear();

    vector<int> blank(t->columnCount, 0);
    vector<vector<int>> pageRows(t->maxRowsPerBlock, blank);
    uint pageRowCounter = 0;

    for (const auto &row : rows)
    {
        pageRows[pageRowCounter++] = row;
        t->rowCount++;
        if (pageRowCounter == t->maxRowsPerBlock)
        {
            bufferManager.writePage(t->tableName, t->blockCount, pageRows, (int)pageRowCounter);
            t->rowsPerBlockCount.emplace_back(pageRowCounter);
            t->blockCount++;
            pageRowCounter = 0;
        }
    }
    if (pageRowCounter)
    {
        bufferManager.writePage(t->tableName, t->blockCount, pageRows, (int)pageRowCounter);
        t->rowsPerBlockCount.emplace_back(pageRowCounter);
        t->blockCount++;
    }

    tableCatalogue.insertTable(t);
    return t;
}

struct HeapItem
{
    vector<int> row;
    int streamIndex;
};

Table* externalSortRangeToTempTable(
    const string &sortedTableName,
    Table *inputTable,
    long long startRow,
    long long rowCount,
    const vector<SortKeySpec> &keys)
{
    logger.log("externalSortRangeToTempTable");

    if (rowCount <= 0)
    {
        // create an empty table; caller can decide to delete it.
        Table *t = new Table(sortedTableName, inputTable->columns);
        tableCatalogue.insertTable(t);
        return t;
    }

    long long maxRunRows = (long long)BLOCK_COUNT * (long long)inputTable->maxRowsPerBlock;
    if (maxRunRows <= 0)
        maxRunRows = rowCount;

    // --- Run Generation ---
    vector<string> runNames;
    runNames.clear();

    Cursor cursor(inputTable->tableName, 0);
    for (long long i = 0; i < startRow; i++)
        cursor.getNext();

    long long remaining = rowCount;
    int runCounter = 0;
    while (remaining > 0)
    {
        long long take = min(remaining, maxRunRows);
        vector<vector<int>> chunk;
        chunk.reserve((size_t)take);
        for (long long i = 0; i < take; i++)
        {
            vector<int> r = cursor.getNext();
            if (r.empty())
                break;
            chunk.emplace_back(std::move(r));
        }
        if (chunk.empty())
            break;

        stableMergeSortRows(chunk, keys);

        string runName = sortedTableName + "__run_" + to_string(runCounter++);
        createTempTableFromRows(runName, inputTable->columns, chunk);
        runNames.emplace_back(runName);
        remaining -= (long long)chunk.size();
    }

    if (runNames.empty())
    {
        Table *t = new Table(sortedTableName, inputTable->columns);
        tableCatalogue.insertTable(t);
        return t;
    }

    // --- Merge Passes ---
    int fanIn = (int)BLOCK_COUNT - 1;
    if (fanIn < 2)
        fanIn = 2;

    int pass = 0;
    while (runNames.size() > 1)
    {
        vector<string> nextRuns;
        nextRuns.clear();

        for (size_t base = 0; base < runNames.size(); base += (size_t)fanIn)
        {
            size_t end = min(runNames.size(), base + (size_t)fanIn);
            vector<Cursor> cursors;
            cursors.reserve(end - base);
            for (size_t i = base; i < end; i++)
                cursors.emplace_back(runNames[i], 0);

            auto cmp = [&](const HeapItem &x, const HeapItem &y) {
                int c = compareRowsWithKeys(x.row, y.row, keys);
                if (c != 0)
                    return c > 0; // min-heap
                return x.streamIndex > y.streamIndex; // stability between streams
            };
            std::priority_queue<HeapItem, vector<HeapItem>, decltype(cmp)> pq(cmp);

            for (int i = 0; i < (int)cursors.size(); i++)
            {
                vector<int> r = cursors[i].getNext();
                if (!r.empty())
                    pq.push(HeapItem{std::move(r), i});
            }
            // To avoid holding the whole merged output in memory, stream it into pages.
            string outRunName = sortedTableName + "__pass_" + to_string(pass) + "_run_" + to_string((int)nextRuns.size());
            Table *outRun = new Table(outRunName, inputTable->columns);
            outRun->rowCount = 0;
            outRun->blockCount = 0;
            outRun->rowsPerBlockCount.clear();

            vector<int> blank(outRun->columnCount, 0);
            vector<vector<int>> pageRows(outRun->maxRowsPerBlock, blank);
            uint pageRowCounter = 0;

            while (!pq.empty())
            {
                HeapItem it = pq.top();
                pq.pop();

                pageRows[pageRowCounter++] = it.row;
                outRun->rowCount++;
                if (pageRowCounter == outRun->maxRowsPerBlock)
                {
                    bufferManager.writePage(outRun->tableName, outRun->blockCount, pageRows, (int)pageRowCounter);
                    outRun->rowsPerBlockCount.emplace_back(pageRowCounter);
                    outRun->blockCount++;
                    pageRowCounter = 0;
                }

                vector<int> nxt = cursors[it.streamIndex].getNext();
                if (!nxt.empty())
                    pq.push(HeapItem{std::move(nxt), it.streamIndex});
            }

            if (pageRowCounter)
            {
                bufferManager.writePage(outRun->tableName, outRun->blockCount, pageRows, (int)pageRowCounter);
                outRun->rowsPerBlockCount.emplace_back(pageRowCounter);
                outRun->blockCount++;
            }

            tableCatalogue.insertTable(outRun);
            nextRuns.emplace_back(outRunName);

            // cleanup input runs from this group
            for (size_t i = base; i < end; i++)
                tableCatalogue.deleteTable(runNames[i]);
        }

        runNames.swap(nextRuns);
        pass++;
    }

    return tableCatalogue.getTable(runNames.front());
}
