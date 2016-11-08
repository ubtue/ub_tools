/** \file    extract_keywords_for_translation.cc
 *  \brief   A tool for extracting keywords that need to be translated.  The keywords and any possibly pre-existing
 *           translations will be stored in a SQL database.
 *  \author  Dr. Johannes Ruscheinski
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
#include <unordered_set>
#include <utility>
#include <vector>
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


void Usage() {
    std::cerr << "Usage: " << ::progname << " title_input norm_data_input\n";
    std::exit(EXIT_FAILURE);
}

static unsigned keyword_count, translation_count, additional_hits, synonym_count;
static DbConnection *shared_connection;


void ExtractGermanSynonyms(const MarcRecord &record,
                           std::vector<std::pair<std::string, std::string>> * const text_and_language_codes)
{
    for (size_t _450_index(record.getFieldIndex("450"));
         _450_index < record.getNumberOfFields() and record.getTag(_450_index) == "450"; ++_450_index)
    {
        const Subfields _450_subfields(record.getSubfields(_450_index));
        if (_450_subfields.hasSubfield('a')) {
            text_and_language_codes->emplace_back(std::make_pair(_450_subfields.getFirstSubfieldValue('a'), "deu"));
            ++synonym_count;
        }
    }
}


void ExtractNonGermanTranslations(const MarcRecord &record,
                                  std::vector<std::pair<std::string, std::string>> * const text_and_language_codes) {
    for (size_t index(record.getFieldIndex("750"));
         index < record.getNumberOfFields() and record.getTag(index) == "750"; ++index) {
        const Subfields _750_subfields(record.getSubfields(index));
        auto start_end(_750_subfields.getIterators('9'));
        if (start_end.first == start_end.second)
            continue;
        std::string language_code;
        for (auto code_and_value(start_end.first); code_and_value != start_end.second; ++code_and_value) {
            if (StringUtil::StartsWith(code_and_value->value_, "L:"))
                language_code = code_and_value->value_.substr(2);
        }
        if (language_code.empty() and _750_subfields.hasSubfield('2')) {
            const std::string _750_2(_750_subfields.getFirstSubfieldValue('2'));
            if (_750_2 == "lcsh")
                language_code = "eng";
            else if (_750_2 == "ram")
                language_code = "fra";
            if (not language_code.empty())
                ++additional_hits;
        }
        if (not language_code.empty()) {
            ++translation_count;
            text_and_language_codes->emplace_back(std::make_pair(_750_subfields.getFirstSubfieldValue('a'),
                                                                 language_code));
        }
    }
}


// Helper function for ExtractTranslations().
void FlushToDatabase(std::string &insert_statement) {
    // Remove trailing comma and space:
    insert_statement.resize(insert_statement.size() - 2);

    insert_statement += ';';
    if (not shared_connection->query(insert_statement))
        Error("Insert failed: " + insert_statement + " (" + shared_connection->getLastErrorMessage() + ")");
}


// Returns a string that looks like "(language_code='deu' OR language_code='eng')" etc.
std::string GenerateLanguageCodeWhereClause(
    const std::vector<std::pair<std::string, std::string>> &text_and_language_codes)
{
    std::string partial_where_clause;

    partial_where_clause += '(';
    std::set<std::string> already_seen;
    for (const auto &text_and_language_code : text_and_language_codes) {
        const std::string &language_code(text_and_language_code.second);
        if (already_seen.find(language_code) == already_seen.cend()) {
            already_seen.insert(language_code);
            if (partial_where_clause.length() > 1)
                partial_where_clause += " OR ";
            partial_where_clause += "language_code='" + language_code + "'";
        }
    }
    partial_where_clause += ')';

    return partial_where_clause;
}


static unsigned no_gnd_code_count;


bool ExtractTranslationsForASingleRecord(MarcRecord * const record, MarcWriter * const /*marc_writer*/,
                                         std::string * const /* err_msg */)
{
    // Extract all synonyms and translations:
    std::vector<std::pair<std::string, std::string>> text_and_language_codes;
    ExtractGermanSynonyms(*record, &text_and_language_codes);
    ExtractNonGermanTranslations(*record, &text_and_language_codes);
    if (text_and_language_codes.empty())
        return true;

    ++keyword_count;

    // Remove entries for which authoritative translation were shipped to us from the BSZ:
    const std::string ppn(record->getControlNumber());
    const std::string DELETE_STMT("DELETE FROM keyword_translations WHERE ppn=\"" + ppn + "\" AND "
                                  + GenerateLanguageCodeWhereClause(text_and_language_codes));
    if (not shared_connection->query(DELETE_STMT))
        Error("Delete failed: " + DELETE_STMT + " (" + shared_connection->getLastErrorMessage() + ")");

    std::string gnd_code;
    if (not MarcUtil::GetGNDCode(*record, &gnd_code)) {
        ++no_gnd_code_count;
        gnd_code = "0";
    }
    
    const std::string INSERT_STATEMENT_START("INSERT INTO keyword_translations (ppn,gnd_code,language_code,"
                                             "translation,preexists) VALUES ");
    std::string insert_statement(INSERT_STATEMENT_START);

    size_t row_counter(0);
    const size_t MAX_ROW_COUNT(1000);

    // Update the database:
    for (const auto &text_and_language_code : text_and_language_codes) {
        const std::string language_code(shared_connection->escapeString(text_and_language_code.second));
        const std::string translation(shared_connection->escapeString(text_and_language_code.first));
        insert_statement += "('" + ppn + "', '" + gnd_code + "', '" + language_code + "', '" + translation
                            + "', TRUE), ";
        if (++row_counter > MAX_ROW_COUNT) {
            FlushToDatabase(insert_statement);
            insert_statement = INSERT_STATEMENT_START;
            row_counter = 0;
        }
    }
    FlushToDatabase(insert_statement);

    return true;
}


void ExtractTranslationsForAllRecords(MarcReader * const authority_reader) {
    std::string err_msg;
    if (not MarcRecord::ProcessRecords(authority_reader, ExtractTranslationsForASingleRecord, nullptr, &err_msg))
        Error("error while extracting translations from \"" + authority_reader->getPath() + "\": " + err_msg);

    std::cerr << "Added " << keyword_count << " keywords to the translation database.\n";
    std::cerr << "Found " << translation_count << " translations in the norm data. (" << additional_hits
              << " due to 'ram' and 'lcsh' entries.)\n";
    std::cerr << "Found " << synonym_count << " synonym entries.\n";
    std::cerr << no_gnd_code_count << " authority records had no GND code.\n";
}


const std::string CONF_FILE_PATH("/var/lib/tuelib/translations.conf");


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    /*FIXME
    const std::string marc_input_filename(argv[1]);
    File marc_input(marc_input_filename, "r");
    if (not marc_input)
        Error("can't open \"" + marc_input_filename + "\" for reading!");
    */
    
    std::unique_ptr<MarcReader> authority_marc_reader(MarcReader::Factory(argv[2], MarcReader::BINARY));

    try {
        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("", "sql_database"));
        const std::string sql_username(ini_file.getString("", "sql_username"));
        const std::string sql_password(ini_file.getString("", "sql_password"));
        DbConnection db_connection(sql_database, sql_username, sql_password);
        shared_connection = &db_connection;

        ExtractTranslationsForAllRecords(authority_marc_reader.get());
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
