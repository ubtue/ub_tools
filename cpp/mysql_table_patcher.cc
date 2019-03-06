/** \brief Utility for updating SQL schemata etc.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "DbConnection.h"
#include "StringUtil.h"
#include "util.h"


namespace {


void SplitIntoDatabaseTableAndVersion(const std::string &update_filename, std::string * const database, std::string * const table,
                                      unsigned * const version)
{
    std::vector<std::string> parts;
    if (unlikely(StringUtil::Split(update_filename, '.', &parts) != 3))
        LOG_ERROR("failed to split \"" + update_filename + "\" into \"database.table.version\"!");
    *database = parts[0];
    *table    = parts[1];
    *version  = StringUtil::ToUnsigned(parts[2]);
}


__attribute__((__const__)) inline std::string GetFirstTableName(const std::string &compound_table_name) {
    const auto first_plus_pos(compound_table_name.find('+'));
    return (first_plus_pos == std::string::npos) ? compound_table_name : compound_table_name.substr(0, first_plus_pos);
}


// The filenames being compared are assumed to have the "structure database.table.version"
bool FileNameCompare(const std::string &filename1, const std::string &filename2) {
    std::string database1, table1;
    unsigned version1;
    SplitIntoDatabaseTableAndVersion(filename1, &database1, &table1, &version1);

    std::string database2, table2;
    unsigned version2;
    SplitIntoDatabaseTableAndVersion(filename2, &database2, &table2, &version2);

    // Compare database names:
    if (database1 < database2)
        return true;
    if (database1 > database2)
        return false;

    // Compare table names:
    if (GetFirstTableName(table1) < GetFirstTableName(table2))
        return true;
    if (GetFirstTableName(table1) > GetFirstTableName(table2))
        return false;

    return version1 < version2;
}


void LoadAndSortUpdateFilenames(const std::string &directory_path, std::vector<std::string> * const update_filenames) {
    FileUtil::Directory directory(directory_path, "[^.]+\\.[^.]+\\.\\d+");
    for (const auto &entry : directory)
        update_filenames->emplace_back(entry.getName());

    std::sort(update_filenames->begin(), update_filenames->end(), FileNameCompare);
}


std::vector<std::string> GetAllTableNames(const std::string &compound_table_name) {
    std::vector<std::string> all_table_names;
    StringUtil::Split(compound_table_name, '+', &all_table_names);
    return all_table_names;
}


void ApplyUpdate(DbConnection * const db_connection, const std::string &update_directory_path, const std::string &update_filename) {
    std::string database, tables;
    unsigned update_version;
    SplitIntoDatabaseTableAndVersion(update_filename, &database, &tables, &update_version);

    db_connection->queryOrDie("START TRANSACTION");


    bool can_update(true);
    for (const auto &table : GetAllTableNames(tables)) {
        unsigned current_version(0);
        db_connection->queryOrDie("SELECT version FROM ub_tools.table_versions WHERE database_name='"
                                  + db_connection->escapeString(database) + "' AND table_name='" + db_connection->escapeString(table) + "'");
        DbResultSet result_set(db_connection->getLastResultSet());
        if (result_set.empty()) {
            db_connection->queryOrDie("INSERT INTO ub_tools.table_versions (database_name,table_name,version) VALUES ('"
                                      + db_connection->escapeString(database) + "','" + db_connection->escapeString(table) + "',0)");
            LOG_INFO("Created a new entry for " + database + "." + table + " in ub_tools.table_versions.");
        } else
            current_version = StringUtil::ToUnsigned(result_set.getNextRow()["version"]);
        if (update_version <= current_version) {
            can_update = false;
            continue;
        }
        if (unlikely(not can_update))
            LOG_ERROR("inconsistent updates for tables \"" + tables + "\"!");

        db_connection->queryOrDie("UPDATE ub_tools.table_versions SET version=" + std::to_string(update_version)
                                  + " WHERE database_name='" + db_connection->escapeString(database) + "' AND table_name='"
                                  + db_connection->escapeString(table) + "'");

        if (unlikely(update_version != current_version + 1))
            LOG_ERROR("update version is " + std::to_string(update_version) + ", current version is " + std::to_string(current_version)
                      + " for table \"" + database + "." + table + "\"!");

        LOG_INFO("applying update \"" + database + "." + table + "." + std::to_string(update_version) + "\".");
    }

    if (can_update) {
        db_connection->queryFileOrDie(update_directory_path + "/" + update_filename);
        db_connection->queryOrDie("COMMIT");
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        ::Usage("[--verbose] update_directory_path");

    std::vector<std::string> update_filenames;
    const std::string update_directory_path(argv[1]);
    LoadAndSortUpdateFilenames(update_directory_path, &update_filenames);

    DbConnection db_connection;
    if (not db_connection.tableExists("ub_tools", "table_versions")) {
        db_connection.queryOrDie("CREATE TABLE ub_tools.table_versions (version INT UNSIGNED NOT NULL, database_name VARCHAR(64) NOT NULL, "
                                 "table_name VARCHAR(64) NOT NULL, UNIQUE(database_name,table_name)) "
                                 "CHARACTER SET utf8mb4 COLLATE utf8mb4_bin");
        LOG_INFO("Created the ub_tools.table_versions table.");
    }

    for (const auto &update_filename : update_filenames)
        ApplyUpdate(&db_connection, update_directory_path, update_filename);

    return EXIT_SUCCESS;
}
