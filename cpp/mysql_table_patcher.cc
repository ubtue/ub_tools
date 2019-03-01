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
    if (table1 < table2)
        return true;
    if (table1 > table2)
        return false;

    return version1 < version2;
}


void LoadAndSortUpdateFilenames(const std::string &directory_path, std::vector<std::string> * const update_filenames) {
    FileUtil::Directory directory(directory_path, "[^.]+\\.[^.]+\\.\\d+");
    for (const auto &entry : directory)
        update_filenames->emplace_back(entry.getName());

    std::sort(update_filenames->begin(), update_filenames->end(), FileNameCompare);
}


void ApplyUpdate(DbConnection * const /*db_connection*/, const std::string &database, const std::string &table, const unsigned version) {
    LOG_INFO("applying update \"" + database + "." + table + "." + std::to_string(version) + "\".");
}


void ApplyUpdates(DbConnection * const db_connection, const std::vector<std::string> &update_filenames) {
    std::string previous_database, previous_table;
    unsigned previous_version;

    for (const auto &update_filename : update_filenames) {
        std::string current_database, current_table;
        unsigned current_version;
        SplitIntoDatabaseTableAndVersion(update_filename, &current_database, &current_table, &current_version);
        if (not previous_database.empty() and (previous_database != current_database or previous_table != current_table))
            ApplyUpdate(db_connection, previous_database, previous_table, previous_version);

        previous_database = current_database;
        previous_table    = current_table;
        previous_version  = current_version;
    }

    if (not previous_database.empty())
        ApplyUpdate(db_connection, previous_database, previous_table, previous_version);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        ::Usage("[--verbose] update_directory_path");

    std::vector<std::string> update_filenames;
    LoadAndSortUpdateFilenames(argv[1], &update_filenames);

    DbConnection db_connection;
    if (not db_connection.tableExists("ub_tools", "table_versions"))
        db_connection.queryOrDie("CREATE TABLE ub_tools.table_versions (version INT UNSIGNED NOT NULL, table_name VARCHAR(64) NOT NULL, "
                                 "UNIQUE(table_name)) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin");

    ApplyUpdates(&db_connection, update_filenames);

    return EXIT_SUCCESS;
}
