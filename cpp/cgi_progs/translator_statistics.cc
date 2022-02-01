/** \file     translator_statistics.cc
 *  \brief    A CGI-tool for showing translator statistics.
  *  \author  andreas-ub
 */
/*
    Copyright (C) 2016-2021, Library of the University of Tübingen

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
#include <set>
#include <sstream>
#include <string>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "EmailSender.h"
#include "HtmlUtil.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "Template.h"
#include "UBTools.h"
#include "UrlUtil.h"
#include "util.h"
#include "WebUtil.h"


namespace {


const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "translations.conf");
enum Category { VUFIND, KEYWORDS };


DbResultSet ExecSqlAndReturnResultsOrDie(const std::string &select_statement, DbConnection * const db_connection) {
    db_connection->queryOrDie(select_statement);
    return db_connection->getLastResultSet();
}


void GetVuFindStatisticsAsHTMLRowsFromDatabase(DbConnection &db_connection, std::vector<std::string> * const rows, const std::string &start_date, const std::string &end_date)
{
    unsigned total_number;
    std::string query_total_number("SELECT COUNT(DISTINCT(token)) AS total_number FROM vufind_translations;");
    DbResultSet result_set_total_number(ExecSqlAndReturnResultsOrDie(query_total_number, &db_connection));
    while (const auto db_row = result_set_total_number.getNextRow())
        total_number = std::stoi(db_row["total_number"]);

    std::map<std::string, unsigned> map_translated;
    std::string query_translated("SELECT language_code,COUNT(DISTINCT(token)) AS untranslated_number FROM vufind_translations GROUP BY language_code;");
    DbResultSet result_set_translated(ExecSqlAndReturnResultsOrDie(query_translated, &db_connection));
    while (const auto db_row = result_set_translated.getNextRow())
        map_translated.emplace(db_row["language_code"], std::stoi(db_row["untranslated_number"]));

    rows->clear();

    std::string query("SELECT language_code,COUNT(distinct token) AS number FROM vufind_translations WHERE next_version_id IS NULL AND prev_version_id IS NOT NULL AND create_timestamp >= '" + start_date + "' AND create_timestamp <= '" + end_date + " 23:59:59" + "' GROUP BY language_code;");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(query, &db_connection));
    while (const auto db_row = result_set.getNextRow()) {
       std::string language_code(db_row["language_code"]);
       std::string recently_number(db_row["number"]);
       std::string untranslated_number = "n.a.";
       auto elem = map_translated.find(language_code);
       if (elem != map_translated.end())
           untranslated_number = std::to_string(total_number - elem->second);
       rows->emplace_back("<tr><td>" + language_code + "</td><td>" + recently_number + "</td><td>" + untranslated_number + "</td></tr>");
    }
}


void GetKeyWordStatisticsAsHTMLRowsFromDatabase(DbConnection &db_connection, std::vector<std::string> * const rows, const std::string &start_date, const std::string &end_date)
{
    unsigned total_number;
    std::string query_total_number("SELECT COUNT(DISTINCT(ppn)) AS total_number FROM keyword_translations;");
    DbResultSet result_set_total_number(ExecSqlAndReturnResultsOrDie(query_total_number, &db_connection));
    while (const auto db_row = result_set_total_number.getNextRow())
        total_number = std::stoi(db_row["total_number"]);

    std::map<std::string, unsigned> map_translated;
    std::string query_translated("SELECT language_code,COUNT(DISTINCT(ppn)) AS untranslated_number FROM keyword_translations GROUP BY language_code;");
    DbResultSet result_set_translated(ExecSqlAndReturnResultsOrDie(query_translated, &db_connection));
    while (const auto db_row = result_set_translated.getNextRow())
        map_translated.emplace(db_row["language_code"], std::stoi(db_row["untranslated_number"]));

    rows->clear();

    std::string query("SELECT language_code,COUNT(distinct ppn) AS number FROM keyword_translations WHERE next_version_id IS NULL AND prev_version_id IS NOT NULL AND create_timestamp >= '" + start_date + "' AND create_timestamp <= '" + end_date + " 23:59:59" + "' GROUP BY language_code;");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(query, &db_connection));

    while (const auto db_row = result_set.getNextRow()) {
       std::string language_code(db_row["language_code"]);
       std::string recently_number(db_row["number"]);
       std::string untranslated_number = "n.a.";
       auto elem = map_translated.find(language_code);
       if (elem != map_translated.end())
           untranslated_number = std::to_string(total_number - elem->second);
       rows->emplace_back("<tr><td>" + language_code + "</td><td>" + recently_number + "</td><td>" + untranslated_number + "</td></tr>");
    }
}


void GetVuFindStatisticsNewEntriesFromDatabase(DbConnection &db_connection, std::string * const number_new_entries, const std::string &start_date, const std::string &end_date)
{
    std::string query("SELECT token FROM vufind_translations WHERE create_timestamp>='" + start_date + "' AND create_timestamp<='" + end_date + " 23:59:59" + "' AND token IN (SELECT token FROM vufind_translations GROUP BY token HAVING COUNT(*)=1);");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(query, &db_connection));
    number_new_entries->clear();
    unsigned counter(0);
    while (const auto db_row = result_set.getNextRow()) { //could use result_set.size(), but prob. more information requested in web gui
       std::string language_code(db_row["token"]);
       ++counter;
    }
    (*number_new_entries) = std::to_string(counter);
}


void GetKeyWordStatisticsNewEntriesFromDatabase(DbConnection &db_connection, std::string * const number_new_entries, const std::string &start_date, const std::string &end_date)
{
    std::string query("SELECT ppn FROM keyword_translations WHERE create_timestamp>='" + start_date + "' AND create_timestamp<='" + end_date + " 23:59:59" + "' AND ppn IN (SELECT ppn FROM keyword_translations GROUP BY ppn HAVING COUNT(*)=1);");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(query, &db_connection));
    number_new_entries->clear();
    unsigned counter(0);
    while (const auto db_row = result_set.getNextRow()) { //could use result_set.size(), but prob. more information requested in web gui
       std::string language_code(db_row["ppn"]);
       ++counter;
    }
    (*number_new_entries) = std::to_string(counter);
}


void ShowFrontPage(DbConnection &db_connection, const std::string &target, const std::string &start_date, const std::string &end_date) {
    Template::Map names_to_values_map;
    std::vector<std::string> vufind_rows;
    std::vector<std::string> keyword_rows;
    std::string number_new_entries_vufind, number_new_entries_keyword;
    
    GetVuFindStatisticsAsHTMLRowsFromDatabase(db_connection, &vufind_rows, start_date, end_date);
    GetVuFindStatisticsNewEntriesFromDatabase(db_connection, &number_new_entries_vufind, start_date, end_date);
    GetKeyWordStatisticsAsHTMLRowsFromDatabase(db_connection, &keyword_rows, start_date, end_date);
    GetKeyWordStatisticsNewEntriesFromDatabase(db_connection, &number_new_entries_keyword, start_date, end_date);
    
    names_to_values_map.insertArray("vufind_rows", vufind_rows);
    names_to_values_map.insertArray("keyword_rows", keyword_rows);
    names_to_values_map.insertScalar("target_translation_scope", target);
    names_to_values_map.insertScalar("number_new_entries_vufind", number_new_entries_vufind);
    names_to_values_map.insertScalar("number_new_entries_keyword", number_new_entries_keyword);
    names_to_values_map.insertScalar("start_date", start_date);
    names_to_values_map.insertScalar("end_date", end_date);

    std::ifstream translator_statistics_html(UBTools::GetTuelibPath() + "translate_chainer/translator_statistics.html", std::ios::binary);
    Template::ExpandTemplate(translator_statistics_html, std::cout, names_to_values_map);
}


// \return the current day as a range endpoint
inline std::string Now(unsigned negative_month_offset=0) {
    unsigned year, month, day;
    TimeUtil::GetCurrentDate(&year, &month, &day);
    if (negative_month_offset > 0) {
        int signed_month = month - (negative_month_offset % 12);
        if (signed_month < 1) {
            year = year - ((negative_month_offset / 12) + 1);
            month = (signed_month + 12);
        }
    }
    return StringUtil::ToString(year, /* radix = */10, /* width = */4, /* padding_char = */'0')
           + "-" + StringUtil::ToString(month, /* radix = */10, /* width = */2, /* padding_char = */'0')
           + "-" + StringUtil::ToString(day, /* radix = */10, /* width = */2, /* padding_char = */'0');
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    std::multimap<std::string, std::string> cgi_args;
    WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);

    const IniFile ini_file(CONF_FILE_PATH);
    const std::string sql_database(ini_file.getString("Database", "sql_database"));
    const std::string sql_username(ini_file.getString("Database", "sql_username"));
    const std::string sql_password(ini_file.getString("Database", "sql_password"));
    DbConnection db_connection(DbConnection::MySQLFactory(sql_database, sql_username, sql_password));

    const std::string translation_target(WebUtil::GetCGIParameterOrDefault(cgi_args, "target", "keywords"));
    
    std::string start_date(WebUtil::GetCGIParameterOrDefault(cgi_args, "start_date", Now(6)));
    if (start_date.empty())
        start_date = Now(6);
    std::string end_date(WebUtil::GetCGIParameterOrDefault(cgi_args, "end_date", Now()));
    if (end_date.empty())
        end_date = Now();

    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";

    ShowFrontPage(db_connection, translation_target, start_date, end_date);
    return EXIT_SUCCESS;
}
