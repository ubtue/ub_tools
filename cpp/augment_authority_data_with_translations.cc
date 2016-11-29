/** \file    augment_authority_data_with_translations.cc
 *  \brief   Extract translations from database amd augment the normdata file
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2016, Library of the University of Tübingen

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
#include <vector>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "DirectoryEntry.h"
#include "IniFile.h"
#include "Leader.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "MarcUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TimeUtil.h"
#include "TranslationUtil.h"
#include "util.h"

// Save language code, translation, origin, status
typedef std::tuple<std::string, std::string, std::string, std::string> OneTranslation;

static unsigned record_count(0);
static unsigned modified_count(0);

void Usage() {
    std::cerr << "Usage: " << ::progname << " authority_data_input authority_data_output\n";
    std::exit(EXIT_FAILURE);
}


inline bool IsSynonym(const std::string &status) { return status == "replaced_synonym" or status == "new_synonym"; }


void ExecSqlOrDie(const std::string &select_statement, DbConnection * const connection) {
    if (unlikely(not connection->query(select_statement)))
        Error("SQL Statement failed: " + select_statement + " (" + connection->getLastErrorMessage() + ")");
}


void ExtractTranslations(DbConnection * const db_connection, std::map<std::string, std::vector<OneTranslation> > * const all_translations) {
    ExecSqlOrDie("SELECT DISTINCT ppn FROM keyword_translations", db_connection);
    DbResultSet ppn_result_set(db_connection->getLastResultSet());
    while (const DbRow ppn_row = ppn_result_set.getNextRow()) {
        std::string ppn = ppn_row["ppn"];
        ExecSqlOrDie("SELECT ppn, language_code, translation, origin, status FROM keyword_translations WHERE ppn='" + ppn + "'", db_connection);
        DbResultSet result_set(db_connection->getLastResultSet());
        std::vector<OneTranslation> translations;
        while (const DbRow row = result_set.getNextRow()) {
            // We are not interested in synonym fields
            if (not IsSynonym(row["status"]))
                translations.emplace_back(row["translation"], row["language_code"], row["origin"], row["status"]);
        }
        all_translations->insert(std::make_pair(ppn, translations));
    }
}


std::string MapLanguageCode(std::string lang_code) {
    if (lang_code == "deu")
        return "de";
    if (lang_code == "eng")
        return "en";
    if (lang_code == "fra")
        return "fr";
    if (lang_code == "dut")
        return "nl";
    if (lang_code == "ita")
        return "it";
    if (lang_code == "spa")
        return "es";
    if (lang_code == "hant")
        return "zh-Hant";
    if (lang_code == "hans")
        return "zh-Hans";
    Error("Unknown language code " + lang_code);
}


void InsertTranslation(MarcRecord * const record, char indicator1, char indicator2, std::string term, std::string language_code, std::string status) {
    Subfields subfields(indicator1, indicator2);
    subfields.addSubfield('a', term);
    subfields.addSubfield('9', "L:" + MapLanguageCode(language_code));
    subfields.addSubfield('9', "Z:" + std::string(IsSynonym(status) ? "VW" : "AF"));
    subfields.addSubfield('2', "IxTheo");
    record->insertField("750", subfields);
}


char DetermineNextFreeIndicator1(MarcRecord * const record, std::vector<size_t> field_indices) {
    char new_indicator1 = ' ';

    for (const auto field_index : field_indices) {
        Subfields subfields(record->getSubfields(field_index));
        char indicator1(subfields.getIndicator1());
        if (indicator1 == '9')
            Error("Indicator1 cannot be further incremented");
        if (indicator1 == ' ')
            new_indicator1 = '1';
        else 
            new_indicator1 = indicator1 >= new_indicator1 ? (indicator1 + 1) : new_indicator1;
    }

    return new_indicator1;
}


size_t GetFieldIndexForExistingTranslation(const MarcRecord *record, const std::vector<size_t> &field_indices, const std::string &language_code) {
    for (auto field_index : field_indices) {
        Subfields subfields_present(record->getSubfields(field_index));
        if (subfields_present.hasSubfieldWithValue('2', "IxTheo") and subfields_present.hasSubfieldWithValue('9', "L:" + MapLanguageCode(language_code)))
            return field_index;
    }
    return MarcRecord::FIELD_NOT_FOUND;
}


void ProcessRecord(MarcRecord * const record, const std::map<std::string, std::vector<OneTranslation> > &all_translations) {
    const std::string ppn(record->getControlNumber());
    auto one_translation(all_translations.find(ppn));

    if (one_translation != all_translations.cend()) {
        // We only insert/replace IxTheo-Translations
        // For MACS Translations we insert an additional field
        for (auto &one_lang_translation : one_translation->second) {
            std::string term(std::get<0>(one_lang_translation));
            std::string language_code(std::get<1>(one_lang_translation));
            std::string origin(std::get<2>(one_lang_translation));
            std::string status(std::get<3>(one_lang_translation));

            // Skip synonyms, german terms and unreliable translations
            if (StringUtil::EndsWith(status, "synonym") or status == "unreliable" or language_code == "deu")
                continue;

            // Don't touch MACS translations, but find and potentially replace IxTheo translations
            std::vector<size_t> field_indices;
            if (record->getFieldIndices("750", &field_indices) > 0) {
                const size_t field_index(GetFieldIndexForExistingTranslation(record, field_indices, language_code));
                if (field_index != MarcRecord::FIELD_NOT_FOUND)
                    record->deleteField(field_index);
            }
            const char indicator1(DetermineNextFreeIndicator1(record, field_indices));
            InsertTranslation(record, indicator1, ' ', term, language_code, status);
        }
        ++modified_count;
    }
}


void AugmentNormdata(MarcReader * const marc_reader, MarcWriter *marc_writer, const std::map<std::string, std::vector<OneTranslation> > &all_translations) {

    // Read in all PPNs from authority data
    while (MarcRecord record = marc_reader->read()) {
        ProcessRecord(&record, all_translations);
        marc_writer->write(record);
        ++record_count;
    }

    std::cerr << "Modified " << modified_count << " of " << record_count << " entries.\n";
}

const std::string CONF_FILE_PATH("/var/lib/tuelib/translations.conf");

int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string marc_input_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);

    if (unlikely(marc_input_filename == marc_output_filename))
        Error("Input file equals output file");

    try {
        std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(marc_input_filename));
        std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename));

        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("", "sql_database"));
        const std::string sql_username(ini_file.getString("", "sql_username"));
        const std::string sql_password(ini_file.getString("", "sql_password"));
        DbConnection db_connection(sql_database, sql_username, sql_password);

        std::map<std::string, std::vector<OneTranslation> > all_translations;
        ExtractTranslations(&db_connection, &all_translations);

        AugmentNormdata(marc_reader.get(), marc_writer.get(), all_translations);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
