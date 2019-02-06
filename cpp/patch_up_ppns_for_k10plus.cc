/** \file    patch_up_ppns_for_k10plus.cc
 *  \brief   Swaps out all persistent old PPN's with new PPN's.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2019, Library of the University of Tübingen

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

#include <iostream>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "ControlNumberGuesser.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"
#include "VuFind.h"


namespace {


void LoadMapping(MARC::Reader * const marc_reader, std::unordered_map<std::string, std::string> * const old_to_new_map) {
    while (const auto record = marc_reader->read()) {
        for (const auto &field : record.getTagRange("035")) {
            const auto subfield_a(field.getFirstSubfieldWithCode('a'));
            if (StringUtil::StartsWith(subfield_a, "(DE-576)"))
                old_to_new_map->emplace(subfield_a.substr(__builtin_strlen("(DE-576)")), record.getControlNumber());
        }
    }

    LOG_INFO("Found " + std::to_string(old_to_new_map->size()) + " mappings of old PPN's to new PPN's in \"" + marc_reader->getPath()
             + "\".\n");
}


void PatchTable(DbConnection * const db_connection, const std::string &table, const std::string &column,
                const std::unordered_map<std::string, std::string> &old_to_new_map)
{
    db_connection->queryOrDie("SELECT DISTINCT " + column + " FROM " + table);
    auto result_set(db_connection->getLastResultSet());
    unsigned replacement_count(0);
    while (const DbRow row = result_set.getNextRow()) {
        const auto old_and_new(old_to_new_map.find(row[column]));
        if (old_and_new != old_to_new_map.cend()) {
            db_connection->queryOrDie("UPDATE IGNORE " + table + " SET " + column + "='" + old_and_new->second
                                      + "' WHERE " + column + "='" + old_and_new->first + "'");
            ++replacement_count;
        }
    }

    LOG_INFO("Replaced " + std::to_string(replacement_count) + " PPN's in ixtheo.keyword_translations.");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc < 2)
        ::Usage("marc_input1 [marc_input2 .. marc_inputN]");

    std::unordered_map<std::string, std::string> old_to_new_map;
    for (int arg_no(1); arg_no < argc; ++arg_no) {
        const auto marc_reader(MARC::Reader::Factory(argv[arg_no]));
        LoadMapping(marc_reader.get(), &old_to_new_map);
    }

    ControlNumberGuesser control_number_guesser;
    control_number_guesser.swapControlNumbers(old_to_new_map);

    std::string mysql_url;
    VuFind::GetMysqlURL(&mysql_url);
    DbConnection db_connection(mysql_url);

    PatchTable(&db_connection, "vufind.resource", "record_id", old_to_new_map);
    PatchTable(&db_connection, "vufind.record", "record_id", old_to_new_map);
    if (VuFind::GetTueFindFlavour() == "ixtheo") {
        PatchTable(&db_connection, "ixtheo.keyword_translations", "ppn", old_to_new_map);
        PatchTable(&db_connection, "vufind.ixtheo_journal_subscriptions", "journal_control_number_or_bundle_name", old_to_new_map);
        PatchTable(&db_connection, "vufind.ixtheo_pda_subscriptions", "book_ppn", old_to_new_map);
        PatchTable(&db_connection, "vufind.relbib_ids", "record_id", old_to_new_map);
        PatchTable(&db_connection, "vufind.bibstudies_ids", "record_id", old_to_new_map);
    } else {
        PatchTable(&db_connection, "vufind.full_text_cache", "id", old_to_new_map);
        PatchTable(&db_connection, "vufind.full_text_cache_urls", "id", old_to_new_map);
    }

    return EXIT_SUCCESS;
}
