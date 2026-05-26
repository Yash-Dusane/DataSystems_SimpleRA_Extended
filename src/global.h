#include "executor.h"
#include "graphCatalogue.h"

extern float BLOCK_SIZE;
extern uint BLOCK_COUNT;
extern uint PRINT_COUNT;
extern vector<string> tokenizedQuery;
extern ParsedQuery parsedQuery;
extern TableCatalogue tableCatalogue;
extern GraphCatalogue graphCatalogue;
extern BufferManager bufferManager;

extern int GLOBAL_BLOCK_ACCESS_COUNT;