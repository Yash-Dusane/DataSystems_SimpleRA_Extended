#include "semanticParser.h"

void executeCommand();

void executeCLEAR();
void executeCROSS();
void executeDISTINCT();
void executeEXPORT();
void executeINDEX();
void executeTRANSACTION();
void executeJOIN();
void executeLIST();
void executeLOAD();
void executePRINT();
void executePROJECTION();
void executeRENAME();
void executeSELECTION();
void executeSETBUFFER();
void executeSORT();
void executeGROUP_BY();
void executeSOURCE();
void executeLOAD_GRAPH();
void executeEXPORT_GRAPH();
void executePATH();
void executeDEGREE();
void executePRINT_GRAPH();
bool evaluateBinOp(int value1, int value2, BinaryOperator binaryOperator);
void printRowCount(int rowCount);
