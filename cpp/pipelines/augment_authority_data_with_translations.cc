/** \file    augment_authority_data_with_translations.cc
 *  \brief   Extract translations from database amd augment the normdata file
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2016-2021 Library of the University of Tübingen

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
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "IniFile.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


// Save language code, translation, origin, status
typedef std::tuple<std::string, std::string, std::string, std::string> Translation;


static unsigned record_count(0);
static unsigned modified_count(0);

const std::string WIKIDATA_STATUS("unreliable_cat2");


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " authority_data_input authority_data_output\n";
    std::exit(EXIT_FAILURE);
}


inline bool IsReliableSynonym(const std::string &status) {
    return status == "replaced_synonym" or status == "new_synonym" or status == "derived_synonym";
}


std::string ReplaceAngleBracketsWithParentheses(const std::string &value) {
    return StringUtil::Map(value, "<>", "()");
}


void ExtractTranslations(DbConnection * const db_connection,
                         std::unordered_map<std::string, std::vector<Translation>> * const all_translations) {
    db_connection->queryOrDie("SELECT DISTINCT ppn FROM keyword_translations");
    DbResultSet ppn_result_set(db_connection->getLastResultSet());
    while (const DbRow ppn_row = ppn_result_set.getNextRow()) {
        std::string ppn = ppn_row["ppn"];
        db_connection->queryOrDie(
            "SELECT ppn, language_code, translation, origin, status FROM keyword_translations "
            "WHERE ppn='"
            + ppn + "' AND next_version_id IS NULL");
        DbResultSet result_set(db_connection->getLastResultSet());
        std::vector<Translation> translations;
        while (const DbRow row = result_set.getNextRow()) {
            // We are not interested in synonym fields as we will directly derive synonyms from the translation field
            // Furthermore we insert keywords where the german translation is the reference and needs no further
            // inserting
            if (not IsReliableSynonym(row["status"]) and row["language_code"] != "ger") {
                std::string translation(row["translation"]);
                // Handle '#'-separated synonyms appropriately
                if (translation.find('#') == std::string::npos and not translation.empty())
                    translations.emplace_back(ReplaceAngleBracketsWithParentheses(TextUtil::CollapseAndTrimWhitespace(translation)),
                                              row["language_code"], row["origin"], row["status"]);
                else {
                    std::vector<std::string> primary_and_synonyms;
                    StringUtil::SplitThenTrim(translation, "#", " \t\n\r", &primary_and_synonyms);
                    // Use the first translation as non-synonmym
                    if (primary_and_synonyms.size() > 0) {
                        translations.emplace_back(TextUtil::CollapseAndTrimWhitespace(primary_and_synonyms[0]), row["language_code"],
                                                  row["origin"], row["status"]);
                        // Add further synonyms as derived synonyms
                        for (auto synonyms(std::next(primary_and_synonyms.cbegin())); synonyms != primary_and_synonyms.cend(); ++synonyms) {
                            const std::string synonym(TextUtil::CollapseAndTrimWhitespace(*synonyms));
                            translations.emplace_back(ReplaceAngleBracketsWithParentheses(synonym), row["language_code"], row["origin"],
                                                      "derived_synonym");
                        }
                    }
                }
            }
        }
        all_translations->insert(std::make_pair(ppn, translations));
    }
}


void InsertTranslation(MARC::Record * const record, const char indicator1, const char indicator2, const std::string &term,
                       const std::string &language_code, const std::string &status) {
    MARC::Subfields subfields;
    subfields.addSubfield('a', term);
    subfields.addSubfield('9', "L:" + language_code);
    subfields.addSubfield('9', "Z:" + std::string(IsReliableSynonym(status) ? "VW" : "AF"));
    subfields.addSubfield('2', status == WIKIDATA_STATUS ? "WikiData" : "IxTheo");
    record->insertField("750", subfields, indicator1, indicator2);
}


bool HasExistingTranslation(const MARC::Record &record, const std::string &language_code, const std::string &status) {
    // We can have several either previously existing or already inserted synonyms, so don't replace synonyms
    if (IsReliableSynonym(status))
        return false;

    for (const auto &field : record.getTagRange("750")) {
        MARC::Subfields subfields(field.getSubfields());
        if (subfields.hasSubfieldWithValue('2', "IxTheo") and subfields.hasSubfieldWithValue('9', "L:" + language_code)
            and subfields.hasSubfieldWithValue('9', "Z:AF"))
            return true;
    }
    return false;
}


void ProcessRecord(MARC::Record * const record, const std::unordered_map<std::string, std::vector<Translation>> &all_translations) {
    const std::string ppn(record->getControlNumber());
    auto one_translation(all_translations.find(ppn));

    if (one_translation != all_translations.cend()) {
        // We only insert/replace IxTheo-Translations
        for (const auto &one_lang_translation : one_translation->second) {
            std::string term(std::get<0>(one_lang_translation));
            std::string language_code(std::get<1>(one_lang_translation));
            std::string origin(std::get<2>(one_lang_translation));
            std::string status(std::get<3>(one_lang_translation));

            // Skip non-derived synonyms, german terms and unreliable translations
            if ((status != "derived_synonym" and StringUtil::EndsWith(status, "synonym")) or status == "unreliable"
                or language_code == "ger")
                continue;

            // Don't touch MACS translations and leave alone authoritative IxTheo Translations from BSZ
            if (not HasExistingTranslation(*record, language_code, status))
                InsertTranslation(record, ' ', '6', term, language_code, status);
        }
        ++modified_count;
    }
}


void AugmentNormdata(MARC::Reader * const marc_reader, MARC::Writer *marc_writer,
                     const std::unordered_map<std::string, std::vector<Translation>> &all_translations) {
    // Read in all PPNs from authority data
    while (MARC::Record record = marc_reader->read()) {
        ProcessRecord(&record, all_translations);
        marc_writer->write(record);
        ++record_count;
    }

    std::cerr << "Modified " << modified_count << " of " << record_count << " entries.\n";
}


} // unnamed namespace


const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "translations.conf");


int Main(int argc, char **argv) {
    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);

    if (unlikely(marc_input_filename == marc_output_filename))
        LOG_ERROR("Input file equals output file");

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(marc_input_filename));
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename));

    const IniFile ini_file(CONF_FILE_PATH);
    const std::string sql_database(ini_file.getString("Database", "sql_database"));
    const std::string sql_username(ini_file.getString("Database", "sql_username"));
    const std::string sql_password(ini_file.getString("Database", "sql_password"));
    DbConnection db_connection(DbConnection::MySQLFactory(sql_database, sql_username, sql_password));

    std::unordered_map<std::string, std::vector<Translation>> all_translations;
    ExtractTranslations(&db_connection, &all_translations);

    AugmentNormdata(marc_reader.get(), marc_writer.get(), all_translations);

    return EXIT_SUCCESS;
}
