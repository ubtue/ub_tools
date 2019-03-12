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
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <cstdlib>
#include <cstring>
#include <kchashdb.h>
#include "Compiler.h"
#include "ControlNumberGuesser.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"
#include "VuFind.h"


namespace {


const std::string OLD_PPN_LIST_FILE(UBTools::GetTuelibPath() + "alread_replaced_old_ppns.blob");


constexpr size_t OLD_PPN_LENGTH(9);


void LoadAlreadyProcessedPPNs(std::unordered_set<std::string> * const alread_processed_ppns) {
    if (not FileUtil::Exists(OLD_PPN_LIST_FILE))
        return;

    std::string blob;
    FileUtil::ReadStringOrDie(OLD_PPN_LIST_FILE, &blob);
    if (unlikely((blob.length() % OLD_PPN_LENGTH) != 0))
        LOG_ERROR("fractional PPN's are not possible!");

    const size_t count(blob.length() / OLD_PPN_LENGTH);
    alread_processed_ppns->reserve(count);

    for (size_t i(0); i < count; ++i)
        alread_processed_ppns->emplace(blob.substr(i * OLD_PPN_LENGTH, OLD_PPN_LENGTH));
}


void LoadMapping(MARC::Reader * const marc_reader, const std::unordered_set<std::string> &alread_processed_ppns,
                 std::unordered_map<std::string, std::string> * const old_to_new_map)
{
    while (const auto record = marc_reader->read()) {
        for (const auto &field : record.getTagRange("035")) {
            const auto subfield_a(field.getFirstSubfieldWithCode('a'));
            if (StringUtil::StartsWith(subfield_a, "(DE-576)")) {
                const auto old_ppn(subfield_a.substr(__builtin_strlen("(DE-576)")));
                if (alread_processed_ppns.find(old_ppn) == alread_processed_ppns.cend())
                    old_to_new_map->emplace(old_ppn, record.getControlNumber());
            }
        }
    }

    LOG_INFO("Found " + std::to_string(old_to_new_map->size()) + " new mappings of old PPN's to new PPN's in \"" + marc_reader->getPath()
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


void StoreNewAlreadyProcessedPPNs(const std::unordered_set<std::string> &alread_processed_ppns,
                                  const std::unordered_map<std::string, std::string> &old_to_new_map)
{
    std::string blob;
    blob.reserve(alread_processed_ppns.size() * OLD_PPN_LENGTH + old_to_new_map.size() * OLD_PPN_LENGTH);

    for (const auto &alread_processed_ppn : alread_processed_ppns)
        blob += alread_processed_ppn;
    for (const auto &old_and_new_ppns : old_to_new_map)
        blob += old_and_new_ppns.first;

    FileUtil::WriteStringOrDie(OLD_PPN_LIST_FILE, blob);
}


void PatchNotifiedDB(const std::string &user_type, const std::unordered_map<std::string, std::string> &old_to_new_map) {
    const std::string DB_FILENAME(UBTools::GetTuelibPath() + user_type + "_notified.db");
    std::unique_ptr<kyotocabinet::HashDB> db(new kyotocabinet::HashDB());
    if (not (db->open(DB_FILENAME, kyotocabinet::HashDB::OWRITER | kyotocabinet::HashDB::OREADER))) {
        LOG_INFO("\"" + DB_FILENAME + "\" not found!");
        return;
    }

    unsigned updated_count(0);
    for (const auto &old_and_new : old_to_new_map) {
        std::string value;
        if (db->get(old_and_new.first, &value)) {
            if (unlikely(not db->remove(old_and_new.first)))
                LOG_ERROR("failed to remove key \"" + old_and_new.first + "\" from \"" + DB_FILENAME + "\"!");
            if (unlikely(not db->add(old_and_new.second, value)))
                LOG_ERROR("failed to add key \"" + old_and_new.second + "\" from \"" + DB_FILENAME + "\"!");
            ++updated_count;
        }
    }

    LOG_INFO("Updated " + std::to_string(updated_count) + " entries in \"" + DB_FILENAME + "\".");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc < 2)
        ::Usage("marc_input1 [marc_input2 .. marc_inputN]");

    std::unordered_set<std::string> alread_processed_ppns;
    LoadAlreadyProcessedPPNs(&alread_processed_ppns);

    std::unordered_map<std::string, std::string> old_to_new_map;
    for (int arg_no(1); arg_no < argc; ++arg_no) {
        const auto marc_reader(MARC::Reader::Factory(argv[arg_no]));
        LoadMapping(marc_reader.get(), alread_processed_ppns, &old_to_new_map);
    }
    if (old_to_new_map.empty()) {
        LOG_INFO("nothing to do!");
        return EXIT_SUCCESS;
    }

    PatchNotifiedDB("ixtheo", old_to_new_map);
    PatchNotifiedDB("relbib", old_to_new_map);

    ControlNumberGuesser control_number_guesser;
    control_number_guesser.swapControlNumbers(old_to_new_map);
    std::shared_ptr<DbConnection> db_connection(VuFind::GetDbConnection());

    PatchTable(db_connection.get(), "vufind.resource", "record_id", old_to_new_map);
    PatchTable(db_connection.get(), "vufind.record", "record_id", old_to_new_map);
    PatchTable(db_connection.get(), "vufind.change_tracker", "id", old_to_new_map);
    if (VuFind::GetTueFindFlavour() == "ixtheo") {
        PatchTable(db_connection.get(), "ixtheo.keyword_translations", "ppn", old_to_new_map);
        PatchTable(db_connection.get(), "vufind.ixtheo_journal_subscriptions", "journal_control_number_or_bundle_name", old_to_new_map);
        PatchTable(db_connection.get(), "vufind.ixtheo_pda_subscriptions", "book_ppn", old_to_new_map);
        PatchTable(db_connection.get(), "vufind.relbib_ids", "record_id", old_to_new_map);
        PatchTable(db_connection.get(), "vufind.bibstudies_ids", "record_id", old_to_new_map);
    } else {
        PatchTable(db_connection.get(), "vufind.full_text_cache", "id", old_to_new_map);
        PatchTable(db_connection.get(), "vufind.full_text_cache_urls", "id", old_to_new_map);
    }

    StoreNewAlreadyProcessedPPNs(alread_processed_ppns, old_to_new_map);

    return EXIT_SUCCESS;
}
