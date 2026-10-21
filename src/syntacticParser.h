#pragma once

#include "tableCatalogue.h"

using namespace std;

struct PathCondition{
    bool isNode;
    bool isUniform;
    int val;
    string attribute;
};


enum QueryType
{
    CLEAR,
    CROSS,
    DISTINCT,
    EXPORT,
    INDEX,
    TRANSACTION,
    JOIN,
    LIST,
    LOAD,
    PRINT,
	PROJECTION,
	RENAME,
	SELECTION,
	SETBUFFER,
	SORT,
	GROUP_BY,
	SOURCE,
	LOAD_GRAPH,
	EXPORT_GRAPH,
	PATH,
    DEGREE,
    PRINT_GRAPH,
    UNDETERMINED
};

enum BinaryOperator
{
    LESS_THAN,
    GREATER_THAN,
    LEQ,
    GEQ,
    EQUAL,
    NOT_EQUAL,
    NO_BINOP_CLAUSE
};

enum SortingStrategy
{
	ASC,
	DESC,
	NO_SORT_CLAUSE
};

enum ArithmeticOperator
{
	PLUS,
	MINUS,
	NO_ARITH_OP
};

enum JoinConditionType
{
	JOIN_ATTR_EQ,
	JOIN_ARITH_EQ,
	NO_JOIN_COND
};

enum AggregateFunction
{
	AGG_MAX,
	AGG_MIN,
	AGG_COUNT,
	AGG_SUM,
	AGG_AVG,
	NO_AGG
};

enum HavingExpressionType
{
	HAVING_AGG_CONST,
	HAVING_AGG_AGG,
	NO_HAVING_EXPR
};

struct AggregateTerm
{
	AggregateFunction function = NO_AGG;
	string attribute = "";
	bool isStar = false;
};

enum SelectType
{
	COLUMN,
	INT_LITERAL,
    NO_SELECT_CLAUSE
};

class ParsedQuery
{

public:
    QueryType queryType = UNDETERMINED;

    string transactionFileName = "";

    string clearRelationName = "";

    string crossResultRelationName = "";
    string crossFirstRelationName = "";
    string crossSecondRelationName = "";

    string distinctResultRelationName = "";
    string distinctRelationName = "";

    string exportRelationName = "";

    IndexingStrategy indexingStrategy = NOTHING;
    string indexColumnName = "";
    string indexRelationName = "";

    BinaryOperator joinBinaryOperator = NO_BINOP_CLAUSE;
    string joinResultRelationName = "";
    string joinFirstRelationName = "";
	string joinSecondRelationName = "";
	string joinFirstColumnName = "";
	string joinSecondColumnName = "";
	JoinConditionType joinConditionType = NO_JOIN_COND;
	ArithmeticOperator joinArithmeticOperator = NO_ARITH_OP;
	int joinArithmeticConstant = 0;
	bool joinHasWhereClause = false;
	string joinWhereColumnName = "";
	BinaryOperator joinWhereBinaryOperator = NO_BINOP_CLAUSE;
	int joinWhereIntLiteral = 0;
	bool joinHasProjectClause = false;
	vector<string> joinProjectColumns;

    string loadRelationName = "";

    string printRelationName = "";

    string projectionResultRelationName = "";
    vector<string> projectionColumnList;
    string projectionRelationName = "";

    string renameFromColumnName = "";
    string renameToColumnName = "";
    string renameRelationName = "";

    SelectType selectType = NO_SELECT_CLAUSE;
    BinaryOperator selectionBinaryOperator = NO_BINOP_CLAUSE;
    string selectionResultRelationName = "";
    string selectionRelationName = "";
    string selectionFirstColumnName = "";
	string selectionSecondColumnName = "";
	int selectionIntLiteral = 0;

	uint setBufferSize = 0;

	SortingStrategy sortingStrategy = NO_SORT_CLAUSE;
	string sortRelationName = "";
	vector<string> sortColumnNames;
	vector<SortingStrategy> sortColumnStrategies;
	bool sortHasTopClause = false;
	long long sortTopCount = 0;
	bool sortHasBottomClause = false;
	long long sortBottomCount = 0;

	string sourceFileName = "";

    // graph
    string graphName = "";
    bool isDirected = false;

    // path
    string pathSrc="";
    string pathDest= "";
    string pathResultName = "";

    vector<PathCondition> pathConditions;

	//degree
	int degreeNodeID = -1;
	string degreeGraphName = "";

	// group by
	vector<string> groupByResultRelationNames;
	string groupByRelationName = "";
	vector<string> groupByColumnNames;
	HavingExpressionType groupByHavingType = NO_HAVING_EXPR;
	AggregateTerm groupByHavingLhs;
	AggregateTerm groupByHavingRhsAgg;
	int groupByHavingRhsConst = 0;
	BinaryOperator groupByHavingBinaryOperator = NO_BINOP_CLAUSE;
	vector<AggregateTerm> groupByReturnAggregates;
	ParsedQuery();
	void clear();
};

bool syntacticParse();
bool syntacticParseCLEAR();
bool syntacticParseCROSS();
bool syntacticParseDISTINCT();
bool syntacticParseEXPORT();
bool syntacticParseINDEX();
bool syntacticParseTRANSACTION();
bool syntacticParseJOIN();
bool syntacticParseLIST();
bool syntacticParseLOAD();
bool syntacticParsePRINT();
bool syntacticParsePROJECTION();
bool syntacticParseRENAME();
bool syntacticParseSELECTION();
bool syntacticParseSETBUFFER();
bool syntacticParseSORT();
bool syntacticParseGROUP_BY(int arrowPos = 1);
bool syntacticParseSOURCE();

bool syntacticParseLOAD_GRAPH();
bool syntacticParseEXPORT_GRAPH();
bool syntacticParsePATH();
bool syntacticParseDEGREE();
bool syntacticParsePRINT_GRAPH();
bool isFileExists(string tableName);
bool isQueryFile(string fileName);
