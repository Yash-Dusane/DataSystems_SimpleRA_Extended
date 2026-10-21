#include "phase2_parse_utils.h"

#include <regex>

bool splitQualifiedColumn(const string &qualified, string &relationName, string &columnName)
{
    size_t pos = qualified.find('.');
    if (pos == string::npos || pos == 0 || pos + 1 >= qualified.size())
        return false;
    if (qualified.find('.', pos + 1) != string::npos)
        return false;
    relationName = qualified.substr(0, pos);
    columnName = qualified.substr(pos + 1);
    return true;
}

string aggregateFunctionToString(AggregateFunction fn)
{
    switch (fn)
    {
    case AGG_MAX:
        return "MAX";
    case AGG_MIN:
        return "MIN";
    case AGG_COUNT:
        return "COUNT";
    case AGG_SUM:
        return "SUM";
    case AGG_AVG:
        return "AVG";
    default:
        return "";
    }
}

bool parseAggregateTermToken(const string &token, AggregateTerm &out)
{
    static const regex aggRe("^(MAX|MIN|COUNT|SUM|AVG)\\(([^)]*)\\)$");
    smatch m;
    if (!regex_match(token, m, aggRe))
        return false;
    string fn = m[1].str();
    string attr = m[2].str();

    out = AggregateTerm();
    if (fn == "MAX")
        out.function = AGG_MAX;
    else if (fn == "MIN")
        out.function = AGG_MIN;
    else if (fn == "COUNT")
        out.function = AGG_COUNT;
    else if (fn == "SUM")
        out.function = AGG_SUM;
    else if (fn == "AVG")
        out.function = AGG_AVG;
    else
        return false;

    if (attr == "*")
    {
        out.isStar = true;
        out.attribute = "*";
    }
    else
    {
        out.isStar = false;
        out.attribute = attr;
    }
    return true;
}

string makeReturnAggregateColumnName(const AggregateTerm &term)
{
    if (term.function == AGG_COUNT && term.isStar)
        return "COUNT";
    return aggregateFunctionToString(term.function) + term.attribute;
}

