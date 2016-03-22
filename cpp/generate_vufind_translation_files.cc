/** \file    generate_vufind_translation_files.cc
 *  \brief   A tool for creating the ".ini" files VuFind uses based on data in the SQL translations table.
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

#include <algorithm>
#include <iostream>
#include <map>
#include <utility>
#include <vector>
#include <cstring>
#include "File.h"
#include "FileUtil.h"
#include "TranslationUtil.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "IniFile.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " output_directory_path\n";
    std::exit(EXIT_FAILURE);
}


// Generates a XX.ini output file with sorted entries.  The XX is a 2-letter language code.
void ProcessLanguage(const std::string &output_file_path, const std::string &_3letter_code, DbConnection * const db_connection) {
    if (unlikely(not FileUtil::RenameFile(output_file_path, output_file_path + ".bak", /* remove_target = */true)))
	Error("failed to rename \"" + output_file_path + "\" to \"" + output_file_path + ".bak\"! ("
	      + std::string(::strerror(errno)) + ")");

    File output(output_file_path, "w");
    if (unlikely(output.fail()))
	Error("failed to open \"" + output_file_path + "\" for writing!");

    const std::string SELECT_STMT("SELECT token,text FROM translations WHERE language_code='" + _3letter_code + "' AND "
				  "category='vufind_translations'");
    if (unlikely(not db_connection->query(SELECT_STMT)))
	Error("Select failed: " + SELECT_STMT + " (" + db_connection->getLastErrorMessage() + ")");

    DbResultSet result_set(db_connection->getLastResultSet());
    if (unlikely(result_set.empty()))
	Error("found no translations for language code \"" + _3letter_code + "\"!");

    std::vector<std::pair<std::string, std::string>> tokens_and_texts;
    while (const DbRow row = result_set.getNextRow())
	tokens_and_texts.emplace_back(row[0], row[1]);

    std::sort(tokens_and_texts.begin(), tokens_and_texts.end(),
	      [](const std::pair<std::string,std::string> &left, const std::pair<std::string,std::string> &right)
	          { return left.first < right.first; });

    for (const auto &token_and_text : tokens_and_texts)
	output << token_and_text.first << " = \"" << token_and_text.second << "\"\n";

    std::cout << "Wrote " << tokens_and_texts.size() << " language mappings to \"" << output_file_path << "\"\n";
}


void GetLanguageCodes(DbConnection * const db_connection, std::map<std::string, std::string> * language_codes) {
    const std::string SELECT_STMT("SELECT DISTINCT language_code FROM translations WHERE category = 'vufind_translations'");
    if (unlikely(not db_connection->query(SELECT_STMT)))
	Error("Select failed: " + SELECT_STMT + " (" + db_connection->getLastErrorMessage() + ")");

    DbResultSet language_codes_result_set(db_connection->getLastResultSet());
    if (unlikely(language_codes_result_set.empty()))
	Error("no language codes found, expected multiple!");

    while (const DbRow row = language_codes_result_set.getNextRow())
	language_codes->emplace(TranslationUtil::MapGerman3LetterCodeToInternational2LetterCode(row[0]), row[0]);
}

			     
const std::string CONF_FILE_PATH("/var/lib/tuelib/translations.conf");


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    const std::string output_directory(argv[1]);
    if (unlikely(not FileUtil::IsDirectory(output_directory)))
	Error("\"" + output_directory + "\" is not a directory or can't be read!");

    try {
	const IniFile ini_file(CONF_FILE_PATH);
	const std::string sql_database(ini_file.getString("", "sql_database"));
	const std::string sql_username(ini_file.getString("", "sql_username"));
	const std::string sql_password(ini_file.getString("", "sql_password"));
	DbConnection db_connection(sql_database, sql_username, sql_password);

	std::map<std::string, std::string> _2letter_and_3letter_codes;
	GetLanguageCodes(&db_connection, &_2letter_and_3letter_codes);
	for (const auto &_2letter_intl_code_and_german_3letter_code : _2letter_and_3letter_codes)
	    ProcessLanguage(output_directory + "/" + _2letter_intl_code_and_german_3letter_code.first + ".ini",
			    _2letter_intl_code_and_german_3letter_code.second, &db_connection);
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}
