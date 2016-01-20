/** \file translation_db_tool.cc
 *  \brief A tool for reading/editing of the translation SQL table.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <map>
#include <cstdlib>
#include <cstring>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "IniFile.h"
#include "SimpleXmlParser.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " command [args]\n\n";
    std::cerr << "       Possible commands are:\n";
    std::cerr << "       get_next start_index language_code\n";
    std::cerr << "       insert index language_code text\n";
    std::exit(EXIT_FAILURE);
}


void GetNext(DbConnection * const connection, const unsigned start_index, const std::string &language_code) {
    const std::string SELECT_STMT("SELECT * FROM translations WHERE index > " + std::to_string(start_index));
    if (unlikely(not connection->query(SELECT_STMT)))
        Error("Select failed: " + SELECT_STMT + " (" + connection->getLastErrorMessage() + ")");
    DbResultSet result_set(connection->getLastResultSet());
    unsigned current_index(0);
    std::map<std::string, std::string> language_code_to_text_map;
    while (const DbRow row = result_set.getNextRow()) {
	const unsigned index(StringUtil::ToUnsigned(row["id"]));
	if (index == current_index)
	    language_code_to_text_map[row["language_code"]] = row["text"];
	else if (language_code_to_text_map.find(language_code) == language_code_to_text_map.cend()) {
	    for (const auto &language_code_and_text : language_code_to_text_map)
		std::cout << language_code_and_text.first << '=' << language_code_and_text.second << '\n';
	    return;
	} else {
	    current_index = index;
	    language_code_to_text_map.clear();
	}
    }
    if (language_code_to_text_map.find(language_code) == language_code_to_text_map.cend()) {
	for (const auto &language_code_and_text : language_code_to_text_map)
	    std::cout << language_code_and_text.first << '=' << language_code_and_text.second << '\n';
    }
}


void Insert(DbConnection * const connection, const unsigned index, const std::string &language_code, const std::string &text) {
    const std::string INSERT_STMT("INSERT INTO translations SET id=" + std::to_string(index) + ",language_code=\""
				  + language_code + "\",text=\"" + connection->escapeString(text) + "\"");
    if (unlikely(not connection->query(INSERT_STMT)))
	Error("Insert failed: " + INSERT_STMT + " (" + connection->getLastErrorMessage() + ")");
}


const std::string CONF_FILE_PATH("/var/lib/tuelib/translation_tool.conf");


int main(int argc, char *argv[]) {
    progname = argv[0];


    try {
	if (argc < 2)
	    Usage();

	const IniFile ini_file(CONF_FILE_PATH);
	const std::string sql_username(ini_file.getString("", "sql_username"));
	const std::string sql_password(ini_file.getString("", "sql_password"));
	DbConnection db_connection("vufind", sql_username, sql_password);

	if (std::strcmp(argv[1], "get_next") == 0) {
	    if (argc != 4)
		Error("\"get_next\" requires exactly two arguments: start_index and language_code!");
	    unsigned start_index;
	    if (not StringUtil::ToUnsigned(argv[2], &start_index))
		Error("\"" + std::string(argv[2])  + "\" is not a valid start index!");
	    const std::string language_code(argv[3]);
	    if (language_code.length() != 3)
		Error("\"" + language_code + "\" is not a valid 3-letter language code!");
	    GetNext(&db_connection, start_index, language_code);
	} else if (std::strcmp(argv[1], "insert") == 0) {
	    if (argc != 5)
		Error("\"insert\" requires exactly three arguments: index, language_code, and text!");
	    unsigned index;
	    if (not StringUtil::ToUnsigned(argv[2], &index))
		Error("\"" + std::string(argv[2])  + "\" is not a valid index!");
	    const std::string language_code(argv[3]);
	    if (language_code.length() != 3)
		Error("\"" + language_code + "\" is not a valid 3-letter language code!");
	    Insert(&db_connection, index, language_code, argv[4]);
	} else
	    Error("unknown command \"" + std::string(argv[1]) + "\"!");
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}
