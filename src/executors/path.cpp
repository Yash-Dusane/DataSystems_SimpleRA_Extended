#include "global.h"
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <queue>
#include <map>
#include <set>
#include <algorithm>
#include <climits>
#include <unordered_map>
#include <unordered_set>

using namespace std;

struct fast_condition {
    int colIdx;
    int val;
};

struct entry {
    int val;
    int nd;
    bool operator>(const entry &other) const {
        return val > other.val;
    }
};

bool checkFastConditions(const vector<int>& row, const vector<fast_condition>& constrs) {
    for (const auto& c : constrs) {
        if (c.colIdx < (int)row.size() && row[c.colIdx] != c.val) {
            return false;
        }
    }
    return true;
}

void generateConfigs(
    size_t idx,
    const vector<PathCondition> &rawConds,
    vector<pair<string, int>> &currN,
    vector<pair<string, int>> &currE,
    vector<pair<vector<pair<string, int>>, vector<pair<string, int>>>> &allConfigs,
    const vector<string> &nodeCols,
    const vector<string> &edgeCols)
{
  if (idx == rawConds.size()) {
    allConfigs.push_back({currN, currE});
    return;
  }

  const auto &cond = rawConds[idx];
  const vector<string> &targetCols = cond.isNode ? nodeCols : edgeCols;
  vector<pair<string, int>> *targetList = cond.isNode ? &currN : &currE;
  
  vector<string> attrs;
  if (cond.attribute == "ANY") {
    size_t start = cond.isNode ? 1 : 3; 
    for (size_t i = start; i < targetCols.size(); i++) attrs.push_back(targetCols[i]);
  } else {
    attrs.push_back(cond.attribute);
  }
  
  vector<int> values;
  if (cond.isUniform) values = {0, 1};
  else values = {cond.val};

  for (const string &attr : attrs) {
    for (int val : values) {
      targetList->push_back({attr, val});
      generateConfigs(idx + 1, rawConds, currN, currE, allConfigs, nodeCols, edgeCols);
      targetList->pop_back();
    }
  }
}

int getStartBlock(Table* table, int key) {
    int l = 0, r = table->blockCount - 1;
    int ans = table->blockCount;

    while (l <= r) {
        int mid = l + (r - l) / 2;
        Page p(table->tableName, mid); 
        vector<int> row = p.getRow(0);

        if (row.empty()) {
            r = mid - 1; 
            continue;
        }

        if (row[0] >= key) {
            ans = mid;
            r = mid - 1;
        } else {
            l = mid + 1;
        }
    }

    if (ans == 0) return 0;
    if (ans == table->blockCount) return table->blockCount - 1;
    return ans - 1; 
}

bool syntacticParsePATH() {
  if (tokenizedQuery.size() < 6 or tokenizedQuery[1] != "<-" or tokenizedQuery[2] != "PATH") {
    cout << "SYNTAX ERROR" << endl;
    return false;
  }
  parsedQuery.queryType = PATH;
  parsedQuery.pathResultName = tokenizedQuery[0];
  parsedQuery.graphName = tokenizedQuery[3];
  parsedQuery.pathSrc = tokenizedQuery[4];
  parsedQuery.pathDest = tokenizedQuery[5];
  parsedQuery.pathConditions.clear();

  size_t index = 6;
  if (index < tokenizedQuery.size()) {
    if (tokenizedQuery[index] != "WHERE") {
      cout << "SYNTAX ERROR" << endl;
      return false;
    }
    index++; 
    
    while (index < tokenizedQuery.size()) {
      string str = tokenizedQuery[index];
      if (str == "AND") {
         index++;
         if (index >= tokenizedQuery.size()) { cout << "SYNTAX ERROR" << endl; return false; }
         str = tokenizedQuery[index];
      }

      PathCondition cond;
      size_t pos_n = str.find("(N)");
      size_t pos_e = str.find("(E)");

      if (pos_n != string::npos) {
        cond.isNode = true;
        cond.attribute = str.substr(0, pos_n);
      } else if (pos_e != string::npos) {
        cond.isNode = false;
        cond.attribute = str.substr(0, pos_e);
      } else {
        cout << "SYNTAX ERROR" << endl;
        return false;
      }

      index++;
      if (index < tokenizedQuery.size() && tokenizedQuery[index] == "==") {
        cond.isUniform = false;
        index++;
        if (index >= tokenizedQuery.size()) { cout << "SYNTAX ERROR" << endl; return false; }
        cond.val = stoi(tokenizedQuery[index]);
        index++;
      } else {
        cond.isUniform = true;
        cond.val = -1;
      }
      parsedQuery.pathConditions.push_back(cond);
    }
  }
  return true;
}

