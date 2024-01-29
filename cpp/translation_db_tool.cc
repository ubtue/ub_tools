/** \file translation_db_tool.cc
 *  \brief A tool for reading/editing of the "translations" SQL table.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "HtmlUtil.h"
#include "IniFile.h"
#include "JSON.h"
#include "MiscUtil.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "TranslationUtil.h"
#include "UBTools.h"
#include "UrlUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " command [args]\n\n";
    std::cerr << "       Possible commands are:\n";
    std::cerr << "       get_missing language_code\n";
    std::cerr << "       insert token language_code text translator\n";
    std::cerr << "       insert ppn gnd_code language_code text translator\n";
    std::cerr << "       update token language_code text translator\n";
    std::cerr << "       update ppn gnd_code language_code text translator\n";
    std::cerr << "       disable_translation ppn true|false\n";
    std::exit(EXIT_FAILURE);
}


unsigned GetMissing(DbConnection * const connection, const std::string &table_name, const std::string &table_key_name,
                    const std::string &category, const std::string &language_code, const std::string &additional_condition = "") {
    // Find a token/ppn where "language_code" is missing:
    connection->queryOrDie("SELECT distinct " + table_key_name + " FROM " + table_name + " WHERE " + table_key_name
                           + " NOT IN (SELECT distinct " + table_key_name + " FROM " + table_name + " WHERE language_code = \""
                           + language_code + "\") " + (additional_condition.empty() ? "" : " AND (" + additional_condition + ")")
                           + " ORDER BY RAND();");
    DbResultSet keys_result_set(connection->getLastResultSet());
    if (keys_result_set.empty())
        return 0;

    // Print the contents of all rows with the token from the last query on stdout:
    DbRow row(keys_result_set.getNextRow());
    const std::string matching_key(row[table_key_name]);
    connection->queryOrDie("SELECT * FROM " + table_name + " WHERE " + table_key_name + "='" + matching_key + "';");
    DbResultSet result_set(connection->getLastResultSet());
    if (result_set.empty())
        return 0;

    const std::set<std::string> column_names(SqlUtil::GetColumnNames(connection, table_name));
    const bool has_gnd_code(column_names.find("gnd_code") != column_names.cend());

    while ((row = result_set.getNextRow()))
        std::cout << row[table_key_name] << ',' << keys_result_set.size() << ',' << row["language_code"] << ','
                  << HtmlUtil::HtmlEscape(row["translation"]) << ',' << category << (has_gnd_code ? "," + row["gnd_code"] : "") << '\n';

    return result_set.size();
}


unsigned GetMissingVuFindTranslations(DbConnection * const connection, const std::string &language_code) {
    return GetMissing(connection, "vufind_translations", "token", "vufind_translations", language_code);
}


unsigned GetMissingKeywordTranslations(DbConnection * const connection, const std::string &language_code) {
    return GetMissing(connection, "keyword_translations", "ppn", "keyword_translations", language_code,
                      "status != \"reliable_synonym\" AND status != \"unreliable_synonym\"");
}


unsigned GetExisting(DbConnection * const connection, const std::string &table_name, const std::string &table_key_name,
                     const std::string &category, const std::string &language_code, const std::string &index_value) {
    // Find a token/ppn where "language_code" is missing:
    connection->queryOrDie("SELECT distinct " + table_key_name + " FROM " + table_name + " WHERE " + table_key_name
                           + " NOT IN (SELECT distinct " + table_key_name + " FROM " + table_name + " WHERE language_code = \""
                           + language_code + "\") ORDER BY RAND();");
    DbResultSet keys_result_set(connection->getLastResultSet());
    const size_t count(keys_result_set.size());

    connection->queryOrDie("SELECT * FROM " + table_name + " WHERE " + table_key_name + "='" + index_value + "';");
    DbResultSet result_set(connection->getLastResultSet());
    if (result_set.empty())
        return 0;

    const std::set<std::string> column_names(SqlUtil::GetColumnNames(connection, table_name));
    const bool has_gnd_code(column_names.find("gnd_code") != column_names.cend());

    while (const DbRow row = result_set.getNextRow())
        std::cout << row[table_key_name] << ',' << count << ',' << row["language_code"] << ',' << HtmlUtil::HtmlEscape(row["translation"])
                  << ',' << category << (has_gnd_code ? "," + row["gnd_code"] : "") << '\n';

    return result_set.size();
}


unsigned GetExistingVuFindTranslations(DbConnection * const connection, const std::string &language_code, const std::string &index_value) {
    return GetExisting(connection, "vufind_translations", "token", "vufind_translations", language_code, index_value);
}


unsigned GetExistingKeywordTranslations(DbConnection * const connection, const std::string &language_code, const std::string &index_value) {
    return GetExisting(connection, "keyword_translations", "ppn", "keyword_translations", language_code, index_value);
}


unsigned GetTranslationHistory(DbConnection * const connection, const std::string &table_name, const std::string &index,
                               const std::string &language_code) {
    if (table_name == "vufind_translations") {
        connection->queryOrDie("SELECT create_timestamp, translator, translation FROM vufind_translations WHERE token='" + index
                               + "' AND language_code='" + language_code + "' ORDER BY create_timestamp DESC;");
    } else if (table_name == "keyword_translations") {
        connection->queryOrDie("SELECT create_timestamp, translator, translation FROM keyword_translations WHERE ppn='" + index
                               + "' AND language_code='" + language_code + "' ORDER BY create_timestamp DESC;");
    } else {
        LOG_ERROR("table_name must be either vufind_translations or keyword_translations");
    }
    DbResultSet result_set(connection->getLastResultSet());
    if (result_set.empty())
        return 0;

    std::cout << "{\"history_entries\":[" << std::endl;
    bool first_elem = true;
    while (const DbRow row = result_set.getNextRow()) {
        if (first_elem == true)
            first_elem = false;
        else
            std::cout << ",";
        std::cout << "{\"timestamp\":\"" << row["create_timestamp"] << "\","
                  << "\"translator\":\"" << row["translator"] << "\","
                  << "\"translation\":\"" << JSON::EscapeString(HtmlUtil::HtmlEscape(row["translation"])) << "\"}" << '\n';
    }
    std::cout << "]}" << std::endl;

    return result_set.size();
}


void UpdateIntoVuFindTranslations(DbConnection * const connection, const std::string &token, const std::string &language_code,
                                  const std::string &text, const std::string &translator) {
    connection->queryOrDie("CALL insert_vufind_translation_entry('" + token + "','" + language_code + "','" + connection->escapeString(text)
                           + "','" + translator + "');");
}


void UpdateIntoKeywordTranslations(DbConnection * const connection, const std::string &ppn, const std::string &gnd_code,
                                   const std::string &language_code, const std::string &text, const std::string &translator) {
    connection->queryOrDie("CALL insert_keyword_translation_entry('" + ppn + "','" + gnd_code + "','" + language_code + "','"
                           + connection->escapeString(text) + "','" + translator + "');");
}


void InsertIntoVuFindTranslations(DbConnection * const connection, const std::string &token, const std::string &language_code,
                                  const std::string &text, const std::string &translator) {
    DbTransaction transaction(connection);
    unsigned existing_translations_count(
        connection->countOrDie("SELECT COUNT(*) AS count FROM vufind_translations WHERE "
                               "token=\""
                                   + token + "\" AND language_code=\"" + language_code + "\"",
                               "count"));
    if (unlikely(existing_translations_count))
        UpdateIntoVuFindTranslations(connection, token, language_code, text, translator);
    else
        connection->queryOrDie("INSERT INTO vufind_translations SET token=\"" + token + "\",language_code=\"" + language_code
                               + "\",translation=\"" + connection->escapeString(text) + "\",translator=\"" + translator + "\";");
    transaction.commit();
}


void InsertIntoKeywordTranslations(DbConnection * const connection, const std::string &ppn, const std::string &gnd_code,
                                   const std::string &language_code, const std::string &text, const std::string &translator) {
    DbTransaction transaction(connection);
    unsigned existing_translations_count(connection->countOrDie("SELECT COUNT(*) AS count FROM keyword_translations WHERE ppn=\"" + ppn
                                                                    + "\" AND gnd_code=\"" + gnd_code + "\" AND language_code=\""
                                                                    + language_code + "\"",
                                                                "count"));
    if (unlikely(existing_translations_count))
        UpdateIntoKeywordTranslations(connection, ppn, gnd_code, language_code, text, translator);
    else
        connection->queryOrDie("INSERT INTO keyword_translations SET ppn=\"" + ppn + "\",gnd_code=\"" + gnd_code + "\",language_code=\""
                               + language_code + "\",translation=\"" + connection->escapeString(text) + "\",origin=\"150\",status=\"new\""
                               + ",translator=\"" + translator + "\";");
    transaction.commit();
}


void ValidateKeywordTranslation(DbConnection * const connection, const std::string &ppn, const std::string &translation) {
    const std::string query("SELECT translation FROM keyword_translations WHERE ppn = \"" + ppn + "\";");
    connection->queryOrDie(query);
    DbResultSet result_set(connection->getLastResultSet());

    while (const DbRow row = result_set.getNextRow()) {
        if (row["translation"].find("<") < row["translation"].find(">") and not(translation.find("<") < translation.find(">"))) {
            std::cout << "Your translation has to have a tag enclosed by '<' and '>'!";
            return;
        }
    }
}


void DisableTranslation(DbConnection * const connection, const std::string &ppn, const bool disable) {
    const std::string query("UPDATE keyword_translations SET translation_disabled='" + std::to_string(disable) + "' "
                            "WHERE ppn='" + ppn + "'");
    connection->queryOrDie(query);
}


const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "translations.conf");


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        if (argc < 2)
            Usage();

        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("Database", "sql_database"));
        const std::string sql_username(ini_file.getString("Database", "sql_username"));
        const std::string sql_password(ini_file.getString("Database", "sql_password"));
        DbConnection db_connection(DbConnection::MySQLFactory(sql_database, sql_username, sql_password));

        if (std::strcmp(argv[1], "get_missing") == 0) {
            if (argc != 3)
                logger->error("\"get_missing\" requires exactly one argument: language_code!");
            const std::string language_code(argv[2]);
            if (not TranslationUtil::IsValidFake3Or4LetterEnglishLanguagesCode(language_code))
                logger->error("\"" + language_code + "\" is not a valid fake 3- or 4-letter english language code!");
            if (not GetMissingVuFindTranslations(&db_connection, language_code))
                GetMissingKeywordTranslations(&db_connection, language_code);
        } else if (std::strcmp(argv[1], "disable_translation") == 0) {
            if (argc != 4)
                logger->error("\"disable_translation\" requires exactly two arguments: index disabled_flag!");
            const std::string index_value(argv[2]);
            const std::string disabled_flag(argv[3]);
            DisableTranslation(&db_connection, index_value, StringUtil::ASCIIToLower(disabled_flag) == "true" ? true : false);
        } else if (std::strcmp(argv[1], "get_existing") == 0) {
            if (argc != 5)
                logger->error("\"get_existing\" requires exactly three arguments: language_code category index!");
            const std::string language_code(argv[2]);
            if (not TranslationUtil::IsValidFake3Or4LetterEnglishLanguagesCode(language_code))
                logger->error("\"" + language_code + "\" is not a valid fake 3- or 4-letter english language code!");
            const std::string category(argv[3]);
            const std::string index_value(argv[4]);
            if (category == "vufind_translations")
                GetExistingVuFindTranslations(&db_connection, language_code, index_value);
            else
                GetExistingKeywordTranslations(&db_connection, language_code, index_value);
        } else if (std::strcmp(argv[1], "insert") == 0) {
            if (argc != 6 and argc != 7)
                logger->error(
                    "\"insert\" requires four or five arguments: token or ppn, gnd_code (if ppn), "
                    "language_code, text, and translator!");

            const std::string language_code(argv[(argc == 6) ? 3 : 4]);
            if (not TranslationUtil::IsValidFake3Or4LetterEnglishLanguagesCode(language_code))
                logger->error("\"" + language_code + "\" is not a valid fake 3- or 4-letter english language code!");

            if (argc == 6)
                InsertIntoVuFindTranslations(&db_connection, argv[2], language_code, argv[4], argv[5]);
            else
                InsertIntoKeywordTranslations(&db_connection, argv[2], argv[3], language_code, argv[5], argv[6]);
        } else if (std::strcmp(argv[1], "update") == 0) {
            if (argc != 6 and argc != 7)
                logger->error(
                    "\"update\" requires four or five arguments: token or ppn, gnd_code (if ppn), "
                    "language_code, text and translator!");

            const std::string language_code(argv[(argc == 6) ? 3 : 4]);
            if (not TranslationUtil::IsValidFake3Or4LetterEnglishLanguagesCode(language_code))
                logger->error("\"" + language_code + "\" is not a valid fake 3-letter english language code!");

            if (argc == 6)
                UpdateIntoVuFindTranslations(&db_connection, argv[2], language_code, argv[4], argv[5]);
            else
                UpdateIntoKeywordTranslations(&db_connection, argv[2], argv[3], language_code, argv[5], argv[6]);
        } else if (std::strcmp(argv[1], "get_history_for_entry") == 0) {
            if (argc != 5)
                logger->error("\"get_history_for_entry\" requires exactly three arguments: table_name ppn language_code!");
            const std::string table_name(argv[2]);
            const std::string index(argv[3]);
            const std::string language_code(argv[4]);
            GetTranslationHistory(&db_connection, table_name, index, language_code);
        } else if (std::strcmp(argv[1], "validate_keyword") == 0) {
            if (argc != 4)
                logger->error("\"get_missing\" requires exactly two arguments: ppn translation!");
            const std::string ppn(argv[2]);
            const std::string translation(argv[3]);
            ValidateKeywordTranslation(&db_connection, ppn, translation);
        } else
            logger->error("unknown command \"" + std::string(argv[1]) + "\"!");
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()) + " (login is " + MiscUtil::GetUserName() + ")");
    }
}
