/** \file    extract_keywords_for_translation.cc
 *  \brief   A tool for extracting keywords that need to be translated.  The keywords and any possibly pre-existing
 *           translations will be stored in a SQL database.
 *  \author  Dr. Johannes Ruscheinski
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
#include <unordered_set>
#include <utility>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "IniFile.h"
#include "MARC.h"
#include "StringUtil.h"
#include "TranslationUtil.h"
#include "UBTools.h"
#include "util.h"


[[noreturn]] void Usage() {
    ::Usage("norm_data_input");
}


static unsigned keyword_count, translation_count, additional_hits, synonym_count, german_term_count;
static DbConnection *shared_connection;
enum Status { RELIABLE, UNRELIABLE, RELIABLE_SYNONYM, UNRELIABLE_SYNONYM };
const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "translations.conf");


std::string StatusToString(const Status status) {
    switch (status) {
    case RELIABLE:
        return "reliable";
    case UNRELIABLE:
        return "unreliable";
    case RELIABLE_SYNONYM:
        return "reliable_synonym";
    case UNRELIABLE_SYNONYM:
        return "unreliable_synonym";
    }

    LOG_ERROR("in StatusToString: we should *never* get here!");
}


using TextLanguageCodeStatusAndOriginTag = std::tuple<std::string, std::string, Status, std::string, bool>;


void ExtractGermanTerms(const MARC::Record &record,
                        std::vector<TextLanguageCodeStatusAndOriginTag> * const text_language_codes_statuses_and_origin_tags) {
    bool updated_german(false);
    for (const auto &_150_field : record.getTagRange("150")) {
        const MARC::Subfields _150_subfields(_150_field.getSubfields());
        // $a is non-repeatable in 150 and necessary
        if (likely(_150_subfields.hasSubfield('a'))) {
            std::string complete_keyword_phrase;
            for (auto subfield : _150_subfields) {
                if (subfield.code_ == 'a')
                    complete_keyword_phrase = StringUtil::RemoveChars("<>", &(subfield.value_));
                // $x and $g are repeatable and possibly belong to each other
                if (subfield.code_ == 'x') {
                    complete_keyword_phrase += " / " + subfield.value_;
                    updated_german = true;
                }
                if (subfield.code_ == '9') {
                    std::string _9_subfield(subfield.value_);
                    if (StringUtil::StartsWith(_9_subfield, "g:")) {
                        _9_subfield = _9_subfield.substr(2);
                        complete_keyword_phrase += " <" + StringUtil::RemoveChars("<>", &_9_subfield) + ">";
                    }
                }
            }

            text_language_codes_statuses_and_origin_tags->emplace_back(
                complete_keyword_phrase, TranslationUtil::MapGermanLanguageCodesToFake3LetterEnglishLanguagesCodes("deu"), RELIABLE, "150",
                updated_german);
            ++german_term_count;
        }
    }
}


void ExtractGermanSynonyms(const MARC::Record &record,
                           std::vector<TextLanguageCodeStatusAndOriginTag> * const text_language_codes_statuses_and_origin_tags) {
    for (const auto &_450_field : record.getTagRange("450")) {
        const MARC::Subfields _450_subfields(_450_field.getSubfields());
        if (_450_subfields.hasSubfield('a')) {
            text_language_codes_statuses_and_origin_tags->emplace_back(
                _450_subfields.getFirstSubfieldWithCode('a'),
                TranslationUtil::MapGermanLanguageCodesToFake3LetterEnglishLanguagesCodes("deu"), RELIABLE_SYNONYM, "450",
                false /*updated_german*/);
            ++synonym_count;
        }
    }
}


bool IsSynonym(const MARC::Subfields &_750_subfields) {
    for (const auto &_9_subfield : _750_subfields.extractSubfields('9')) {
        if (_9_subfield == "Z:VW")
            return true;
    }
    return false;
}


