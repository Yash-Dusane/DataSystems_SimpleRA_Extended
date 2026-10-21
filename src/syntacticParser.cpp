#include "global.h"

bool syntacticParse()
{
    logger.log("syntacticParse");
    string possibleQueryType = tokenizedQuery[0];
    // cout<<"HERE"<<endl;
    // cout<<"PRINTING TOK"<<endl;
    // for(auto g: tokenizedQuery )cout<<g<<' ';cout<<endl;
    if (tokenizedQuery.size() < 2)
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }

    auto arrowIt = find(tokenizedQuery.begin(), tokenizedQuery.end(), "<-");
    int arrowPos = (arrowIt == tokenizedQuery.end()) ? -1 : (int)distance(tokenizedQuery.begin(), arrowIt);

    if (possibleQueryType == "CLEAR")
        return syntacticParseCLEAR();
    else if (possibleQueryType == "INDEX")
        return syntacticParseINDEX();
    else if (possibleQueryType == "LIST")
        return syntacticParseLIST();
    else if (possibleQueryType == "LOAD")
    {
        // cout<<"IN LOAD!!!"<<endl;
        // for(auto g: tokenizedQuery)cout<<g<<' ';cout<<endl;
        if( tokenizedQuery.size() > 1 and tokenizedQuery[1] == "GRAPH" ){
            return syntacticParseLOAD_GRAPH();
        }
        else{
            return syntacticParseLOAD();
        }
    }
    else if (possibleQueryType == "PRINT"){
        if(tokenizedQuery.size()>1 and tokenizedQuery[1]=="GRAPH"){
            return syntacticParsePRINT_GRAPH();
        }
        return syntacticParsePRINT();
    }
    else if (possibleQueryType == "RENAME")
        return syntacticParseRENAME();
    else if(possibleQueryType == "EXPORT"){
        if( tokenizedQuery.size() > 1 and tokenizedQuery[1] == "GRAPH" ){
            return syntacticParseEXPORT_GRAPH();
        }
        else{
            return syntacticParseEXPORT();
        }
    }
    else if(possibleQueryType == "SOURCE")
        return syntacticParseSOURCE();
    else if(possibleQueryType == "SETBUFFER")
        return syntacticParseSETBUFFER();
    else if(possibleQueryType == "SORT")
        return syntacticParseSORT();
    else if(possibleQueryType=="DEGREE"){
        return syntacticParseDEGREE();
    }
    else
    {
        if (arrowPos == -1 || tokenizedQuery.size() < (size_t)(arrowPos + 2))
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }

        // Multi-assignment is supported only for GROUP BY in Phase 2.
        if (arrowPos > 1)
        {
            if (tokenizedQuery[arrowPos + 1] == "GROUP")
                return syntacticParseGROUP_BY(arrowPos);
            cout << "SYNTAX ERROR" << endl;
            return false;
        }

        // Single-result assignment statements follow the original flow.
        if (arrowPos != 1)
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }

        possibleQueryType = tokenizedQuery[2];
        if (possibleQueryType == "PROJECT")
            return syntacticParsePROJECTION();
        else if (possibleQueryType == "SELECT")
            return syntacticParseSELECTION();
        else if (possibleQueryType == "JOIN")
            return syntacticParseJOIN();
        else if (possibleQueryType == "CROSS")
            return syntacticParseCROSS();
        else if (possibleQueryType == "DISTINCT")
            return syntacticParseDISTINCT();
        else if (possibleQueryType == "GROUP")
            return syntacticParseGROUP_BY(arrowPos);
        else if (possibleQueryType =="PATH"){
            // cout<<"HI"<<endl;
            return syntacticParsePATH();
        }
        else
        {
            cout << "SYNTAX ERROR" << endl;
            return false;
        }
    }
    return false;
}

ParsedQuery::ParsedQuery()
{
}

void ParsedQuery::clear()
{
    logger.log("ParseQuery::clear");
    this->queryType = UNDETERMINED;

    this->clearRelationName = "";

    this->crossResultRelationName = "";
    this->crossFirstRelationName = "";
    this->crossSecondRelationName = "";

    this->distinctResultRelationName = "";
    this->distinctRelationName = "";

    this->exportRelationName = "";

    this->indexingStrategy = NOTHING;
    this->indexColumnName = "";
    this->indexRelationName = "";

    this->joinBinaryOperator = NO_BINOP_CLAUSE;
    this->joinResultRelationName = "";
    this->joinFirstRelationName = "";
    this->joinSecondRelationName = "";
    this->joinFirstColumnName = "";
    this->joinSecondColumnName = "";
    this->joinConditionType = NO_JOIN_COND;
    this->joinArithmeticOperator = NO_ARITH_OP;
    this->joinArithmeticConstant = 0;
    this->joinHasWhereClause = false;
    this->joinWhereColumnName = "";
    this->joinWhereBinaryOperator = NO_BINOP_CLAUSE;
    this->joinWhereIntLiteral = 0;
    this->joinHasProjectClause = false;
    this->joinProjectColumns.clear();

    this->loadRelationName = "";

    this->printRelationName = "";

    this->projectionResultRelationName = "";
    this->projectionColumnList.clear();
    this->projectionRelationName = "";

    this->renameFromColumnName = "";
    this->renameToColumnName = "";
    this->renameRelationName = "";

    this->selectType = NO_SELECT_CLAUSE;
    this->selectionBinaryOperator = NO_BINOP_CLAUSE;
    this->selectionResultRelationName = "";
    this->selectionRelationName = "";
    this->selectionFirstColumnName = "";
    this->selectionSecondColumnName = "";
    this->selectionIntLiteral = 0;

    this->setBufferSize = 0;

    this->sortingStrategy = NO_SORT_CLAUSE;
    this->sortRelationName = "";
    this->sortColumnNames.clear();
    this->sortColumnStrategies.clear();
    this->sortHasTopClause = false;
    this->sortTopCount = 0;
    this->sortHasBottomClause = false;
    this->sortBottomCount = 0;

    this->sourceFileName = "";

    // graph
    this->isDirected = false;
    this->graphName = "";

    // path
    this->pathConditions.clear();
    this->pathDest = "";
    this->pathSrc = "";
    this->pathResultName = "";
    this-> degreeNodeID = -1;
    this->degreeGraphName="";

    this->groupByResultRelationNames.clear();
    this->groupByRelationName = "";
    this->groupByColumnNames.clear();
    this->groupByHavingType = NO_HAVING_EXPR;
    this->groupByHavingLhs = AggregateTerm();
    this->groupByHavingRhsAgg = AggregateTerm();
    this->groupByHavingRhsConst = 0;
    this->groupByHavingBinaryOperator = NO_BINOP_CLAUSE;
    this->groupByReturnAggregates.clear();
}

/**
 * @brief Checks to see if source file exists. Called when LOAD command is
 * invoked.
 *
 * @param tableName 
 * @return true 
 * @return false 
 */
bool isFileExists(string tableName)
{
    string fileName = "../data/" + tableName + ".csv";
    struct stat buffer;
    return (stat(fileName.c_str(), &buffer) == 0);
}

/**
 * @brief Checks to see if source file exists. Called when SOURCE command is
 * invoked.
 *
 * @param tableName 
 * @return true 
 * @return false 
 */
bool isQueryFile(string fileName){
    fileName = "../data/" + fileName + ".ra";
    struct stat buffer;
    return (stat(fileName.c_str(), &buffer) == 0);
}
