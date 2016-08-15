/** \file translation_db_tool.cc
 *  \brief A tool for reading/editing of the "translations" SQL table.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <set>
#include <cstdlib>
#include <cstring>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "IniFile.h"
#include "SimpleXmlParser.h"
#include "StringUtil.h"
#include "TranslationUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " command [args]\n\n";
    std::cerr << "       Possible commands are:\n";
    std::cerr << "       get_missing language_code\n";
    std::cerr << "       insert index language_code category text\n";
    std::exit(EXIT_FAILURE);
}


void ExecSqlOrDie(const std::string &select_statement, DbConnection * const connection) {
    if (unlikely(not connection->query(select_statement)))
        Error("SQL Statement failed: " + select_statement + " (" + connection->getLastErrorMessage() + ")");
}


// Replaces a comma with "\," and a slash with "\\".
std::string EscapeCommasAndBackslashes(const std::string &text) {
    std::string escaped_text;
    for (const auto ch : text) {
        if (unlikely(ch == ',' or ch == '\\'))
            escaped_text += '\\';
        escaped_text += ch;
    }

    return escaped_text;
}


void GetMissing(DbConnection * const connection, const std::string &language_code) {
    // Find an ID where "language_code" is missing:
    ExecSqlOrDie("SELECT id FROM translations WHERE id NOT IN (SELECT id FROM translations "
                 "WHERE language_code = \"" + language_code + "\") LIMIT 1", connection);
    DbResultSet id_result_set(connection->getLastResultSet());
    if (id_result_set.empty()) // The language code whose absence we're looking for exists for all ID's.!
        return;

    // Print the contents of all rows with the ID from the last query on stdout:
    const std::string matching_id(id_result_set.getNextRow()["id"]);
    ExecSqlOrDie("SELECT * FROM translations WHERE id=" + matching_id, connection);
    DbResultSet result_set(connection->getLastResultSet());
    if (not result_set.empty()) {
        const DbRow row = result_set.getNextRow();
        std::cout << row["id"] << ',' << row["language_code"] << ',' << EscapeCommasAndBackslashes(row["text"]) << ','
                  << row["category"] << '\n';
    }
}


void Insert(DbConnection * const connection, const unsigned index, const std::string &language_code,
            const std::string &category, const std::string &text)
{
    const std::string ID(std::to_string(index));
    if (category == "vufind_translations") {
        // First get the token that we need for the INSERT:
        const std::string SELECT_STMT("SELECT token FROM translations WHERE category='vufind_translations' AND id='"
                                      + ID + "'");
        ExecSqlOrDie(SELECT_STMT, connection);
        DbResultSet result_set(connection->getLastResultSet());
        if (result_set.empty())
            Error("Select unexpectedly returned an empty result set: " + SELECT_STMT);
        const DbRow row = result_set.getNextRow();
        const std::string token(row[0]);

        ExecSqlOrDie("INSERT INTO translations SET id=" + ID + ",language_code=\"" + language_code + "\",text=\""
                     + connection->escapeString(text) + "\", category='vufind_translations', token='" + token
                     + "', preexists=FALSE", connection);
    } else
        ExecSqlOrDie("INSERT INTO translations SET id=" + ID + ",language_code=\"" + language_code + "\",text=\""
                     + connection->escapeString(text) + "\", category='" + category + "', preexists=FALSE",
                     connection);
}


void GetPossibleCategories(DbConnection * const connection, std::set<std::string> * const categories) {
    categories->clear();

    ExecSqlOrDie("SELECT DISTINCT category FROM translations", connection);
    DbResultSet categories_result_set(connection->getLastResultSet());
    while (const DbRow row = categories_result_set.getNextRow())
        categories->insert(row[0]);
}


bool IsValid3LetterLanguageCode(const std::string &language_code_candidate) {
    static std::vector<std::string> VALID_LANGUAGE_CODES{
        "deu", "eng", "fra"
    };

    return std::find(VALID_LANGUAGE_CODES.cbegin(), VALID_LANGUAGE_CODES.cend(), language_code_candidate)
           != VALID_LANGUAGE_CODES.end();
}


const std::string CONF_FILE_PATH("/var/lib/tuelib/translations.conf");


int main(int argc, char *argv[]) {
    progname = argv[0];

    try {
        if (argc < 2)
            Usage();

        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("", "sql_database"));
        const std::string sql_username(ini_file.getString("", "sql_username"));
        const std::string sql_password(ini_file.getString("", "sql_password"));
        DbConnection db_connection("vufind", sql_username, sql_password);

        if (std::strcmp(argv[1], "get_missing") == 0) {
            if (argc != 3)
                Error("\"get_missing\" requires exactly one argument: language_code!");
            const std::string language_code(argv[2]);
            if (not IsValid3LetterLanguageCode(language_code))
                Error("\"" + language_code + "\" is not a valid 3-letter language code!");
            GetMissing(&db_connection, language_code);
        } else if (std::strcmp(argv[1], "insert") == 0) {
            if (argc != 6)
                Error("\"insert\" requires exactly four arguments: index, language_code, category, and text!");

            unsigned index;
            if (not StringUtil::ToUnsigned(argv[2], &index))
                Error("\"" + std::string(argv[2])  + "\" is not a valid index!");

            const std::string language_code(argv[3]);
            if (TranslationUtil::IsValidGerman3LetterCode(language_code))
                Error("\"" + language_code + "\" is not a valid German 3-letter language code!");

            std::set<std::string> valid_categories;
            GetPossibleCategories(&db_connection, &valid_categories);
            const std::string category(argv[4]);
            if (valid_categories.find(category) == valid_categories.cend())
                Error("\"" + category + "\" is not a valid category, valid categories are: "
                      + StringUtil::Join(valid_categories, ", ") + ".");

            Insert(&db_connection, index, language_code, category, argv[5]);
        } else
            Error("unknown command \"" + std::string(argv[1]) + "\"!");
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