void ExtractNonGermanTranslations(const MARC::Record &record,
                                  std::vector<TextLanguageCodeStatusAndOriginTag> * const text_language_codes_statuses_and_origin_tags) {
    for (const auto &_750_field : record.getTagRange("750")) {
        const MARC::Subfields _750_subfields(_750_field.getSubfields());
        std::vector<std::string> _9_subfields(_750_subfields.extractSubfields('9'));
        if (_9_subfields.empty())
            continue;
        std::string language_code;
        for (const auto &_9_subfield : _9_subfields) {
            if (StringUtil::StartsWith(_9_subfield, "L:"))
                language_code = _9_subfield.substr(2);
        }
        if (language_code.empty() and _750_subfields.hasSubfield('2')) {
            const std::string _750_2(_750_subfields.getFirstSubfieldWithCode('2'));
            if (_750_2 == "lcsh")
                language_code = "eng";
            else if (_750_2 == "ram")
                language_code = "fra";
            if (not language_code.empty())
                ++additional_hits;
        }

        language_code = TranslationUtil::MapGermanLanguageCodesToFake3LetterEnglishLanguagesCodes(language_code);
        if (language_code != "???") {
            const bool is_synonym(IsSynonym(_750_subfields));
            Status status;
            if (_750_subfields.getFirstSubfieldWithCode('2') == "IxTheo")
                status = is_synonym ? RELIABLE_SYNONYM : RELIABLE;
            else
                status = is_synonym ? UNRELIABLE_SYNONYM : UNRELIABLE;
            ++translation_count;
            text_language_codes_statuses_and_origin_tags->emplace_back(_750_subfields.getFirstSubfieldWithCode('a'), language_code, status,
                                                                       "750", false);
        }
    }
}


// Helper function for ExtractTranslations().
void FlushToDatabase(std::string &insert_statement) {
    // Remove trailing comma and space:
    insert_statement.resize(insert_statement.size() - 2);

    insert_statement += ';';
    shared_connection->queryOrDie(insert_statement);
}


