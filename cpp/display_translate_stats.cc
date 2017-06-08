/** \file    display_translate_stats.cc
 *  \brief   Generates a web page with simple translation stats.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016,2017, Library of the University of Tübingen

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
#include <vector>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "util.h"


const std::string CONF_FILE_PATH("/var/lib/tuelib/translations.conf");


void GetLanguageCodes(DbConnection * const db_connection, std::vector<std::string> * const language_codes) {
    db_connection->queryOrDie("SELECT DISTINCT language_code FROM vufind_translations;");
    DbResultSet result_set(db_connection->getLastResultSet());
    DbRow row;
    while (row = result_set.getNextRow())
        language_codes->emplace_back(row["language_code"]);
}


void GenerateStats(DbConnection * const db_connection, const std::vector<std::string> &language_codes,
                   const std::string &table_name, const std::string &table_key_name)
{
    for (const auto &language_code : language_codes) {
        db_connection->queryOrDie("SELECT COUNT(*) FROM " + table_name + " WHERE language_code='" + language_code
                                  + "';");
        DbResultSet result_set(db_connection->getLastResultSet());
        unsigned translated_count;
        if (result_set.empty())
            translated_count = 0;
        else {
            const DbRow row(result_set.getNextRow());
            translated_count = StringUtil::ToUnsigned(row["COUNT(*)"]);
        }

        // Find tokens/PPN's where "language_code" is missing:
        db_connection->queryOrDie("SELECT DISTINCT " + table_key_name + " FROM " + table_name + " WHERE "
                                  + table_key_name + " NOT IN (SELECT DISTINCT " + table_key_name + " FROM "
                                  + table_name + " WHERE language_code = \"" + language_code + "\");");
        const unsigned not_yet_translated(db_connection->getLastResultSet().size());

        std::cout << "        <tr>" << language_code << "</tr><tr>" << (translated_count + not_yet_translated)
                  << "</tr><tr>" << translated_count << "</tr>\n";
    }
}


int main(int /*argc*/, char *argv[]) {
    ::progname = argv[0];

    try {
        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("", "sql_database"));
        const std::string sql_username(ini_file.getString("", "sql_username"));
        const std::string sql_password(ini_file.getString("", "sql_password"));
        DbConnection db_connection(sql_database, sql_username, sql_password);

        std::vector<std::string> language_codes;
        GetLanguageCodes(&db_connection, &language_codes);

        std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";
        std::cout << "<html>\n";
        std::cout << "  <title>Translation Stats</table>\n";
        std::cout << "  <body>\n";
        std::cout << "    <h2>VuFind Interface Translations</h2>\n";
        std::cout << "    <table>\n";
        std::cout << "      <th>Language</th><th>Total count</th><th>Translated</th>>\n";
        GenerateStats(&db_connection, language_codes, "vufind_translations", "token");
        std::cout << "    </table>\n";
        std::cout << "    <h2>Keyword Interface Translations</h2>\n";
        std::cout << "    <table>\n";
        std::cout << "      <th>Language</th><th>Total count</th><th>Translated</th>>\n";
        GenerateStats(&db_connection, language_codes, "keyword_translations", "ppn");
        std::cout << "    </table>\n";
        std::cout << "  </body>\n";
        std::cout << "</html>\n";
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
