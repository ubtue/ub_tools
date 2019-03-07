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


void SplitIntoDatabaseTablesAndVersions(const std::string &update_filename, std::string * const database,
                                        std::vector<std::pair<std::string, unsigned>> * const tables_and_versions)
{
    tables_and_versions->clear();

    std::vector<std::string> parts;
    if (unlikely(StringUtil::Split(update_filename, '.', &parts, /* suppress_empty_components = */false) != 2))
        LOG_ERROR("failed to split \"" + update_filename + "\" into \"database.table;version[+table;version]*\"! (# of parts = "
                  + std::to_string(parts.size()) + ")");
    *database = parts[0];

    std::vector<std::string> parts2;
    StringUtil::Split(parts[1], '+', &parts2, /* suppress_empty_components = */false);

    for (const auto &part : parts2) {
        std::vector<std::string> parts3;
        if (unlikely(StringUtil::Split(part, ';', &parts3, /* suppress_empty_components = */false) != 2
                     or parts3[0].empty() or parts3[1].empty()))
            LOG_ERROR("failed to split \"" + part + "\" into table and version!");
        tables_and_versions->emplace_back(std::make_pair(parts3[0], StringUtil::ToUnsigned(parts3[1])));
    }
}


// The filenames being compared are assumed to have the "structure database.table;version[+table;version]*"
bool FileNameCompare(const std::string &filename1, const std::string &filename2) {
    std::string database1;
    std::vector<std::pair<std::string, unsigned>> tables_and_versions1;
    SplitIntoDatabaseTablesAndVersions(filename1, &database1, &tables_and_versions1);

    std::string database2;
    std::vector<std::pair<std::string, unsigned>> tables_and_versions2;
    SplitIntoDatabaseTablesAndVersions(filename2, &database2, &tables_and_versions2);

    // Compare database names:
    if (database1 < database2)
        return true;
    if (database1 > database2)
        return false;

    // Compare table names and versions.  If we have more than one common table name, the ordering has to be the same for all
    // shared table names or we have an unresolvable situation!
    unsigned one_before_two(0), two_before_one(0); // Ordering between shared table names.
    for (const auto &table_and_version1 : tables_and_versions1) {
        const auto table_and_version2(std::find_if(tables_and_versions2.cbegin(), tables_and_versions2.cend(),
                                                   [&table_and_version1](const std::pair<std::string, unsigned> &table_and_version)
                                                   { return table_and_version.first == table_and_version1.first; }));
        if (table_and_version2 != tables_and_versions2.cend()) {
            if (unlikely(table_and_version1.second == table_and_version2->second))
                LOG_ERROR("impossible filename comparsion \"" + filename1 + "\" with \"" + filename2 + "\"! (1)");
            else if (table_and_version1.second < table_and_version2->second)
                ++one_before_two;
            else
                ++two_before_one;
        }
    }
    if (unlikely(one_before_two > 0 and two_before_one > 0))
        LOG_ERROR("impossible filename comparsion \"" + filename1 + "\" with \"" + filename2 + "\"! (2)");
    else if (one_before_two > 0)
        return true;
    else if (two_before_one > 0)
        return false;

    // ...fall back on alphanumeric comparison:
    return tables_and_versions1[0].first < tables_and_versions2[0].first;
}


void LoadAndSortUpdateFilenames(const bool test, const std::string &directory_path, std::vector<std::string> * const update_filenames) {
    FileUtil::Directory directory(directory_path, "[^.]+\\.[^.;]+;\\d+(?:\\+[^.;]+;\\d+)*");
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


void ApplyUpdate(DbConnection * const db_connection, const std::string &update_directory_path, const std::string &update_filename) {
    std::string database;
    std::vector<std::pair<std::string, unsigned>> tables_and_versions;
    SplitIntoDatabaseTablesAndVersions(update_filename, &database, &tables_and_versions);

    db_connection->queryOrDie("START TRANSACTION");

    bool can_update(true);
    for (const auto &table_and_version : tables_and_versions) {
        unsigned current_version(0);
        db_connection->queryOrDie("SELECT version FROM ub_tools.table_versions WHERE database_name='"
                                  + db_connection->escapeString(database) + "' AND table_name='"
                                  + db_connection->escapeString(table_and_version.first) + "'");
        DbResultSet result_set(db_connection->getLastResultSet());
        if (result_set.empty()) {
            db_connection->queryOrDie("INSERT INTO ub_tools.table_versions (database_name,table_name,version) VALUES ('"
                                      + db_connection->escapeString(database) + "','"
                                      + db_connection->escapeString(table_and_version.first) + "',0)");
            LOG_INFO("Created a new entry for " + database + "." + table_and_version.first + " in ub_tools.table_versions.");
        } else
            current_version = StringUtil::ToUnsigned(result_set.getNextRow()["version"]);
        if (table_and_version.second <= current_version) {
            can_update = false;
            continue;
        }
        if (unlikely(not can_update))
            LOG_ERROR("inconsistent update \"" + update_filename + "\"!");

        db_connection->queryOrDie("UPDATE ub_tools.table_versions SET version=" + std::to_string(table_and_version.second)
                                  + " WHERE database_name='" + db_connection->escapeString(database) + "' AND table_name='"
                                  + db_connection->escapeString(table_and_version.first) + "'");

        if (unlikely(table_and_version.second != current_version + 1))
            LOG_ERROR("update version is " + std::to_string(table_and_version.second) + ", current version is "
                      + std::to_string(current_version) + " for table \"" + database + "." + table_and_version.first + "\"!");

        LOG_INFO("applying update \"" + database + "." + table_and_version.first + "." + std::to_string(table_and_version.second) + "\".");
    }

    if (can_update) {
        db_connection->queryFileOrDie(update_directory_path + "/" + update_filename);
        db_connection->queryOrDie("COMMIT");
    }
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
