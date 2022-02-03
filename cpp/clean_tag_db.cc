/** \file    Clean up the tags database
 *  \brief   Remove unreferenced tags and tag references from the database
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2018-2021, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <cstdlib>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input\n";
    std::exit(EXIT_FAILURE);
}


void ExtractAllRecordIDs(MARC::Reader * const marc_reader, std::unordered_set<std::string> * const all_record_ids) {
    while (const MARC::Record &record = marc_reader->read())
        all_record_ids->emplace(record.getControlNumber());
}

void GetUnreferencedPPNsFromDB(DbConnection * const db_connection, const std::unordered_set<std::string> &all_record_ids,
                               std::vector<std::string> * const unreferenced_ppns) {
    // Get the ppn from the resources table
    db_connection->queryOrDie("SELECT DISTINCT record_id FROM resource");
    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty())
        return;

    while (const auto &db_row = result_set.getNextRow()) {
        const std::string record_id(db_row["record_id"]);
        if (all_record_ids.find(record_id) == all_record_ids.cend())
            unreferenced_ppns->push_back(record_id);
    }
}

auto FormatSQLValue = [](const std::string term) { return "('" + term + "')"; };


void CreateTemporaryUnreferencedPPNTable(DbConnection * const db_connection, const std::vector<std::string> &unreferenced_ppns) {
    db_connection->queryOrDie("CREATE TEMPORARY TABLE unreferenced_ppns (`record_id` varchar(255))");
    const std::string INSERT_STATEMENT_START("INSERT IGNORE INTO unreferenced_ppns VALUES ");

    // Only transmit a limited number of values in a bunch
    const long MAX_ROW_COUNT(100);
    for (size_t row_count(0); row_count < unreferenced_ppns.size(); row_count += MAX_ROW_COUNT) {
        std::vector<std::string> values;
        std::vector<std::string> formatted_values;
        const auto copy_start(unreferenced_ppns.cbegin() + std::min(row_count, unreferenced_ppns.size()));
        std::copy_n(copy_start, std::min(MAX_ROW_COUNT, std::distance(copy_start, unreferenced_ppns.cend())), std::back_inserter(values));
        std::transform(values.begin(), values.end(), std::back_inserter(formatted_values), FormatSQLValue);
        db_connection->queryOrDie(INSERT_STATEMENT_START + StringUtil::Join(formatted_values, ","));
    }
}


void RemoveUnreferencedEntries(DbConnection * const db_connection) {
    // Delete the unreferenced IDs from the resource tags;
    const std::string GET_UNREFERENCED_IDS_STATEMENT("SELECT id FROM resource where record_id IN (SELECT * FROM unreferenced_ppns)");

    const std::string DELETE_UNREFERENCED_RESOURCE_TAGS_ENTRIES("DELETE FROM resource_tags WHERE resource_id IN ("
                                                                + GET_UNREFERENCED_IDS_STATEMENT + ")");

    db_connection->queryOrDie(DELETE_UNREFERENCED_RESOURCE_TAGS_ENTRIES);

    // Delete the unused tags
    const std::string DELETE_UNREFERENCED_TAG_ENTRIES("DELETE FROM tags WHERE id NOT IN (SELECT DISTINCT tag_id FROM resource_tags)");

    db_connection->queryOrDie(DELETE_UNREFERENCED_TAG_ENTRIES);

    // Delete the unused resources
    const std::string DELETE_UNREFERENCED_RESOURCES_ENTRIES(
        "DELETE FROM resource WHERE id NOT IN (SELECT resource_id FROM resource_tags) \
         AND id NOT IN (SELECT resource_id FROM user_resource)");

    db_connection->queryOrDie(DELETE_UNREFERENCED_RESOURCES_ENTRIES);
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 2)
        Usage();

    const std::string marc_input_filename(argv[1]);

    auto db_connection(DbConnection::VuFindMySQLFactory());
    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename));
    std::unordered_set<std::string> all_record_ids;
    ExtractAllRecordIDs(marc_reader.get(), &all_record_ids);
    std::vector<std::string> unreferenced_ppns;
    GetUnreferencedPPNsFromDB(&db_connection, all_record_ids, &unreferenced_ppns);
    CreateTemporaryUnreferencedPPNTable(&db_connection, unreferenced_ppns);
    RemoveUnreferencedEntries(&db_connection);
    LOG_INFO("Removed superfluous references for " + std::to_string(unreferenced_ppns.size()) + " PPN(s)");

    return EXIT_SUCCESS;
}