bool semanticParsePATH() {
  if (!graphCatalogue.isGraph(parsedQuery.graphName)) {
    cout << "SEMANTIC ERROR: Graph doesn't exist" << endl;
    return false;
  }
  return true;
}

int inf = 1e9 + 7;

void executePATH() {
  GLOBAL_BLOCK_ACCESS_COUNT = 0;
  Graph *graph = graphCatalogue.getGraph(parsedQuery.graphName);
  int srcID = stoi(parsedQuery.pathSrc);
  int destID = stoi(parsedQuery.pathDest);
  int val = inf;
  vector<int> best_path;

  vector<pair<vector<pair<string, int>>, vector<pair<string, int>>>> stringConfigs;
  vector<pair<string, int>> tempN, tempE;
  generateConfigs(0, parsedQuery.pathConditions, tempN, tempE, stringConfigs,
                   graph->nodeTable->columns, graph->edgeTable->columns);

  if (stringConfigs.empty()) stringConfigs.push_back({{}, {}});

  for (const auto &strConfig : stringConfigs) {
      
    vector<fast_condition> nodeConds, edgeConds;
    
    for(auto& p : strConfig.first) {
        int idx = graph->nodeTable->getColumnIndex(p.first);
        if(idx != -1) nodeConds.push_back({idx, p.second});
    }
    for(auto& p : strConfig.second) {
        int idx = graph->edgeTable->getColumnIndex(p.first);
        if(idx != -1) edgeConds.push_back({idx, p.second});
    }

    vector<bool> valid;
    bool sFound = false, dFound = false;

    Cursor nodeCursor = graph->nodeTable->getCursor();
    vector<int> row;
    while (!(row = nodeCursor.getNext()).empty()) {
      int nid = row[0];
      if (nid == srcID) sFound = true;
      if (nid == destID) dFound = true;
      
      if (checkFastConditions(row, nodeConds)) {
        if (nid >= (int)valid.size()) valid.resize(nid + 1, false);
        valid[nid] = true;
      }
    }

    if (!sFound || !dFound) { cout << "Node does not exist" << endl; return; }
    
    if (srcID >= (int)valid.size() || !valid[srcID] || 
        destID >= (int)valid.size() || !valid[destID]) continue;

    priority_queue<entry, vector<entry>, greater<entry>> pq;
    map<int, int> dist;
    map<int, int> parent;

    dist[srcID] = 0;
    pq.push({0, srcID});
    parent[srcID] = -1;

    while (!pq.empty()) {
      entry top = pq.top();
      pq.pop();

      if (top.val > dist[top.nd]) continue;
      
      if (top.nd == destID) {
        if (top.val < val) {
          val = top.val;
          best_path.clear();
          int curr = destID;
          while (curr != -1) { best_path.push_back(curr); curr = parent[curr]; }
          reverse(best_path.begin(), best_path.end());
        }
        break; 
      }

      int startBlock = getStartBlock(graph->edgeTable, top.nd);

      Cursor edgeCursor(graph->edgeTable->tableName, startBlock);
      vector<int> eRow;
      while (!(eRow = edgeCursor.getNext()).empty()) {
        int src = eRow[0];
        if (src < top.nd) continue;
        if (src > top.nd) break; 
        
        if (!checkFastConditions(eRow, edgeConds)) continue;

        int v = eRow[1];
        int w = eRow[2];

        if (v >= (int)valid.size() || !valid[v]) continue;

        if (dist.find(v) == dist.end() || top.val + w < dist[v]) {
          dist[v] = top.val + w;
          parent[v] = top.nd;
          pq.push({dist[v], v}); 
        }
      }
    }
  }

  if (val == inf) cout << "FALSE" << endl;
  else {
    cout << "TRUE " << val << endl;
    
    string resName = parsedQuery.pathResultName;
    Graph *resGraph = new Graph(resName, graph->isDirected);

    string srcNodeFile = "../data/" + graph->nodeTable->tableName + ".csv";
    string srcEdgeFile = "../data/" + graph->edgeTable->tableName + ".csv";

    string outNodeFile = "../data/" + resGraph->nodeTable->tableName + ".csv";
    string outEdgeFile = "../data/" + resGraph->edgeTable->tableName + ".csv";

    unordered_set<int> neededNodes(best_path.begin(), best_path.end());
    unordered_map<int, string> nodeLines;
    nodeLines.reserve(neededNodes.size());

    ifstream nIn(srcNodeFile);
    string nodeHeader;
    if (getline(nIn, nodeHeader))
    {
      string line;
      while (getline(nIn, line))
      {
        if (line.empty())
          continue;
        size_t commaPos = line.find(',');
        if (commaPos == string::npos)
          continue;
        int nid = stoi(line.substr(0, commaPos));
        if (neededNodes.count(nid))
          nodeLines[nid] = line;
      }
    }

    vector<pair<int, int>> pathPairs;
    pathPairs.reserve(best_path.size() > 0 ? best_path.size() - 1 : 0);
    for (size_t i = 0; i + 1 < best_path.size(); ++i)
      pathPairs.push_back({best_path[i], best_path[i + 1]});

    auto makeKey = [](int a, int b) -> long long {
      return (static_cast<long long>(a) << 32) ^ static_cast<unsigned int>(b);
    };

    unordered_set<long long> neededEdges;
    neededEdges.reserve(pathPairs.size() * 2);
    for (const auto &p : pathPairs)
    {
      int a = p.first, b = p.second;
      if (!graph->isDirected && a > b)
        swap(a, b);
      neededEdges.insert(makeKey(a, b));
    }

    unordered_map<long long, string> edgeLines;
    edgeLines.reserve(neededEdges.size());

    ifstream eIn(srcEdgeFile);
    string edgeHeader;
    if (getline(eIn, edgeHeader))
    {
      string line;
      while (getline(eIn, line))
      {
        if (line.empty())
          continue;
        size_t c1 = line.find(',');
        if (c1 == string::npos)
          continue;
        size_t c2 = line.find(',', c1 + 1);
        if (c2 == string::npos)
          continue;
        int a = stoi(line.substr(0, c1));
        int b = stoi(line.substr(c1 + 1, c2 - c1 - 1));
        if (!graph->isDirected && a > b)
          swap(a, b);
        long long key = makeKey(a, b);
        if (neededEdges.count(key))
          edgeLines[key] = line;
      }
    }

    {
      ofstream nOut(outNodeFile, ios::trunc);
      nOut << nodeHeader << endl;
      for (int nid : best_path)
      {
        auto it = nodeLines.find(nid);
        if (it != nodeLines.end())
          nOut << it->second << endl;
      }
    }

    {
      ofstream eOut(outEdgeFile, ios::trunc);
      eOut << edgeHeader << endl;
      for (const auto &p : pathPairs)
      {
        int a = p.first, b = p.second;
        if (!graph->isDirected && a > b)
          swap(a, b);
        long long key = makeKey(a, b);
        auto it = edgeLines.find(key);
        if (it != edgeLines.end())
          eOut << it->second << endl;
      }
    }

    string edgeTemp = "../data/temp/" + resGraph->edgeTable->tableName + ".csv";
    if (resGraph->isDirected)
    {
      string cmd = "cp " + outEdgeFile + " " + edgeTemp;
      system(cmd.c_str());
    }
    else
    {
      ifstream fin(outEdgeFile);
      ofstream fout(edgeTemp);
      if (fin.is_open() && fout.is_open())
      {
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
              fout << "," << tokens[i];
            fout << endl;
          }
        }
      }
      fin.close();
      fout.close();
    }

    string sortedFile = edgeTemp + ".sorted";
    string sortCmd = "(head -n 1 " + edgeTemp + " && tail -n +2 " + edgeTemp + " | sort -t, -k1n) > " + sortedFile;
    system(sortCmd.c_str());
    string mvCmd = "mv " + sortedFile + " " + edgeTemp;
    system(mvCmd.c_str());

    resGraph->edgeTable->sourceFileName = edgeTemp;

    resGraph->nodeTable->load();
    resGraph->edgeTable->load();
    tableCatalogue.insertTable(resGraph->nodeTable);
    tableCatalogue.insertTable(resGraph->edgeTable);
    graphCatalogue.insertGraph(resGraph);
  }
  // cout<<"Block accesses "<< GLOBAL_BLOCK_ACCESS_COUNT<<endl;
}
