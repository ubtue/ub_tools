// Test harness for some of the Postgres-related functionality in our Db* classes.
#include <iostream>
#include <memory>
#include <cstdlib>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "util.h"


int Main(int argc, char *argv[]) {
    if (argc != 5 and argc != 6)
        ::Usage("database_name user_name password query [hostname]");

    const std::string database_name(argv[1]);
    const std::string user_name(argv[2]);
    const std::string password(argv[3]);
    const std::string query(argv[4]);

    std::string hostname(argc == 6 ? argv[5] : "localhost");

    std::string error_message;
    std::unique_ptr<DbConnection> db_connection(
        DbConnection::PostgresFactory(&error_message, database_name, user_name, password, hostname));
    if (db_connection == nullptr)
        LOG_ERROR("failed to create a Postgres DbConnection: " + error_message);

    db_connection->queryOrDie(query);
    auto result_set(db_connection->getLastResultSet());
    std::cout << "The result size is " << result_set.size() << ".\n";
    const size_t column_count(result_set.getColumnCount());
    std::cout << "The number of columns in the result set is " << column_count << ".\n";

    const auto &column_names_and_indices(result_set.getColumnNamesAndIndices());
    std::cout << "Column names and indices are:\n";
    for (const auto &[column_name, column_index] : column_names_and_indices)
        std::cout << '\t' << column_name << " -> " << column_index << '\n';


    DbRow db_row;
    while (db_row = result_set.getNextRow()) {
        for (size_t column_no(0); column_no < column_count; ++column_no) {
            if (column_no > 0)
                std::cout << ", ";
            std::cout << db_row[column_no];
        }
        std::cout << '\n';
    }

    return EXIT_SUCCESS;
}
