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
#include "MarcUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " inferior_title_input superior_title_input norm_data_input\n";
    std::exit(EXIT_FAILURE);
}


static std::unordered_set<std::string> *shared_norm_data_control_numbers;


bool RecordKeywordControlNumbers(MarcUtil::Record * const record, XmlWriter * const /*xml_writer*/,
				 std::string * const /* err_msg */)
{
    std::vector<std::string> keyword_tags;
    StringUtil::Split("600:610:611:630:650:653:656", ':', &keyword_tags);
    const std::vector<std::string> &fields(record->getFields());
    const std::vector<DirectoryEntry> &dir_entries(record->getDirEntries());
    for (const auto &keyword_tag : keyword_tags) {
	const ssize_t start_index(record->getFieldIndex(keyword_tag));
	if (start_index == -1)
	    continue;

        for (size_t index(start_index); index < dir_entries.size() and dir_entries[index].getTag() == keyword_tag; ++index) {
            const Subfields subfields(fields[index]);
            const auto begin_end(subfields.getIterators('0'));
            for (auto subfield0(begin_end.first); subfield0 != begin_end.second; ++subfield0) {
                if (not StringUtil::StartsWith(subfield0->second, "(DE-576)"))
                    continue;

                const std::string topic_id(subfield0->second.substr(8));
		shared_norm_data_control_numbers->insert(topic_id);
            }
        }
    }

    return true;
}


void ExtractKeywordNormdataControlNumbers(File * const marc_input,
					  std::unordered_set<std::string> * const norm_data_control_numbers)
{
    const size_t orig_size(norm_data_control_numbers->size());

    shared_norm_data_control_numbers = norm_data_control_numbers;
    std::string err_msg;
    if (not MarcUtil::ProcessRecords(marc_input, RecordKeywordControlNumbers, nullptr, &err_msg))
	Error("error while extracting keyword control numbers for \"" + marc_input->getPath() + "\": " + err_msg);

    std::cerr << "Found " << (norm_data_control_numbers->size() - orig_size) << " new keyword control numbers in "
	      << marc_input->getPath() << '\n';
}


static unsigned keyword_count, translation_count;
static DbConnection *shared_connection;


