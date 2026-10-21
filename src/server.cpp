//Server Code
#include "global.h"

using namespace std;

int GLOBAL_BLOCK_ACCESS_COUNT=0;
float BLOCK_SIZE = 1;
uint BLOCK_COUNT = 10;
uint PRINT_COUNT = 1e9+7;
Logger logger;
vector<string> tokenizedQuery;
ParsedQuery parsedQuery;
BufferManager bufferManager;
TableCatalogue tableCatalogue;
GraphCatalogue graphCatalogue;

void doCommand()
{
    logger.log("doCommand");
    if (syntacticParse() && semanticParse())
        executeCommand();
    return;
}

int main(void)
{

    regex delim("[^\\s,]+");
    string command;
    system("rm -rf ../data/temp");
    system("mkdir ../data/temp");

    while(!cin.eof())
    {
        cout << "\n> ";
        tokenizedQuery.clear();
        parsedQuery.clear();
        logger.log("\nReading New Command: ");
        getline(cin, command);
        logger.log(command);


        auto words_begin = std::sregex_iterator(command.begin(), command.end(), delim);
        auto words_end = std::sregex_iterator();
        for (std::sregex_iterator i = words_begin; i != words_end; ++i)
            tokenizedQuery.emplace_back((*i).str());

        // strip trailing semicolons (queries in docs often end with ';')
        vector<string> cleanedTokens;
        cleanedTokens.reserve(tokenizedQuery.size());
        for (auto &tok : tokenizedQuery)
        {
            while (!tok.empty() && tok.back() == ';')
                tok.pop_back();
            if (!tok.empty())
                cleanedTokens.emplace_back(tok);
        }
        tokenizedQuery.swap(cleanedTokens);

        if (tokenizedQuery.size() == 1 && tokenizedQuery.front() == "QUIT")
        {
            break;
        }

        if (tokenizedQuery.empty())
        {
            continue;
        }

        if (tokenizedQuery.size() == 1)
        {
            cout << "SYNTAX ERROR" << endl;
            continue;
        }

        doCommand();
    }
}
