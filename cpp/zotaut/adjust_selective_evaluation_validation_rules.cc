/** \brief Tool to adjust validation rules for journal set to selective
 *         evaluation to avoid failing QS checks for field not eligible
 *         in this context
 *  \author Johannes Riedl
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <iostream>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"
#include "ZoteroHarvesterConfig.h"


namespace {


using namespace ZoteroHarvester;


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " zotero_harvester.conf\n";
    std::exit(EXIT_FAILURE);
}


std::string AssembleValaditionRulesIfNotExist(const std::string journal_id)
{
    const auto tag("084");
    const std::vector<char> subfield_codes({ 'a', '2' });
    const std::vector<std::string> record_types({"regular_article", "review"});
    std::vector<std::string> values;
    const auto field_presence("ignore");
    // The following is the logic for "Insert if not exists beforehand"
    // The SELECTs in the for loop generate anonymous tuples that are merged in an multi row table by UNION ALL
    for (const auto &subfield_code : subfield_codes)
        for (const auto &record_type : record_types)
             values.emplace_back("(SELECT " + journal_id + ", \'" + tag + "\', \'" + subfield_code + "\', NULL, \'"
                                 + record_type + "\', \'"  + field_presence + "\')");

    return std::string("INSERT INTO metadata_presence_tracer ") +
                       "SELECT * FROM (" + StringUtil::Join(values, " UNION ALL ") + ") AS tmp " +
                       "WHERE NOT EXISTS(SELECT 1 FROM metadata_presence_tracer WHERE journal_id=" + journal_id
                                       + " AND marc_field_tag=" + tag + " LIMIT 1);";
}


void LoadHarvesterConfig(const std::string &config_path,
                         std::vector<std::unique_ptr<Config::JournalParams>> * const journal_params)
{
    std::unique_ptr<Config::GlobalParams> global_params;
    std::vector<std::unique_ptr<Config::GroupParams>> group_params;
    Config::LoadHarvesterConfigFile(config_path,
                                    &global_params,
                                    &group_params,
                                    journal_params);
}


std::string GetJournalId(DbConnection * const db_connection, const std::string &zeder_id, const std::string &group) {
    db_connection->queryOrDie("SELECT id FROM zeder_journals WHERE zeder_id=\'" + zeder_id +
                              "\' AND zeder_instance=\'" + group + "\'");
    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty()) {
        return "";
    }
    if (result_set.size() != 1)
        LOG_ERROR("Unable to uniquely determine journal_id for zeder_id " + zeder_id + " and group " + group);
    return result_set.getNextRow()["id"];
}


void UpdateRules(DbConnection * const db_connection, const std::vector<std::unique_ptr<Config::JournalParams>> &journal_params) {
    for (const auto &journal : journal_params)
         if (journal->selective_evaluation_) {
             const std::string journal_id(GetJournalId(db_connection, std::to_string(journal->zeder_id_),
                                          StringUtil::ASCIIToLower(journal->group_)));
             if (journal_id.empty()) {
                 LOG_WARNING("No journal_id result for zeder_id " +  std::to_string(journal->zeder_id_)
                              + " in group " + journal->group_ + " - Skipping journal");
                 continue;
             }
             db_connection->queryOrDie(AssembleValaditionRulesIfNotExist(journal_id));
         }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2)
        Usage();

    const std::string config_path(argv[1]);
    std::vector<std::unique_ptr<Config::JournalParams>> journal_params;
    LoadHarvesterConfig(config_path, &journal_params);
    DbConnection db_connection;
    UpdateRules(&db_connection, journal_params);
    return EXIT_SUCCESS;
}
