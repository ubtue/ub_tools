/** \brief Utility for updating SQL schemata etc.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019-2021 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <iostream>
#include <utility>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "DbConnection.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--test] update_directory_path");
}


void SplitIntoDatabaseAndVersion(const std::string &update_filename, std::string * const database, unsigned * const version) {
    const auto first_dot_pos(update_filename.find('.'));
    if (first_dot_pos == std::string::npos or first_dot_pos == 0 or first_dot_pos == update_filename.length() - 1)
        LOG_ERROR("invalid update filename \"" + update_filename + "\"!");

    if (not StringUtil::ToUnsigned(update_filename.substr(first_dot_pos + 1), version))
        LOG_ERROR("bad or missing version in update filename \"" + update_filename + "\"!");

    *database = update_filename.substr(0, first_dot_pos);
}


// The filenames being compared are assumed to have the "structure database.version]*"
bool FileNameCompare(const std::string &filename1, const std::string &filename2) {
    std::string database1;
    unsigned version1;
    SplitIntoDatabaseAndVersion(filename1, &database1, &version1);

    std::string database2;
    unsigned version2;
    SplitIntoDatabaseAndVersion(filename2, &database2, &version2);

    // Compare database names:
    if (database1 < database2)
        return true;
    if (database1 > database2)
        return false;

    return version1 < version2;
}


void LoadAndSortUpdateFilenames(const bool test, const std::string &directory_path,
                                std::vector<std::string> * const update_filenames)
{
    FileUtil::Directory directory(directory_path, "[^.]+\\.\\d+");
    for (const auto &entry : directory)
        update_filenames->emplace_back(entry.getName());

    std::sort(update_filenames->begin(), update_filenames->end(), FileNameCompare);

    if (test) {
        std::cerr << "Sorted filenames:\n";
        for (const auto &filename : *update_filenames)
            std::cerr << filename << '\n';
        std::exit(0);
    }
}


void ApplyUpdate(DbConnection * const db_connection, const std::string &update_directory_path,
                 const std::string &update_filename, std::string * const current_schema, const std::string &last_schema)
{
    std::string database;
    unsigned update_version;
    SplitIntoDatabaseAndVersion(update_filename, &database, &update_version);
    *current_schema = database;

    if (not db_connection->mySQLDatabaseExists(database)) {
        LOG_INFO("database \"" + database + "\" does not exist, skipping file " + update_filename);
        return;
    }

    if (last_schema.empty() or database != last_schema) {
        LOG_INFO("switching to database: " + database);
        db_connection->queryOrDie("USE " + database);
    }

    DbTransaction transaction(db_connection); // No new scope required as the transaction is supposed to last until the end
                                              // of this function anyway!

    unsigned current_version(0);
    db_connection->queryOrDie("SELECT version FROM ub_tools.database_versions WHERE database_name='"
                              + db_connection->escapeString(database) + "'");
    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty()) {
        db_connection->queryOrDie("INSERT INTO ub_tools.database_versions (database_name,version) VALUES ('"
                                  + db_connection->escapeString(database) + "',0)");
            LOG_INFO("Created a new entry for database \"" + database + " in ub_tools.database_versions.");
    } else
        current_version = StringUtil::ToUnsigned(result_set.getNextRow()["version"]);
    if (update_version <= current_version)
        return;

    // Sanity check:
    if (unlikely(update_version != current_version + 1))
        LOG_ERROR("update version is " + std::to_string(update_version) + ", current version is "
                  + std::to_string(current_version) + " for database \"" + database + "\"!");

    LOG_INFO("applying update " + std::to_string(update_version) + " to database \"" + database + "\".");
    db_connection->queryFileOrDie(update_directory_path + "/" + update_filename);
    db_connection->queryOrDie("UPDATE ub_tools.database_versions SET version=" + std::to_string(update_version)
                              + " WHERE database_name='" + db_connection->escapeString(database) + "'");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2 and argc != 3)
        Usage();

    bool test(false);
    if (argc == 3) {
        if (std::strcmp(argv[1], "--test") != 0)
            Usage();
        test = true;
        --argc, ++argv;
    }

    std::vector<std::string> update_filenames;
    const std::string update_directory_path(argv[1]);
    LoadAndSortUpdateFilenames(test, update_directory_path, &update_filenames);

    DbConnection db_connection(DbConnection::UBToolsFactory());
    const std::string system_table_name("database_versions");
    if (not db_connection.tableExists("ub_tools", system_table_name)) {
        db_connection.queryOrDie("CREATE TABLE ub_tools." + system_table_name + " (version INT UNSIGNED NOT NULL,"
                                 "database_name VARCHAR(64) NOT NULL,UNIQUE (database_name)) "
                                 "CHARACTER SET utf8mb4 COLLATE utf8mb4_bin");
        LOG_INFO("Created the ub_tools." + system_table_name + " table.");
    }

    std::string last_schema;
    std::string current_schema;
    for (const auto &update_filename : update_filenames) {
        ApplyUpdate(&db_connection, update_directory_path, update_filename, &current_schema, last_schema);
        last_schema = current_schema;
    }

    return EXIT_SUCCESS;
}
