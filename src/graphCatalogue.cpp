#include "graphCatalogue.h"
#include <bits/stdc++.h>


  void GraphCatalogue::insertGraph( Graph* graph ){
    this->graphs[ graph->name ] = graph;
  }
  
  void GraphCatalogue::deleteGraph( string graphName ){
    this->graphs.erase( graphName );
  }

  Graph* GraphCatalogue::getGraph( string graphName ){
    if( this->graphs.count( graphName )){
      return this->graphs[ graphName ];
    }
    return nullptr;
  }
  
  bool GraphCatalogue::isGraph( string graphName ){
    if( this->graphs.count( graphName )){
      return true;
    }
    return false;
  }

  // debug routine to be removed
  bool GraphCatalogue::ispresent(string name){
    return isGraph(name);
  }


