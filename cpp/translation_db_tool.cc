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
#include "MiscUtil.h"
#include "SimpleXmlParser.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "TranslationUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " command [args]\n\n";
    std::cerr << "       Possible commands are:\n";
    std::cerr << "       get_missing language_code\n";
    std::cerr << "       insert token language_code text\n";
    std::cerr << "       insert ppn gnd_code language_code text\n";
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


unsigned GetMissing(DbConnection * const connection, const std::string &table_name, 
                    const std::string &table_key_name, const std::string &category,
                    const std::string &language_code) 
{
     // Find a token/ppn where "language_code" is missing:
    ExecSqlOrDie("SELECT distinct " + table_key_name + " FROM " + table_name + " WHERE " + table_key_name + 
                 " NOT IN (SELECT distinct " + table_key_name + " FROM " + table_name + 
                 " WHERE language_code = \"" + language_code + "\") ORDER BY RAND();", connection);
    DbResultSet keys_result_set(connection->getLastResultSet());
    const size_t count(keys_result_set.size());
    if (count == 0)
        return 0;

    // Print the contents of all rows with the token from the last query on stdout:
    DbRow row(keys_result_set.getNextRow());
    const std::string matching_key(row[table_key_name]);
    ExecSqlOrDie("SELECT * FROM " + table_name + " WHERE " + table_key_name + "='" + matching_key + "';", connection);
    DbResultSet result_set(connection->getLastResultSet());
    if (result_set.empty())
        return 0;

    const std::set<std::string> column_names(SqlUtil::GetColumnNames(connection, table_name));
    const bool has_gnd_code(column_names.find("gnd_code") != column_names.cend());
    
    while (row = result_set.getNextRow())
        std::cout << row[table_key_name] << ',' << count << ',' << row["language_code"] << ','
                  << EscapeCommasAndBackslashes(row["translation"]) << ',' << category
                  << (has_gnd_code ? "," + row["gnd_code"] : "") << '\n';

    return result_set.size();
}


unsigned GetMissingVuFindTranslations(DbConnection * const connection, const std::string &language_code) {
    return GetMissing(connection, "vufind_translations", "token", "vufind_translations", language_code);
}


unsigned GetMissingKeywordTranslations(DbConnection * const connection, const std::string &language_code) {
    return GetMissing(connection, "keyword_translations", "ppn", "keywords", language_code);
}


void InsertIntoVuFindTranslations(DbConnection * const connection, const std::string &token,
                                  const std::string &language_code, const std::string &text)
{
    ExecSqlOrDie("INSERT INTO vufind_translations SET token=\"" + token + "\",language_code=\"" + language_code
                 + "\",translation=\"" + connection->escapeString(text) + "\";", connection);
}


void InsertIntoKeywordTranslations(DbConnection * const connection, const std::string &ppn,
                                   const std::string &gnd_code, const std::string &language_code,
                                   const std::string &text)
{
    ExecSqlOrDie("INSERT INTO keyword_translations SET ppn=\"" + ppn + ",gnd_code=\"" + gnd_code
                 + "\",language_code=\"" + language_code + "\", gnd_code=\"" + gnd_code + "\",translation=\""
                 + connection->escapeString(text) + "\";", connection);
}


const std::string CONF_FILE_PATH("/var/lib/tuelib/translations.conf");


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        if (argc < 2)
            Usage();

        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("", "sql_database"));
        const std::string sql_username(ini_file.getString("", "sql_username"));
        const std::string sql_password(ini_file.getString("", "sql_password"));
        DbConnection db_connection(sql_database, sql_username, sql_password);

        if (std::strcmp(argv[1], "get_missing") == 0) {
            if (argc != 3)
                Error("\"get_missing\" requires exactly one argument: language_code!");
            const std::string language_code(argv[2]);
            if (not TranslationUtil::IsValidGerman3LetterCode(language_code))
                Error("\"" + language_code + "\" is not a valid 3-letter language code!");
            if (not GetMissingVuFindTranslations(&db_connection, language_code))
                GetMissingKeywordTranslations(&db_connection, language_code);
        } else if (std::strcmp(argv[1], "insert") == 0) {
            if (argc != 5 and argc != 6)
                Error("\"insert\" requires three or four arguments: token or ppn, gnd_code (if ppn), "
                      "language_code, and text!");

            const std::string language_code(argv[(argc == 6) ? 3 : 4]);
            if (not TranslationUtil::IsValidGerman3LetterCode(language_code))
                Error("\"" + language_code + "\" is not a valid German 3-letter language code!");

            if (argc == 5)
                InsertIntoVuFindTranslations(&db_connection, argv[2], language_code, argv[4]);
            else
                InsertIntoKeywordTranslations(&db_connection, argv[2], argv[3], language_code, argv[5]);
        } else
            Error("unknown command \"" + std::string(argv[1]) + "\"!");
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()) + " (login is " + MiscUtil::GetUserName() + ")");
    }
}
