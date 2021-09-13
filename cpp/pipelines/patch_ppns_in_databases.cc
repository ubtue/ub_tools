/** \file    patch_ppns_in_databases.cc
 *  \brief   Swaps changed and deletes PPN's in various databases.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2019-2021, Library of the University of Tübingen

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
#include <vector>
#include <cstdlib>
#include <cstring>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "FileUtil.h"
#include "KeyValueDB.h"
#include "MapUtil.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"
#include "VuFind.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--store-only|--report-only] marc_input1 [marc_input2 .. marc_inputN] [-- deletion_list]\n"
            "If --store-only has been specified, no swapping will be performed and only the persistent map file will be overwritten.\n"
            "If deletion lists should be processed, they need to be specified after a double-hyphen to indicate the end of the MARC files.");
}


struct PPNsAndSigil {
    std::string old_ppn_, old_sigil_, new_ppn_;
public:
    PPNsAndSigil(const std::string &old_ppn, const std::string &old_sigil, const std::string &new_ppn)
        : old_ppn_(old_ppn), old_sigil_(old_sigil), new_ppn_(new_ppn) { }
    PPNsAndSigil() = default;
    PPNsAndSigil(const PPNsAndSigil &other) = default;
};


void LoadMappingByFieldSpec(const MARC::Record &record, const std::string &tag, const char subfield_code,
                            const std::unordered_multimap<std::string, std::string> &already_processed_ppns_and_sigils,
                            std::vector<PPNsAndSigil> * const old_ppns_sigils_and_new_ppns)
{
    static auto matcher(RegexMatcher::RegexMatcherFactoryOrDie("^\\((DE-627)\\)(.+)"));

    for (const auto &field : record.getTagRange(tag)) {
        const auto subfield(field.getFirstSubfieldWithCode(subfield_code));
        if (matcher->matched(subfield)) {
            const std::string old_sigil((*matcher)[1]);
            const std::string old_ppn((*matcher)[2]);
            if (old_ppn != record.getControlNumber() and not MapUtil::Contains(already_processed_ppns_and_sigils, old_ppn, old_sigil))
                old_ppns_sigils_and_new_ppns->emplace_back(old_ppn, old_sigil, record.getControlNumber());
        }
    }
}


void LoadMapping(MARC::Reader * const marc_reader,
                 const std::unordered_multimap<std::string, std::string> &already_processed_ppns_and_sigils,
                 std::vector<PPNsAndSigil> * const old_ppns_sigils_and_new_ppns)
{
    while (const auto record = marc_reader->read()) {
        LoadMappingByFieldSpec(record, "035", 'a', already_processed_ppns_and_sigils, old_ppns_sigils_and_new_ppns);
        LoadMappingByFieldSpec(record, "889", 'w', already_processed_ppns_and_sigils, old_ppns_sigils_and_new_ppns);
    }

    LOG_INFO("Found " + std::to_string(old_ppns_sigils_and_new_ppns->size()) + " new mappings of old PPN's to new PPN's in \""
             + marc_reader->getPath() + "\".\n");
}


void PatchTable(DbConnection * const db_connection, const std::string &table, const std::string &column,
                const std::vector<PPNsAndSigil> &old_ppns_sigils_and_new_ppns, const bool report_only)
{
    const unsigned MAX_BATCH_SIZE(100);

    if (not report_only)
        db_connection->queryOrDie("BEGIN");

    unsigned replacement_count(0), batch_size(0);
    for (const auto &old_ppn_sigil_and_new_ppn : old_ppns_sigils_and_new_ppns) {
        ++batch_size;

        const std::string where("WHERE " + column + "='" + old_ppn_sigil_and_new_ppn.old_ppn_ + "'");
        if (report_only)
            replacement_count += db_connection->countOrDie("SELECT count(*) AS replacement_count FROM " + table + " " + where, "replacement_count");
        else {
            db_connection->queryOrDie("UPDATE IGNORE " + table + " SET " + column + "='" + old_ppn_sigil_and_new_ppn.new_ppn_ + "' " + where);
            replacement_count += db_connection->getNoOfAffectedRows();
            if (batch_size >= MAX_BATCH_SIZE) {
                db_connection->queryOrDie("COMMIT");
                db_connection->queryOrDie("BEGIN");
            }
        }
    }

    if (report_only)
        LOG_INFO("Would replace " + std::to_string(replacement_count) + " rows in " + table + ".");
    else {
        db_connection->queryOrDie("COMMIT");
        LOG_INFO("Replaced " + std::to_string(replacement_count) + " rows in " + table + ".");
    }

}


void DeleteFromTable(DbConnection * const db_connection, const std::string &table, const std::string &column,
                     const std::unordered_set<std::string> &deletion_ppns, const bool report_only)
{
    const unsigned MAX_BATCH_SIZE(100);

    if (not report_only)
        db_connection->queryOrDie("BEGIN");

    unsigned deletion_count(0), batch_size(0);
    for (const auto &deletion_ppn : deletion_ppns) {
        ++batch_size;

        const std::string where("WHERE " + column + "='" + deletion_ppn + "'");
        if (report_only)
            deletion_count += db_connection->countOrDie("SELECT count(*) AS deletion_count FROM '" + table + "' " + where, "deletion_count");
        else {
            db_connection->queryOrDie("DELETE FROM '" + table + "' " + where);
            deletion_count += db_connection->getNoOfAffectedRows();
            if (batch_size >= MAX_BATCH_SIZE) {
                db_connection->queryOrDie("COMMIT");
                db_connection->queryOrDie("BEGIN");
            }
        }
    }

    if (report_only)
        LOG_INFO("Would delete " + std::to_string(deletion_count) + " rows from " + table + ".");
    else {
        db_connection->queryOrDie("COMMIT");
        LOG_INFO("Deleted " + std::to_string(deletion_count) + " rows from " + table + ".");
    }
}


void PatchNotifiedDB(const std::string &user_type, const std::vector<PPNsAndSigil> &old_ppns_sigils_and_new_ppns, const bool report_only) {
    const std::string DB_FILENAME(UBTools::GetTuelibPath() + user_type + "_notified.db");
    if (not FileUtil::Exists(DB_FILENAME)) {
        LOG_INFO("\"" + DB_FILENAME + "\" not found!");
        return;
    }

    KeyValueDB db(DB_FILENAME);

    unsigned updated_count(0);
    for (const auto &ppns_and_sigil : old_ppns_sigils_and_new_ppns) {
        if (db.keyIsPresent(ppns_and_sigil.old_ppn_)) {
            const std::string value(db.getValue(ppns_and_sigil.old_ppn_));

            if (not report_only) {
                db.remove(ppns_and_sigil.old_ppn_);
                db.addOrReplace(ppns_and_sigil.new_ppn_, value);
            }
            ++updated_count;
        }
    }

    if (report_only)
        LOG_INFO("Would update " + std::to_string(updated_count) + " entries in \"" + DB_FILENAME + "\".");
    else
        LOG_INFO("Updated " + std::to_string(updated_count) + " entries in \"" + DB_FILENAME + "\".");
}


void DeleteFromNotifiedDB(const std::string &user_type, const std::unordered_set<std::string> &deletion_ppns, const bool report_only) {
    const std::string DB_FILENAME(UBTools::GetTuelibPath() + user_type + "_notified.db");
    if (not FileUtil::Exists(DB_FILENAME)) {
        LOG_INFO("\"" + DB_FILENAME + "\" not found!");
        return;
    }

    KeyValueDB db(DB_FILENAME);

    unsigned deletion_count(0);
    for (const auto &deletion_ppn : deletion_ppns) {
        if (db.keyIsPresent(deletion_ppn)) {
            if (not report_only)
                db.remove(deletion_ppn);
            ++deletion_count;
        }
    }

    if (report_only)
        LOG_INFO("Would delete " + std::to_string(deletion_count) + " entries from \"" + DB_FILENAME + "\".");
    else
        LOG_INFO("Deleted " + std::to_string(deletion_count) + " entries from \"" + DB_FILENAME + "\".");
}


void CheckMySQLPermissions(DbConnection * const db_connection) {
    if (not db_connection->mySQLUserHasPrivileges("vufind", DbConnection::MYSQL_ALL_PRIVILEGES))
        LOG_ERROR("'" + db_connection->mySQLGetUser() + "'@'" + db_connection->mySQLGetHost()
                  + "' needs all permissions on the vufind database!");
    if (VuFind::GetTueFindFlavour() == "ixtheo") {
        if (not db_connection->mySQLUserHasPrivileges("ixtheo", DbConnection::MYSQL_ALL_PRIVILEGES))
            LOG_ERROR("'" + db_connection->mySQLGetUser() + "'@' " + db_connection->mySQLGetHost()
                      + "' needs all permissions on the ixtheo database!");
    }
}


void AddPPNsAndSigilsToMultiMap(const std::vector<PPNsAndSigil> &old_ppns_sigils_and_new_ppns,
                                std::unordered_multimap<std::string, std::string> * const already_processed_ppns_and_sigils)
{
    for (const auto &old_ppn_sigil_and_new_ppn : old_ppns_sigils_and_new_ppns)
        already_processed_ppns_and_sigils->emplace(std::make_pair(old_ppn_sigil_and_new_ppn.old_ppn_, old_ppn_sigil_and_new_ppn.old_sigil_));
}


template<class SetOrMap, typename ProcessNotifieldDBFunc, typename ProcessTableFunc>
void ProcessAllDatabases(DbConnection * const db_connection, const SetOrMap &set_or_map, const ProcessNotifieldDBFunc notified_db_func,
                         const ProcessTableFunc table_func, const bool report_only)
{
    notified_db_func("ixtheo", set_or_map, report_only);
    notified_db_func("relbib", set_or_map, report_only);

    table_func(db_connection, "vufind.resource", "record_id", set_or_map, report_only);
    table_func(db_connection, "vufind.record", "record_id", set_or_map, report_only);
    table_func(db_connection, "vufind.change_tracker", "id", set_or_map, report_only);
    if (VuFind::GetTueFindFlavour() == "ixtheo") {
        table_func(db_connection, "ixtheo.keyword_translations", "ppn", set_or_map, report_only);
        table_func(db_connection, "vufind.ixtheo_journal_subscriptions", "journal_control_number_or_bundle_name",
                   set_or_map, report_only);
        table_func(db_connection, "vufind.ixtheo_pda_subscriptions", "book_ppn", set_or_map, report_only);
    }
}


} // unnamed namespace


static const std::string ALREADY_SWAPPED_PPNS_MAP_FILE(UBTools::GetTuelibPath() + "k10+_ppn_map.map");


int Main(int argc, char **argv) {
    if (argc < 2)
        Usage();

    bool store_only(false), report_only(false);
    if (std::strcmp(argv[1], "--store-only") == 0) {
        store_only = true;
        --argc, ++argv;
        if (argc < 2)
            Usage();
    } else if (std::strcmp(argv[1], "--report-only") == 0) {
        report_only = true;
        --argc, ++argv;
        if (argc < 2)
            Usage();
    }

    DbConnection db_connection(DbConnection::UBToolsFactory());

    CheckMySQLPermissions(&db_connection);

    std::unordered_multimap<std::string, std::string> already_processed_ppns_and_sigils;
    if (not FileUtil::Exists(ALREADY_SWAPPED_PPNS_MAP_FILE))
        FileUtil::WriteStringOrDie(ALREADY_SWAPPED_PPNS_MAP_FILE, "");
    if (not store_only)
        MapUtil::DeserialiseMap(ALREADY_SWAPPED_PPNS_MAP_FILE, &already_processed_ppns_and_sigils);

    std::vector<PPNsAndSigil> old_ppns_sigils_and_new_ppns;
    int arg_no(1);
    for (/* Intentionally empty! */; arg_no < argc; ++arg_no) {
        if (__builtin_strcmp(argv[arg_no], "--") == 0) {
            ++arg_no;
            break;
        }
        const auto marc_reader(MARC::Reader::Factory(argv[arg_no]));
        LoadMapping(marc_reader.get(), already_processed_ppns_and_sigils, &old_ppns_sigils_and_new_ppns);
    }

    std::unordered_set <std::string> deletion_ppns;
    if (arg_no < argc) {
        for (auto line : FileUtil::ReadLines((argv[arg_no])))
            deletion_ppns.emplace(line);
        ++arg_no;
    }
    if (arg_no != argc)
        Usage();

    if (old_ppns_sigils_and_new_ppns.empty() and deletion_ppns.empty()) {
        LOG_INFO("nothing to do!");
        return EXIT_SUCCESS;
    }

    if (report_only) {
        if (not deletion_ppns.empty()) {
            LOG_INFO("Deletions:");
            for (const auto &ppn : deletion_ppns)
                LOG_INFO(ppn);
        }

        if (not old_ppns_sigils_and_new_ppns.empty()) {
            LOG_INFO("Old PPN to New PPN Mapping:");
            for (const auto &old_ppn_sigil_and_new_ppn : old_ppns_sigils_and_new_ppns)
                LOG_INFO(old_ppn_sigil_and_new_ppn.old_ppn_ + " -> " + old_ppn_sigil_and_new_ppn.new_ppn_);
        }
    }

    if (old_ppns_sigils_and_new_ppns.empty())
        goto clean_up_deleted_ppns;

    if (store_only) {
        AddPPNsAndSigilsToMultiMap(old_ppns_sigils_and_new_ppns, &already_processed_ppns_and_sigils);
        MapUtil::SerialiseMap(ALREADY_SWAPPED_PPNS_MAP_FILE, already_processed_ppns_and_sigils);
        if (not deletion_ppns.empty())
            goto clean_up_deleted_ppns;
        return EXIT_SUCCESS;
    }

    ProcessAllDatabases(&db_connection, old_ppns_sigils_and_new_ppns, PatchNotifiedDB, PatchTable, report_only);
    AddPPNsAndSigilsToMultiMap(old_ppns_sigils_and_new_ppns, &already_processed_ppns_and_sigils);

    if (not report_only)
        MapUtil::SerialiseMap(ALREADY_SWAPPED_PPNS_MAP_FILE, already_processed_ppns_and_sigils);

clean_up_deleted_ppns:
    ProcessAllDatabases(&db_connection, deletion_ppns, DeleteFromNotifiedDB, DeleteFromTable, report_only);

    return EXIT_SUCCESS;
}
