#include "global.h"

#define endl '\n'

bool syntacticParseLOAD_GRAPH()
{
  if (tokenizedQuery.size() < 4)
  {
    cout << "SYNTAX ERROR" << endl;
    return false;
  }
  parsedQuery.queryType = LOAD_GRAPH;
  parsedQuery.graphName = tokenizedQuery[2];
  string type = tokenizedQuery[3];
  if (type == "U")
  {
    parsedQuery.isDirected = false;
  }
  else if (type == "D")
  {
    parsedQuery.isDirected = true;
  }
  else
  {
    cout << "SYNTAX ERROR" << endl;
    return false;
  }
  return true;
}

bool semanticParseLOAD_GRAPH()
{
  if (graphCatalogue.isGraph(parsedQuery.graphName))
  {
    cout << "SEMANTIC ERROR: Graph already exists" << endl;
    return false;
  }
  string suffix = parsedQuery.isDirected ? "_D" : "_U";
  string nodeFile = parsedQuery.graphName + "_Nodes" + suffix;
  string edgeFile = parsedQuery.graphName + "_Edges" + suffix;
  // cout<<"NODE FILE IS"<<nodeFile<<endl;
  // cout<<"Edge FILE IS"<<edgeFile<<endl;
  if (!isFileExists(nodeFile))
  {
    cout << "SEMANTIC ERROR: Data file doesn't exist" << endl;
    return false;
  }
  if (!isFileExists(edgeFile))
  {
    cout << "SEMANTIC ERROR: Data file doesn't exist" << endl;
    return false;
  }
  return true;
}

void executeLOAD_GRAPH()
{
  GLOBAL_BLOCK_ACCESS_COUNT = 0;
  Graph *graph = new Graph(parsedQuery.graphName, parsedQuery.isDirected);

  if (!graph->nodeTable->load())
  {
    delete graph;
    cout << "ERROR: Unable to Load Nodes" << endl;
    return;
  }
  string orgname = "../data/" + parsedQuery.graphName + (parsedQuery.isDirected ? "_Edges_D.csv" : "_Edges_U.csv");
  string tempname = "../data/temp/" + graph->edgeTable->tableName + ".csv";

  if (parsedQuery.isDirected)
  {
    string cmd = "cp " + orgname + " " + tempname;
    system(cmd.c_str());
  }
  else
  {
    ifstream fin(orgname);
    ofstream fout(tempname);

    if (!fin.is_open() or !fout.is_open())
    {
      cout << "Error handling edge file" << endl;
      delete graph;
      return;
    }

    string line;
    if (getline(fin, line))
      fout << line << endl;

    while (getline(fin, line))
    {
      if (line.empty())
        continue;
      fout << line << endl;

      stringstream ss(line);
      string token;
      vector<string> tokens;
      while (getline(ss, token, ','))
        tokens.push_back(token);

      if (tokens.size() >= 3)
      {
        fout << tokens[1] << "," << tokens[0] << "," << tokens[2];
        for (size_t i = 3; i < tokens.size(); i++)
        {
          fout << "," << tokens[i];
        }
        fout << endl;
      }
    }
    fin.close();
    fout.close();
    // cout << "Loaded Graph... (Block Accesses: " << GLOBAL_BLOCK_ACCESS_COUNT << ")" << endl;
  }

  string sortedFile = tempname + ".sorted";
  string sortCmd = "(head -n 1 " + tempname + " && tail -n +2 " + tempname + " | sort -t, -k1n) > " + sortedFile;
  system(sortCmd.c_str());

  string mvCmd = "mv " + sortedFile + " " + tempname;
  system(mvCmd.c_str());
  graph->edgeTable->sourceFileName = tempname;

  if (!graph->edgeTable->load())
  {
    delete graph;
    cout << "ERROR: Unable to Load edges" << endl;
    return;
  }
  tableCatalogue.insertTable(graph->nodeTable);
  tableCatalogue.insertTable(graph->edgeTable);
  graphCatalogue.insertGraph(graph);
  logger.log("Successfully added graph to the catalog");
  int f = 1;
  if (!parsedQuery.isDirected)
    f = 2;
  // cout << "Loaded Graph. #Nodes = " << graph->nodeTable->rowCount << ", #Edges =  " << graph->edgeTable->rowCount/f << endl;
  cout << "Loaded Graph.Node Count:" << graph->nodeTable->rowCount << ",Edge Count:" << graph->edgeTable->rowCount / f << endl;
}
