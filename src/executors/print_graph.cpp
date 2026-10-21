#include "global.h"
#define endl '\n'
void executePRINT_GRAPH();
bool semanticParsePRINT_GRAPH();
bool syntacticParsePRINT_GRAPH();

static void printCsvDataLines(const string &filePath, uint limit)
{
    ifstream fin(filePath);
    if (!fin.is_open())
        return;

    string line;
    // Skip header
    if (!getline(fin, line))
        return;

    uint printed = 0;
    while (printed < limit && getline(fin, line))
    {
        if (line.empty())
            continue;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        cout << line << endl;
        printed++;
    }
}

bool syntacticParsePRINT_GRAPH()
{
    if (tokenizedQuery.size() != 3)
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }
    parsedQuery.queryType = PRINT_GRAPH;
    parsedQuery.graphName = tokenizedQuery[2];
    return true;
}
bool semanticParsePRINT_GRAPH()
{
    if (!graphCatalogue.isGraph(parsedQuery.graphName))
    {
        cout << "SEMANTIC ERROR: Graph doesn't exist" << endl;
        return false;
    }
    return true;
}
void executePRINT_GRAPH()
{
    logger.log("executePRINT_GRAPH");
    GLOBAL_BLOCK_ACCESS_COUNT = 0;

    Graph *graph = graphCatalogue.getGraph(parsedQuery.graphName);
    cout << graph->nodeTable->rowCount << endl;
    if (!graph->isDirected)
    {
        cout << (graph->edgeTable->rowCount / 2) << endl;
    }
    else
    {
        cout << graph->edgeTable->rowCount << endl;
    }

    cout << (graph->isDirected ? "D" : "U") << endl;
    cout << endl;

    string nodeFile = "../data/" + graph->nodeTable->tableName + ".csv";
    string edgeFile = "../data/" + graph->edgeTable->tableName + ".csv";

    printCsvDataLines(nodeFile, PRINT_COUNT);
    cout << endl;
    printCsvDataLines(edgeFile, PRINT_COUNT);

    // cout << "Printed Graph... (Block Accesses: " << GLOBAL_BLOCK_ACCESS_COUNT << ")" << endl;
}
