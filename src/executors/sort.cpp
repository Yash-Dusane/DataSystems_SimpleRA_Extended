#include"global.h"
/**
 * @brief File contains method to process SORT commands.
 * 
 * syntax:
 * R <- SORT relation_name BY column_name IN sorting_order
 * 
 * sorting_order = ASC | DESC 
 */
bool syntacticParseSORT(){
    logger.log("syntacticParseSORT");
    if(tokenizedQuery.size()!= 8 || tokenizedQuery[4] != "BY" || tokenizedQuery[6] != "IN"){
        cout<<"SYNTAX ERROR"<<endl;
        return false;
    }
    parsedQuery.queryType = SORT;
    parsedQuery.sortResultRelationName = tokenizedQuery[0];
    parsedQuery.sortRelationName = tokenizedQuery[3];
    parsedQuery.sortColumnName = tokenizedQuery[5];
    string sortingStrategy = tokenizedQuery[7];
    if(sortingStrategy == "ASC")
        parsedQuery.sortingStrategy = ASC;
    else if(sortingStrategy == "DESC")
        parsedQuery.sortingStrategy = DESC;
    else{
        cout<<"SYNTAX ERROR"<<endl;
        return false;
    }
    return true;
}
// void executeSORT(){
//     logger.log("executeSORT");
//     return;
// }
bool semanticParseSORT(){
    logger.log("semanticParseSORT");

    if(tableCatalogue.isTable(parsedQuery.sortResultRelationName)){
        cout<<"SEMANTIC ERROR: Resultant relation already exists"<<endl;
        return false;
    }

    if(!tableCatalogue.isTable(parsedQuery.sortRelationName)){
        cout<<"SEMANTIC ERROR: Relation doesn't exist"<<endl;
        return false;
    }

    if(!tableCatalogue.isColumnFromTable(parsedQuery.sortColumnName, parsedQuery.sortRelationName)){
        cout<<"SEMANTIC ERROR: Column doesn't exist in relation"<<endl;
        return false;
    }

    return true;
}

void executeSORT(){
    logger.log("executeSORT");

    Table* table = tableCatalogue.getTable(parsedQuery.sortRelationName);
    string resultTableName = parsedQuery.sortResultRelationName;
    string columnName = parsedQuery.sortColumnName;
    
    int colIndex = table->getColumnIndex(columnName);
    if(colIndex == -1) return;
    string inputFile = table->sourceFileName;
    string outputFile = "../data/temp/" + resultTableName + ".csv";
    string sortOpts = "-t, -k" + to_string(colIndex + 1) + "n";
    if (parsedQuery.sortingStrategy == DESC) {
        sortOpts += "r";
    }
    string command = "(head -n 1 " + inputFile + " && tail -n +2 " + inputFile + " | sort " + sortOpts + ") > " + outputFile;

    int ret = system(command.c_str());
    if (ret != 0) {
        cout << "Error: System sort failed." << endl;
        return;
    }

    Table* resultTable = new Table();
    resultTable->tableName = resultTableName;
    resultTable->sourceFileName = outputFile;
    
    if (resultTable->load()) {
        tableCatalogue.insertTable(resultTable);
        cout << "Number of rows in " << resultTableName << ": " << resultTable->rowCount << endl;
    } else {
        cout << "Error: Failed to load sorted table." << endl;
        delete resultTable;
    }
}