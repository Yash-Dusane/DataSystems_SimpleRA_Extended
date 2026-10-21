#pragma once

#include "syntacticParser.h"
#include "table.h"

struct SortKeySpec
{
    int columnIndex;
    SortingStrategy strategy;
};

/**
 * @brief External stable sort over a contiguous range of rows in inputTable.
 *
 * @param sortedTableName Name of the output temp table to create (will be inserted into tableCatalogue).
 * @param inputTable Table to read from.
 * @param startRow 0-based start row (inclusive).
 * @param rowCount Number of rows to sort.
 * @param keys Sort keys (priority order).
 */
Table* externalSortRangeToTempTable(
    const string &sortedTableName,
    Table *inputTable,
    long long startRow,
    long long rowCount,
    const vector<SortKeySpec> &keys);

