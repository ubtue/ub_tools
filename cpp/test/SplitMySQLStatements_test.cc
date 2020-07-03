// Test harness for DbConnection::SplitMySQLStatements.
#include <iostream>
#include "DbConnection.h"
#include "FileUtil.h"
#include "util.h"


int Main(int argc, char *argv[]) {
    if (argc != 2)
        ::Usage("path_to_file_containing_sql_statements");

    const std::string path(argv[1]);
    std::string file_contents;
    if (not FileUtil::ReadString(path, &file_contents))
        LOG_ERROR("can't read \"" + path + "\"!");

    const auto statements(DbConnection::SplitMySQLStatements(file_contents));
    for (const auto &statement : statements)
        std::cout << ">>" << statement << "<<\n";

    return EXIT_SUCCESS;
}