bool ExtractTranslations(MarcUtil::Record * const record, XmlWriter * const /*xml_writer*/, std::string * const /* err_msg */) {
    const std::vector<std::string> &fields(record->getFields());
    if (shared_norm_data_control_numbers->find(fields[0]) == shared_norm_data_control_numbers->cend())
	return true; // Not one of the records w/ a keyword used in our title data.

    const ssize_t _150_index(record->getFieldIndex("150"));
    if (_150_index == -1)
	return true;
    const Subfields _150_subfields(fields[_150_index]);
    const std::string german_text(_150_subfields.getFirstSubfieldValue('a'));
    if (unlikely(german_text.empty()))
	return true;
    ++keyword_count;
 
    std::vector<std::pair<std::string, std::string>> text_and_language_codes;
    text_and_language_codes.emplace_back(std::make_pair(german_text, "deu"));

    // Find translations:
    const ssize_t first_750_index(record->getFieldIndex("750"));
    if (first_750_index != -1) {
	const std::vector<DirectoryEntry> &dir_entries(record->getDirEntries());
        for (size_t index(first_750_index); index < dir_entries.size() and dir_entries[index].getTag() == "750"; ++index) {
	    const Subfields _750_subfields(fields[index]);
	    auto start_end(_750_subfields.getIterators('9'));
	    if (start_end.first == start_end.second)
		continue;
	    std::string language_code, association;
	    for (auto code_and_value(start_end.first); code_and_value != start_end.second; ++code_and_value) {
		if (StringUtil::StartsWith(code_and_value->second, "L:"))
		    language_code = code_and_value->second.substr(2);
		else if (StringUtil::StartsWith(code_and_value->second, "Z:"))
		    association = code_and_value->second.substr(2);
	    }
	    if (not language_code.empty()) {
		++translation_count;
		text_and_language_codes.emplace_back(std::make_pair(_750_subfields.getFirstSubfieldValue('a'), language_code));
	    }
        }
    }

    //
    // Update the database.
    //

    // 1. Determine the ID:
    const std::string SELECT_EXISTING("SELECT id FROM translations WHERE text=\"" + german_text + "\" AND language_code=\"deu\"");
    if (not shared_connection->query(SELECT_EXISTING))
        Error("Select failed: " + SELECT_EXISTING + " (" + shared_connection->getLastErrorMessage() + ")");
    DbResultSet id_result_set(shared_connection->getLastResultSet());
    std::string id;
    if (not id_result_set.empty())
	id = id_result_set.getNextRow()["id"];
    else { // We don't have any entries for this German term yet.
	const std::string SELECT_MAX_ID("SELECT MAX(id) FROM translations");
	if (not shared_connection->query(SELECT_MAX_ID))
	    Error("Select failed: " + SELECT_MAX_ID + " (" + shared_connection->getLastErrorMessage() + ")");
	DbResultSet max_id_result_set(shared_connection->getLastResultSet());
	if (max_id_result_set.empty())
	    id = "1";
	else {
	    const std::string max_id(max_id_result_set.getNextRow()["MAX(id)"]); // Apparently SQL NULL can be  returned
	                                                                         // which leads to an empty string here.
	    id = std::to_string(max_id.empty() ? 1 : StringUtil::ToUnsigned(max_id) + 1);
	}
    }

    // 2. Insert the terms in the various languages:
    for (const auto &text_and_language_code : text_and_language_codes) {
	const std::string INSERT_STMT("INSERT IGNORE INTO translations SET id=" + id + ", language_code=\""
				      + text_and_language_code.second + "\", text=\"" + text_and_language_code.first + "\"");
	if (not shared_connection->query(INSERT_STMT))
	    Error("Insert failed: " + INSERT_STMT + " (" + shared_connection->getLastErrorMessage() + ")");
    }

    return true;
}


void ExtractTranslationTerms(File * const norm_data_input, DbConnection * const connection) {
    shared_connection = connection;

    std::string err_msg;
    if (not MarcUtil::ProcessRecords(norm_data_input, ExtractTranslations, nullptr, &err_msg))
	Error("error while extracting translations from \"" + norm_data_input->getPath() + "\": " + err_msg);

    std::cerr << "Added " << keyword_count << " to the translation database.\n";
    std::cerr << "Found " << translation_count << " translations in the norm data.\n";
}

			     
const std::string CONF_FILE_PATH("/var/lib/tuelib/extract_keywords_for_translation.conf");


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 4)
        Usage();

    const std::string inferior_marc_input_filename(argv[1]);
    File inferior_marc_input(inferior_marc_input_filename, "rm");
    if (not inferior_marc_input)
        Error("can't open \"" + inferior_marc_input_filename + "\" for reading!");

    const std::string superior_marc_input_filename(argv[2]);
    File superior_marc_input(superior_marc_input_filename, "rm");
    if (not superior_marc_input)
        Error("can't open \"" + superior_marc_input_filename + "\" for reading!");

    const std::string norm_data_marc_input_filename(argv[3]);
    File norm_data_marc_input(norm_data_marc_input_filename, "rm");
    if (not norm_data_marc_input)
        Error("can't open \"" + norm_data_marc_input_filename + "\" for reading!");

    try {
	const IniFile ini_file(CONF_FILE_PATH);
	const std::string sql_database(ini_file.getString("", "sql_database"));
	const std::string sql_username(ini_file.getString("", "sql_username"));
	const std::string sql_password(ini_file.getString("", "sql_password"));
	DbConnection db_connection(sql_database, sql_username, sql_password);

	std::unordered_set<std::string> norm_data_control_numbers;
	ExtractKeywordNormdataControlNumbers(&inferior_marc_input, &norm_data_control_numbers);
	ExtractKeywordNormdataControlNumbers(&superior_marc_input, &norm_data_control_numbers);
	ExtractTranslationTerms(&norm_data_marc_input, &db_connection);
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}