// Returns a string that looks like "(language_code='deu' OR language_code='eng')" etc.
std::string GenerateLanguageCodeWhereClause(
    const std::vector<TextLanguageCodeStatusAndOriginTag> &text_language_codes_statuses_and_origin_tags) {
    std::string partial_where_clause;

    partial_where_clause += '(';
    std::set<std::string> already_seen;
    for (const auto &text_language_code_status_and_origin : text_language_codes_statuses_and_origin_tags) {
        const std::string &language_code(std::get<1>(text_language_code_status_and_origin));
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


std::string GetPseudoGNDSigil(const IniFile &ini_file) {
    static const std::string pseudo_gnd_sigil(
        ini_file.lookup("Configuration", "pseudo_gnd_sigil", nullptr) ? ini_file.getString("Configuration", "pseudo_gnd_sigil") : "");
    return pseudo_gnd_sigil;
}


bool HasPseudoGNDCode(const MARC::Record &record, const IniFile &ini_file, std::string * const pseudo_gnd_code) {
    const std::string pseudo_gnd_sigil(GetPseudoGNDSigil(ini_file));
    if (pseudo_gnd_sigil.empty())
        return false;
    pseudo_gnd_code->clear();
    for (const auto &_035_field : record.getTagRange("035")) {
        const MARC::Subfields _035_subfields(_035_field.getSubfields());
        const std::string _035a_field(_035_subfields.getFirstSubfieldWithCode('a'));
        if (StringUtil::StartsWith(_035a_field, pseudo_gnd_sigil)) {
            *pseudo_gnd_code = _035a_field.substr(pseudo_gnd_sigil.length());
            return not pseudo_gnd_code->empty();
        }
    }
    return false;
}


std::string IsPriorityEntry(const MARC::Record &record) {
    return record.hasTag("PRI") ? "true" : "false";
}


bool ExtractTranslationsForASingleRecord(const MARC::Record * const record, const IniFile &ini_file) {
    // Skip records that are not GND records:
    std::string gnd_code;
    std::string pseudo_gnd_code;
    if (not MARC::GetGNDCode(*record, &gnd_code) and not HasPseudoGNDCode(*record, ini_file, &pseudo_gnd_code))
        return true;

    // Extract all synonyms and translations:
    std::vector<TextLanguageCodeStatusAndOriginTag> text_language_codes_statuses_and_origin_tags;
    ExtractGermanTerms(*record, &text_language_codes_statuses_and_origin_tags);
    ExtractGermanSynonyms(*record, &text_language_codes_statuses_and_origin_tags);
    ExtractNonGermanTranslations(*record, &text_language_codes_statuses_and_origin_tags);
    if (text_language_codes_statuses_and_origin_tags.empty())
        return true;

    ++keyword_count;

    // Remove entries for which authoritative translation were shipped to us from the BSZ:
    // if prev_version_id!=null -> it's a successor
    // if next_version_id!=null -> it has been modified by translation tool (stored proc.)
    const std::string ppn(record->getControlNumber());
    shared_connection->queryOrDie("DELETE FROM keyword_translations WHERE ppn=\"" + ppn + "\" AND "
                                  + "prev_version_id IS NULL AND next_version_id IS NULL AND translator IS NULL AND "
                                  + GenerateLanguageCodeWhereClause(text_language_codes_statuses_and_origin_tags));

    if (not MARC::GetGNDCode(*record, &gnd_code)) {
        ++no_gnd_code_count;
        gnd_code = "0";
    }

    std::vector<std::string> gnd_systems;
    for (const auto &_065_field : record->getTagRange("065")) {
        std::vector<std::string> _065a_subfields(_065_field.getSubfields().extractSubfields('a'));
        gnd_systems.insert(gnd_systems.begin(), _065a_subfields.begin(), _065a_subfields.end());
    }
    std::string gnd_system(StringUtil::Join(gnd_systems, ","));
    const std::string INSERT_STATEMENT_START(
        "INSERT IGNORE INTO keyword_translations (ppn,gnd_code,language_code,"
        "translation,status,origin,gnd_system,german_updated, priority_entry) VALUES ");
    std::string insert_statement(INSERT_STATEMENT_START);

    size_t row_counter(0);
    const size_t MAX_ROW_COUNT(1000);

    // Update the database:
    for (const auto &text_language_code_status_and_origin : text_language_codes_statuses_and_origin_tags) {
        const std::string language_code(shared_connection->escapeString(std::get<1>(text_language_code_status_and_origin)));
        const std::string translation(shared_connection->escapeString(std::get<0>(text_language_code_status_and_origin)));
        const std::string status(StatusToString(std::get<2>(text_language_code_status_and_origin)));
        const std::string &origin(std::get<3>(text_language_code_status_and_origin));
        const bool updated_german(std::get<4>(text_language_code_status_and_origin));

        // check if there is an existing entry, insert ignore does not work here
        // any longer due to the deleted unique key for the history functionality.
        // Unsure if it worked before due to translator=null not affecting a ukey in mysql
        const std::string CHECK_EXISTING("SELECT ppn FROM keyword_translations WHERE ppn=\"" + ppn + "\" AND language_code=\""
                                         + language_code + "\";");
        shared_connection->queryOrDie(CHECK_EXISTING);
        DbResultSet result(shared_connection->getLastResultSet());
        if (not result.empty()) {
            continue;
        }

        insert_statement += "('" + ppn + "', '" + gnd_code + "', '" + language_code + "', '" + translation + "', '" + status + "', '"
                            + origin + "', '" + gnd_system + "', " + (updated_german ? "true" : "false") + ", " + IsPriorityEntry(*record)
                            + "), ";
        if (++row_counter > MAX_ROW_COUNT) {
            FlushToDatabase(insert_statement);
            insert_statement = INSERT_STATEMENT_START;
            row_counter = 0;
        }
    }
    if (row_counter > 0)
        FlushToDatabase(insert_statement);

    return true;
}


void ExtractTranslationsForAllRecords(MARC::Reader * const authority_reader, const IniFile &ini_file) {
    while (const MARC::Record record = authority_reader->read()) {
        if (not ExtractTranslationsForASingleRecord(&record, ini_file))
            LOG_ERROR("error while extracting translations from \"" + authority_reader->getPath() + "\"");
    }
    std::cerr << "Added " << keyword_count << " keywords to the translation database.\n";
    std::cerr << "Found " << german_term_count << " german terms.\n";
    std::cerr << "Found " << translation_count << " translations in the norm data. (" << additional_hits
              << " due to 'ram' and 'lcsh' entries.)\n";
    std::cerr << "Found " << synonym_count << " synonym entries.\n";
    std::cerr << no_gnd_code_count << " authority records had no GND code.\n";
}


int Main(int argc, char **argv) {
    if (argc != 2)
        Usage();

    std::unique_ptr<MARC::Reader> authority_marc_reader(MARC::Reader::Factory(argv[1], MARC::FileType::BINARY));
    try {
        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("Database", "sql_database"));
        const std::string sql_username(ini_file.getString("Database", "sql_username"));
        const std::string sql_password(ini_file.getString("Database", "sql_password"));
        DbConnection db_connection(DbConnection::MySQLFactory(sql_database, sql_username, sql_password));
        shared_connection = &db_connection;

        ExtractTranslationsForAllRecords(authority_marc_reader.get(), ini_file);
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }

    return EXIT_SUCCESS;
}
