/** \file    PhaseExtractKeywordsForTranslation.cc
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

#include "PhaseExtractKeywordsForTranslation.h"

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
#include "MarcUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TimeUtil.h"
#include "TranslationUtil.h"
#include "WallClockTimer.h"
#include "util.h"


const std::string CONF_FILE_PATH("/var/lib/tuelib/translations.conf");

static unsigned keyword_count, translation_count, additional_hits, synonym_count;
static std::unordered_set <std::string> shared_norm_data_control_numbers;
static DbConnection *shared_connection;


PhaseExtractKeywordsForTranslation::PhaseExtractKeywordsForTranslation() {
    const IniFile ini_file(CONF_FILE_PATH);
    const std::string sql_database(ini_file.getString("", "sql_database"));
    const std::string sql_username(ini_file.getString("", "sql_username"));
    const std::string sql_password(ini_file.getString("", "sql_password"));
    shared_connection = new DbConnection(sql_database, sql_username, sql_password);
}


PipelinePhaseState PhaseExtractKeywordsForTranslation::preprocess(const MarcUtil::Record &record, std::string * const) {
    std::vector <std::string> keyword_tags;
    StringUtil::Split("600:610:611:630:650:653:656", ':', &keyword_tags);
    const std::vector <std::string> &fields(record.getFields());
    const std::vector <DirectoryEntry> &dir_entries(record.getDirEntries());
    for (const auto &keyword_tag : keyword_tags) {
        const ssize_t start_index(record.getFieldIndex(keyword_tag));
        if (start_index == -1)
            continue;

        // TODO: Update to new MarcUtil API.
        for (size_t index(start_index); index < dir_entries.size() and dir_entries[index].getTag() == keyword_tag; ++index) {
            const Subfields subfields(fields[index]);
            const auto begin_end(subfields.getIterators('0'));
            for (auto subfield0(begin_end.first); subfield0 != begin_end.second; ++subfield0) {
                if (not StringUtil::StartsWith(subfield0->second, "(DE-576)"))
                    continue;

                const std::string topic_id(subfield0->second.substr(8));
                shared_norm_data_control_numbers.insert(topic_id);
            }
        }
    }
    return SUCCESS;
}


PipelinePhaseState PhaseExtractKeywordsForTranslation::preprocessNormData(const MarcUtil::Record &record, std::string * const) {
    const std::vector <std::string> &fields(record.getFields());
    if (shared_norm_data_control_numbers.find(fields[0]) == shared_norm_data_control_numbers.cend())
        return SUCCESS; // Not one of the records w/ a keyword used in our title data.

    // Extract original German entry:
    const ssize_t _150_index(record.getFieldIndex("150"));
    if (_150_index == -1)
        return SUCCESS;

    const Subfields _150_subfields(fields[_150_index]);
    const std::string german_text(_150_subfields.getFirstSubfieldValue('a'));
    if (unlikely(german_text.empty()))
        return SUCCESS;
    ++keyword_count;

    std::vector <std::pair<std::string, std::string>> text_and_language_codes;
    text_and_language_codes.emplace_back(std::make_pair(german_text, "deu"));

    // Look for German synonyms:
    const std::vector <DirectoryEntry> &dir_entries(record.getDirEntries());
    ssize_t _450_index(record.getFieldIndex("450"));
    if (_450_index != -1) {
        for (/* Intentionally empty! */;
                static_cast<size_t>(_450_index) < fields.size() and dir_entries[_450_index].getTag() == "450"; ++_450_index) {
            const Subfields _450_subfields(fields[_450_index]);
            if (_450_subfields.hasSubfield('a')) {
                text_and_language_codes.emplace_back(std::make_pair(_450_subfields.getFirstSubfieldValue('a'), "deu"));
                ++synonym_count;
            }
        }
    }

    // Find translations:
    const ssize_t first_750_index(record.getFieldIndex("750"));
    if (first_750_index != -1) {
        for (size_t index(first_750_index); index < dir_entries.size() and dir_entries[index].getTag() == "750"; ++index) {
            const Subfields _750_subfields(fields[index]);
            auto start_end(_750_subfields.getIterators('9'));
            if (start_end.first == start_end.second)
                continue;
            std::string language_code;
            for (auto code_and_value(start_end.first); code_and_value != start_end.second; ++code_and_value) {
                if (StringUtil::StartsWith(code_and_value->second, "L:"))
                    language_code = code_and_value->second.substr(2);
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
                text_and_language_codes.emplace_back(std::make_pair(_750_subfields.getFirstSubfieldValue('a'), language_code));
            }
        }
    }

    // Update the database.
    const std::string id(TranslationUtil::GetId(shared_connection, german_text));
    for (const auto &text_and_language_code : text_and_language_codes) {
        const std::string REPLACE_STMT("REPLACE INTO translations SET id=" + id + ", language_code=\""
                                               + text_and_language_code.second + "\", category=\"keywords\", preexists=TRUE, text=\""
                                               + shared_connection->escapeString(text_and_language_code.first) + "\""
        );
        if (not shared_connection->query(REPLACE_STMT))
            Error("Insert failed: " + REPLACE_STMT + " (" + shared_connection->getLastErrorMessage() + ")");
    }
    return SUCCESS;
};


PipelinePhaseState PhaseExtractKeywordsForTranslation::process(MarcUtil::Record &, std::string * const) {
    return SUCCESS;
};


PhaseExtractKeywordsForTranslation::~PhaseExtractKeywordsForTranslation() {
    std::cerr << "Extract keywords for translation:\n";
    std::cerr << "\tAdded " << keyword_count << " to the translation database.\n";
    std::cerr << "\tFound " << translation_count << " translations in the norm data. (" << additional_hits << " due to 'ram' and 'lcsh' entries.)\n";
    std::cerr << "\tFound " << synonym_count << " synonym entries.\n";

    delete shared_connection;
}