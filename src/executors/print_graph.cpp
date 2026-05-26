#include "global.h"
#define endl '\n'
void executePRINT_GRAPH();
bool semanticParsePRINT_GRAPH();
bool syntacticParsePRINT_GRAPH();

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

    Table *nodeTable = graph->nodeTable;
    Cursor nodeCursor = nodeTable->getCursor();
    vector<int> row;
    uint count = 0;

    while (count < PRINT_COUNT and !(row = nodeCursor.getNext()).empty())
    {
        nodeTable->writeRow(row, cout);
        count++;
    }

    cout << endl;

    Table *edgeTable = graph->edgeTable;
    Cursor edgeCursor = edgeTable->getCursor();
    count = 0;

    while (count < PRINT_COUNT and !(row = edgeCursor.getNext()).empty())
    {
        if (!graph->isDirected)
        {
            if (row[0] > row[1])
            {
                continue;
            }
        }

        edgeTable->writeRow(row, cout);
        count++;
    }

    // cout << "Printed Graph... (Block Accesses: " << GLOBAL_BLOCK_ACCESS_COUNT << ")" << endl;
}