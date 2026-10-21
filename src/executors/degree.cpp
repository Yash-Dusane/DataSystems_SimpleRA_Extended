#include "global.h"

int getStartBlockTemp(Table *table, int key)
{
  int l = 0, r = table->blockCount - 1;
  int ans = table->blockCount;

  while (l <= r)
  {
    int mid = l + (r - l) / 2;
    Page p(table->tableName, mid);
    vector<int> row = p.getRow(0);

    if (row.empty())
    {
      r = mid - 1;
      continue;
    }

    if (row[0] >= key)
    {
      ans = mid;
      r = mid - 1;
    }
    else
    {
      l = mid + 1;
    }
  }

  if (ans == 0)
    return 0;
  if (ans == table->blockCount)
    return table->blockCount - 1;
  return ans - 1;
}

bool syntacticParseDEGREE()
{
  if (tokenizedQuery.size() != 3)
  {
    cout << "SYNTAX ERROR" << endl;
    return false;
  }
  parsedQuery.queryType = DEGREE;
  parsedQuery.graphName = tokenizedQuery[1];

  string nodeIDStr = tokenizedQuery[2];
  if (nodeIDStr.find_first_not_of("0123456789") != string::npos)
  {
    cout << "SYNTAX ERROR" << endl;
    return false;
  }
  parsedQuery.degreeNodeID = stoi(nodeIDStr);
  return true;
}

bool semanticParseDEGREE()
{
  if (!graphCatalogue.isGraph(parsedQuery.graphName))
  {
    cout << "SEMANTIC ERROR: Graph doesn't exist" << endl;
    return false;
  }
  return true;
}

void executeDEGREE()
{
  logger.log("executeDEGREE");
  GLOBAL_BLOCK_ACCESS_COUNT = 0;

  string graphName = parsedQuery.graphName;
  int targetNode = parsedQuery.degreeNodeID;
  Graph *graph = graphCatalogue.getGraph(graphName);

  bool nodeExists = false;
  Cursor nodeCursor = graph->nodeTable->getCursor();
  vector<int> row;
  while (!(row = nodeCursor.getNext()).empty())
  {
    if (row[0] == targetNode)
    {
      nodeExists = true;
      break;
    }
  }

  if (!nodeExists)
  {
    cout << "Node does not exist" << endl;
    return;
  }

  int degree = 0;
  int startBlock = getStartBlockTemp(graph->edgeTable, targetNode);

  if (!graph->isDirected)
  {
    Cursor edgeCursor(graph->edgeTable->tableName, startBlock);
    vector<int> eRow;
    while (!(eRow = edgeCursor.getNext()).empty())
    {
      if (eRow[0] < targetNode)
        continue;
      if (eRow[0] > targetNode)
        break;

      if (eRow[0] == targetNode)
      {
        degree++;
      }
    }
  }
  else
  {
    Cursor outCursor(graph->edgeTable->tableName, startBlock);
    vector<int> eRow;
    while (!(eRow = outCursor.getNext()).empty())
    {
      if (eRow[0] < targetNode)
        continue;
      if (eRow[0] > targetNode)
        break;
      if (eRow[0] == targetNode)
        degree++;
    }

    Cursor inCursor = graph->edgeTable->getCursor();
    while (!(eRow = inCursor.getNext()).empty())
    {
      if (eRow[1] == targetNode)
      {
        degree++;
      }
    }
  }

  cout << degree << endl;
}
