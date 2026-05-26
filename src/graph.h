
#include "table.h"
#pragma once
class Graph{
  public:
  string name;
  bool isDirected;
  Table* nodeTable;
  Table* edgeTable;

  Graph(string name, bool isDirected){
    this->name = name;
    this->isDirected = isDirected;
    string suffix = "";
    if( isDirected )
    {
      suffix = "_D";
    }
    else{
      suffix = "_U";
    }
    this->edgeTable = new Table( name + "_Edges" + suffix );
    this->nodeTable = new Table( name + "_Nodes" + suffix );
  }

  ~Graph(){
    delete nodeTable;
    delete edgeTable;
  }

};
