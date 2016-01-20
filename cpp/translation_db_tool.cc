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
    std::cerr << "       get_missing language_code\n";
    std::cerr << "       insert index language_code text\n";
    std::exit(EXIT_FAILURE);
}


void GetMissing(DbConnection * const connection, const std::string &language_code) {
    // Find an ID where "language_code" is missing:
    const std::string SELECT_ID_STMT("SELECT id FROM translations WHERE id NOT IN (SELECT id FROM translations "
				     "WHERE language_code = \"" + language_code + "\") LIMIT 1");
    if (unlikely(not connection->query(SELECT_ID_STMT)))
        Error("Select failed: " + SELECT_ID_STMT + " (" + connection->getLastErrorMessage() + ")");
    DbResultSet id_result_set(connection->getLastResultSet());
    if (id_result_set.empty()) // The language code whose absence we're looking for exists for all ID's.!
	return;

    // Print the contents of all rows with the ID from the last query on stdout:
    const std::string matching_id(id_result_set.getNextRow()["id"]);
    const std::string SELECT_MATCHING_ROW_STMT("SELECT * FROM translations WHERE id=" + matching_id);
    if (unlikely(not connection->query(SELECT_MATCHING_ROW_STMT)))
        Error("Select failed: " + SELECT_MATCHING_ROW_STMT + " (" + connection->getLastErrorMessage() + ")");
    DbResultSet result_set(connection->getLastResultSet());
    while (const DbRow row = result_set.getNextRow())
	std::cout << row["id"] << ',' << row["language_code"] << ',' << row["text"] << '\n';
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

	if (std::strcmp(argv[1], "get_missing") == 0) {
	    if (argc != 3)
		Error("\"get_missing\" requires exactly one argument: language_code!");
	    const std::string language_code(argv[2]);
	    if (language_code.length() != 3)
		Error("\"" + language_code + "\" is not a valid 3-letter language code!");
	    GetMissing(&db_connection, language_code);
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
