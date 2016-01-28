/** \file    extract_vufind_translations_for_translation.cc
 *  \brief   A tool for extracting translations that need to be translated.  The keywords and any possibly pre-existing
 *           translations will be stored in an SQL database.
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
#include <map>
#include <unordered_map>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "File.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "TranslationUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " de.ini other_local_vufind_language_maps\n";
    std::exit(EXIT_FAILURE);
}


void ReadIniFile(const std::string &ini_filename, std::unordered_map<std::string, std::string> * const english_to_other_map) {
    File input(ini_filename, "rm");
    if (not input)
	throw std::runtime_error("can't open \"" + ini_filename + "\" for reading!");

    unsigned line_no(0);
    while (not input.eof()) {
	++line_no;
	std::string line;
	if (input.getline(&line) == 0 or line.empty())
	    continue;

	const std::string::size_type first_equal_pos(line.find('='));
	if (unlikely(first_equal_pos == std::string::npos))
	    throw std::runtime_error("missing equal-sign in \"" + ini_filename + "\" on line " + std::to_string(line_no) + "!");

	const std::string key(StringUtil::Trim(line.substr(0, first_equal_pos)));
	if (unlikely(key.empty()))
	    throw std::runtime_error("missing English key in \"" + ini_filename + "\" on line " + std::to_string(line_no) + "!");

	std::string rest(StringUtil::Trim(line.substr(first_equal_pos + 1)));
	if (unlikely(rest.empty()))
	    throw std::runtime_error("missing translation in \"" + ini_filename + "\" on line " + std::to_string(line_no)
				     + "! (1)");
	if (rest[0] == '"') {
	}

	(*english_to_other_map)[key] = rest;
    }

    std::cout << "Read " << english_to_other_map->size() << " mappings from English to another language from \"" << ini_filename
	      << "\".\n";
}


void InsertGerman(DbConnection * const connection,
		  const std::unordered_map<std::string, std::string> &keys_to_german_map)
{
    for (const auto &key_and_german : keys_to_german_map) {
	const std::string id(TranslationUtil::GetId(connection, key_and_german.second));
	const std::string INSERT_GERMAN("REPLACE INTO translations SET id=" + id
					+ ", language_code=\"deu\", category=\"vufind_translations\", preexists=TRUE, text=\""
					+ connection->escapeString(key_and_german.second) + "\"");
	if (not connection->query(INSERT_GERMAN))
	    Error("Insert failed: " + INSERT_GERMAN + " (" + connection->getLastErrorMessage() + ")");
    }
}


void InsertOther(DbConnection * const connection, const std::string &language_code,
		 const std::unordered_map<std::string, std::string> &keys_to_german_map,
		 const std::unordered_map<std::string, std::string> &keys_to_other_map)
{
    for (const auto &key_and_other : keys_to_other_map) {
	const auto &key_and_german(keys_to_german_map.find(key_and_other.first));
	if (unlikely(key_and_german == keys_to_german_map.cend()))
	    continue;
	const std::string id(TranslationUtil::GetId(connection, key_and_german->second));

	const std::string INSERT_OTHER("REPLACE INTO translations SET id=" + id + ", language_code=\""
				       + language_code + "\", category=\"vufind_translations\", preexists=TRUE, text=\""
				       + connection->escapeString(key_and_other.second) + "\"");
	if (not connection->query(INSERT_OTHER))
	    Error("Insert failed: " + INSERT_OTHER + " (" + connection->getLastErrorMessage() + ")");
    }
}


static std::map<std::string, std::string> intl_2letter_code_to_german_3letter_code_map{
    { "de", "deu" },
    { "en", "eng" },
    { "fr", "fra" }
};


const std::string CONF_FILE_PATH("/var/lib/tuelib/translations.conf");


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc < 3)
        Usage();

    try {
	const IniFile ini_file(CONF_FILE_PATH);
	const std::string sql_database(ini_file.getString("", "sql_database"));
	const std::string sql_username(ini_file.getString("", "sql_username"));
	const std::string sql_password(ini_file.getString("", "sql_password"));
	DbConnection db_connection(sql_database, sql_username, sql_password);

	const std::string de_ini_filename(argv[1]);
	if (unlikely(de_ini_filename != "de.ini" and not StringUtil::EndsWith(de_ini_filename, "/de.ini")))
	    Error("first INI file must be \"de.ini\"!");

	std::unordered_map<std::string, std::string> keys_to_german_map;
	ReadIniFile(de_ini_filename, &keys_to_german_map);
	InsertGerman(&db_connection, keys_to_german_map);

	for (int arg_no(2); arg_no < argc; ++arg_no) {
	    // Get the 2-letter language code from the filename.  We expect filenames of the form "xx.ini" or "some_path/xx.ini":
	    const std::string ini_filename(argv[arg_no]);
	    if (unlikely(not StringUtil::EndsWith(ini_filename, ".ini")))
		Error("expected filename \"" + ini_filename + "\" to end in \".ini\"!");
	    std::string two_letter_code;
	    if (ini_filename.length() == 6)
		two_letter_code = ini_filename.substr(0, 2);
	    else {
		const std::string::size_type last_slash_pos(ini_filename.rfind('/'));
		if (unlikely(last_slash_pos == std::string::npos or (last_slash_pos + 6 + 1 != ini_filename.length())))
		    Error("INI filename does not match expected pattern: \"" + ini_filename + "\"!");
		two_letter_code = ini_filename.substr(last_slash_pos + 1, 2);
	    }

	    // Now map the international 2-letter code to a German 3-letter code:
	    const auto &two_letter_code_and_german_3letter_code(
                intl_2letter_code_to_german_3letter_code_map.find(two_letter_code));
	    if (unlikely(two_letter_code_and_german_3letter_code == intl_2letter_code_to_german_3letter_code_map.cend()))
		Error("don't know how to map the 2-letter code \"" + two_letter_code + "\" to a German 3-letter code!");

	    std::unordered_map<std::string, std::string> keys_to_other_map;
	    ReadIniFile(ini_filename, &keys_to_other_map);
	    InsertOther(&db_connection, two_letter_code_and_german_3letter_code->second, keys_to_german_map,
			keys_to_other_map);
	}
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}
