#include "global.h"

#define endl '\n'

bool syntacticParseEXPORT_GRAPH();
bool semanticParseEXPORT_GRAPH();
void executeEXPORT_GRAPH();

bool syntacticParseEXPORT_GRAPH(){
  if( tokenizedQuery.size()!=3 ){
    cout<<"SYNTAX ERROR"<<endl;
    return false;
  }
  parsedQuery.queryType = EXPORT_GRAPH;
  parsedQuery.graphName = tokenizedQuery[2];
  return true;
}

bool semanticParseEXPORT_GRAPH(){
  if( !graphCatalogue.isGraph( parsedQuery.graphName )){
    cout<<"SEMANTIC ERROR: Graph doesn't exist"<<endl; return false;
  }
  return true;
}

void executeEXPORT_GRAPH(){
  // cout<<"Hi"<<endl;
  GLOBAL_BLOCK_ACCESS_COUNT = 0;
  logger.log("Successfully exported the graph");
  Graph* graph = graphCatalogue.getGraph(parsedQuery.graphName);
  graph->nodeTable->makePermanent();
  if(graph->isDirected){
    graph->edgeTable->makePermanent();
  }
  else{
    string tempsrc = "../data/"+ graph->edgeTable->tableName + ".csv";
    ofstream fout (tempsrc, ios::out);
    graph->edgeTable->writeRow(graph->edgeTable->columns, fout);

    Cursor cursor = graph->edgeTable->getCursor();
    vector<int>row;
    while(!(row=cursor.getNext()).empty()){
      if(row[0]<=row[1]){
        graph->edgeTable->writeRow(row, fout);
      }
    }
    fout.close();
    bufferManager.deleteFile(graph->edgeTable->sourceFileName);
  }
  // cout<<"Graph Successfully exported";
  // cout << "Exported Graph... (Block Accesses: " << GLOBAL_BLOCK_ACCESS_COUNT << ")" << endl;
}