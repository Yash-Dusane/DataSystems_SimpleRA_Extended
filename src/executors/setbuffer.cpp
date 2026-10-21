#include "global.h"
/**
 * @brief
 * SYNTAX: SETBUFFER K
 * 2 <= K <= 10
 */

bool syntacticParseSETBUFFER()
{
    logger.log("syntacticParseSETBUFFER");
    if (tokenizedQuery.size() != 2)
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }
    regex numeric("^[0-9]+$");
    if (!regex_match(tokenizedQuery[1], numeric))
    {
        cout << "SYNTAX ERROR" << endl;
        return false;
    }
    parsedQuery.queryType = SETBUFFER;
    parsedQuery.setBufferSize = (uint)stoul(tokenizedQuery[1]);
    return true;
}

bool semanticParseSETBUFFER()
{
    logger.log("semanticParseSETBUFFER");
    uint k = parsedQuery.setBufferSize;
    if (k < 2 || k > 10)
    {
        cout << "SEMANTIC ERROR" << endl;
        return false;
    }
    return true;
}

void executeSETBUFFER()
{
    logger.log("executeSETBUFFER");
    BLOCK_COUNT = parsedQuery.setBufferSize;
    bufferManager.setMaxPages(BLOCK_COUNT);
    return;
}

