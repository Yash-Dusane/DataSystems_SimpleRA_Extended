#pragma once

#include "syntacticParser.h"

bool splitQualifiedColumn(const string &qualified, string &relationName, string &columnName);
bool parseAggregateTermToken(const string &token, AggregateTerm &out);
string aggregateFunctionToString(AggregateFunction fn);
string makeReturnAggregateColumnName(const AggregateTerm &term);

