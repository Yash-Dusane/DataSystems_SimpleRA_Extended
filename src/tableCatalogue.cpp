#include "global.h"

void TableCatalogue::insertTable(Table* table)
{
    logger.log("TableCatalogue::~insertTable"); 
    this->tables[table->tableName] = table;
}
void TableCatalogue::deleteTable(string tableName)
{
    logger.log("TableCatalogue::deleteTable"); 
    auto it = this->tables.find(tableName);
    if (it == this->tables.end() || it->second == nullptr)
        return;
    it->second->unload();
    delete it->second;
    this->tables.erase(it);
}
Table* TableCatalogue::getTable(string tableName)
{
    logger.log("TableCatalogue::getTable"); 
    auto it = this->tables.find(tableName);
    if (it == this->tables.end())
        return nullptr;
    return it->second;
}
bool TableCatalogue::isTable(string tableName)
{
    logger.log("TableCatalogue::isTable"); 
    if (this->tables.count(tableName))
        return true;
    return false;
}

bool TableCatalogue::isColumnFromTable(string columnName, string tableName)
{
    logger.log("TableCatalogue::isColumnFromTable"); 
    if (this->isTable(tableName))
    {
        Table* table = this->getTable(tableName);
        if (table->isColumn(columnName))
            return true;
    }
    return false;
}

void TableCatalogue::print()
{
    logger.log("TableCatalogue::print"); 
    cout << "\nRELATIONS" << endl;

    int rowCount = 0;
    for (auto rel : this->tables)
    {
        cout << rel.first << endl;
        rowCount++;
    }
    printRowCount(rowCount);
}

TableCatalogue::~TableCatalogue(){
    logger.log("TableCatalogue::~TableCatalogue"); 
    for(auto table: this->tables){
        table.second->unload();
        delete table.second;
    }
}
