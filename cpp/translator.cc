/** \file    translator.cc
 *  \brief   A CGI-tool for translating vufind tokens and keywords.
 *  \author  Oliver Obenland (oliver.obenland@uni-tuebingen.de)
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
#include <string>
#include <set>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "HtmlUtil.h"
#include "IniFile.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "util.h"
#include "WebUtil.h"


const int ENTRIES_PER_PAGE(20);
const std::string NO_GND_CODE("-1");


DbResultSet ExecSqlOrDie(const std::string &select_statement, DbConnection &db_connection) {
    if (unlikely(not db_connection.query(select_statement)))
        Error("SQL Statement failed: " + select_statement + " (" + db_connection.getLastErrorMessage() + ")");
    return db_connection.getLastResultSet();
}


std::vector<std::string> GetLanguageCodesFromTable(DbConnection &db_connection, const std::string &table_name) {
    const std::string query("SELECT DISTINCT language_code from " + table_name + " ORDER BY language_code;");
    DbResultSet result_set(ExecSqlOrDie(query, db_connection));
    std::vector <std::string> language_codes;
    while (const DbRow db_row = result_set.getNextRow()) {
        language_codes.emplace_back(db_row["language_code"]);
    }
    return language_codes;
}


std::vector<std::string> GetLanguageCodes(DbConnection &db_connection) {
    std::vector <std::string> language_codes(GetLanguageCodesFromTable(db_connection, "vufind_translations"));
    for (auto &language_code : GetLanguageCodesFromTable(db_connection, "keyword_translations")) {
        if (std::find(language_codes.begin(), language_codes.end(), language_code) == language_codes.end())
            language_codes.emplace_back(language_code);
    }
    return language_codes;
}


std::string GetCGIParameterOrDefault(const std::multimap<std::string, std::string> &cgi_args,
                                     const std::string &parameter_name,
                                     const std::string &default_value) {
    const auto key_and_value(cgi_args.find(parameter_name));
    if (key_and_value == cgi_args.cend())
        return default_value;

    return key_and_value->second;
}


std::string CreateRowEntry(const std::string &token, const std::string &label, const std::string language_code,
                           const std::string &category, const std::string &status = "UNKNOWN") {
    if (status == "reliable")
        return "<td>" + HtmlUtil::HtmlEscape(label) + "</td>";
    return "<td><a href='/cgi-bin/translate_chainer?category=" + UrlUtil::UrlEncode(category) + "&index="
           + UrlUtil::UrlEncode(token) + "&language_code=" + UrlUtil::UrlEncode(language_code) + "'>"
           + HtmlUtil::HtmlEscape(label) + "</a></td>";
}


void GetVuFindTranslationsAsHTMLRowsFromDatabase(DbConnection &db_connection, const std::string &lookfor,
                                                 const std::string &offset, std::vector<std::string> *const rows,
                                                 std::string *const headline) {
    rows->clear();

    const std::string token_where_clause(
            lookfor.empty() ? "" : "WHERE token LIKE '%" + lookfor + "%' OR translation LIKE '%" + lookfor + "%'");
    const std::string token_query(
            "SELECT token FROM vufind_translations " + token_where_clause + " ORDER BY token LIMIT " + offset + ", " +
            std::to_string(ENTRIES_PER_PAGE));
    const std::string query("SELECT token, translation, language_code FROM vufind_translations "
                            "WHERE token IN (SELECT * FROM (" + token_query + ") as t) ORDER BY token, language_code");

    DbResultSet result_set(ExecSqlOrDie(query, db_connection));

    std::vector<std::string> language_codes(GetLanguageCodes(db_connection));
    *headline = "<th>Token</th><th>" + StringUtil::Join(language_codes, "</th><th>") + "</th>";
    if (result_set.empty())
        return;

    DbRow db_row(result_set.getNextRow());
    std::string current_token(db_row["token"]);
    std::vector<std::string> row_values(language_codes.size(), "<td></td>");
    do {
        std::string token(db_row["token"]);
        if (token != current_token) {
            rows->emplace_back("<td>" + HtmlUtil::HtmlEscape(current_token) + "</td>" + StringUtil::Join(row_values, ""));
            current_token = token;
            row_values.clear();
            row_values.resize(language_codes.size(), "<td></td>");
        }
        const auto index(std::find(language_codes.begin(), language_codes.end(), db_row["language_code"]) -
                     language_codes.begin());
        row_values[index] = CreateRowEntry(current_token, db_row["translation"], db_row["language_code"],
                                           "vufind_translations");
    } while (db_row = result_set.getNextRow());
    rows->emplace_back("<td>" + HtmlUtil::HtmlEscape(current_token) + "</td>" + StringUtil::Join(row_values, ""));
}


void GetKeyWordTranslationsAsHTMLRowsFromDatabase(DbConnection &db_connection, const std::string &lookfor,
                                                  const std::string &offset, std::vector<std::string> *const rows,
                                                  std::string *const headline) {
    rows->clear();

    const std::string ppn_where_clause(lookfor.empty() ? "" : "WHERE translation LIKE '%" + lookfor + "%'");
    const std::string ppn_query("SELECT ppn FROM keyword_translations " + ppn_where_clause + " ORDER BY translation LIMIT " + offset + ", " + std::to_string(ENTRIES_PER_PAGE) );
    const std::string query("SELECT ppn, translation, language_code, status FROM keyword_translations "
                            "WHERE  ppn IN (SELECT ppn FROM (" + ppn_query + ") as t) AND status != \"reliable_synonym\" AND status != \"unreliable_synonym\" ORDER BY ppn, translation;");
    DbResultSet result_set(ExecSqlOrDie(query, db_connection));

    std::vector<std::string> language_codes(GetLanguageCodes(db_connection));
    *headline = "<th>" + StringUtil::Join(language_codes, "</th><th>") + "</th>";
    if (result_set.empty())
        return;

    DbRow db_row(result_set.getNextRow());
    std::string current_ppn(db_row["ppn"]);
    std::vector<std::string> row_values(language_codes.size(), "<td></td>");
    do {
        std::string ppn(db_row["ppn"]);
        if (ppn != current_ppn) {
            rows->emplace_back(StringUtil::Join(row_values, ""));
            current_ppn = ppn;
            row_values.clear();
            row_values.resize(language_codes.size(), "<td></td>");
        }
        auto index = std::find(language_codes.begin(), language_codes.end(), db_row["language_code"]) -
                     language_codes.begin();
        row_values[index] = CreateRowEntry(current_ppn, db_row["translation"], db_row["language_code"],
                                           "keyword_translations", db_row["status"]);
    } while (db_row = result_set.getNextRow());
    rows->emplace_back(StringUtil::Join(row_values, ""));
}


void ShowFrontPage(DbConnection &db_connection, const std::string &lookfor, const std::string &offset) {
    std::map<std::string, std::vector<std::string>> names_to_values_map;
    std::vector<std::string> rows;
    std::string headline;
    GetVuFindTranslationsAsHTMLRowsFromDatabase(db_connection, lookfor, offset, &rows, &headline);
    names_to_values_map.emplace("keyword_row", rows);
    names_to_values_map.emplace("keyword_table_headline", std::vector<std::string> {headline});


    GetKeyWordTranslationsAsHTMLRowsFromDatabase(db_connection, lookfor, offset, &rows, &headline);
    names_to_values_map.emplace("vufind_token_row", rows);
    names_to_values_map.emplace("vufind_token_table_headline", std::vector<std::string> {headline});

    names_to_values_map.emplace("lookfor", std::vector<std::string> {lookfor});
    names_to_values_map.emplace("prev_offset", std::vector<std::string>
                                               {std::to_string(std::max(0, std::stoi(offset) - ENTRIES_PER_PAGE))});
    names_to_values_map.emplace("next_offset",
                                std::vector<std::string> {std::to_string(std::stoi(offset) + ENTRIES_PER_PAGE)});

    names_to_values_map.emplace("target_language_code", std::vector<std::string> {""});

    std::ifstream translate_html("/var/lib/tuelib/translate_chainer/translation_front_page.html", std::ios::binary);
    MiscUtil::ExpandTemplate(translate_html, std::cout, names_to_values_map);
}


const std::string CONF_FILE_PATH("/var/lib/tuelib/translations.conf");


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        std::multimap<std::string, std::string> cgi_args;
        WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);

        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("", "sql_database"));
        const std::string sql_username(ini_file.getString("", "sql_username"));
        const std::string sql_password(ini_file.getString("", "sql_password"));
        DbConnection db_connection(sql_database, sql_username, sql_password);

        std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";

        const std::string lookfor(GetCGIParameterOrDefault(cgi_args, "lookfor", ""));
        const std::string offset(GetCGIParameterOrDefault(cgi_args, "offset", "0"));
        ShowFrontPage(db_connection, lookfor, offset);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
