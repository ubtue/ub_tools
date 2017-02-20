/** \file    translator.cc
 *  \brief   A CGI-tool for translating vufind tokens and keywords.
 *  \author  Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author  Johannes Riedl
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

#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <boost/bind.hpp>
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


const int ENTRIES_PER_PAGE(30);
const std::string NO_GND_CODE("-1");
const std::string LANGUAGES_SECTION("Languages");
const std::string TRANSLATION_LANGUAGES_SECTION("TranslationLanguages");
const std::string ADDITIONAL_VIEW_LANGUAGES("AdditionalViewLanguages");
const std::string USER_SECTION("Users");
const std::string ALL_SUPPORTED_LANGUAGES("all");
const std::string SYNONYM_COLUMN_DESCRIPTOR("syn");
const int NO_INDEX(-1);



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


void ShowErrorPage(const std::string &title, const std::string &error_message, const std::string &description = "") {
    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";
    std::cout << "<!DOCTYPE html><html><head><title>" + title + "</title></head>"
              << "<body>"
              << "  <h1>" + error_message + "</h1>"
              << "  <h3>" + description + "</h3>"
              << "</body>"
              << "</html>";
}



std::string GetCGIParameterOrDefault(const std::multimap<std::string, std::string> &cgi_args,
                                     const std::string &parameter_name,
                                     const std::string &default_value) {
    const auto key_and_value(cgi_args.find(parameter_name));
    if (key_and_value == cgi_args.cend())
        return default_value;

    return key_and_value->second;
}


const std::string GetTranslatorOrEmptyString() {
    return (std::getenv("REMOTE_USER") != nullptr) ? std::getenv("REMOTE_USER") : "";
}


const std::string AssembleTermIdentifiers(const std::string &category, const std::string &index, 
                                          const std::string &language_code, const std::string &gnd_code = "", const std::string &translation = "") {

       return std::string(" category=\"" +  UrlUtil::UrlEncode(category) + "\" index=\"" + UrlUtil::UrlEncode(index) + "\" language_code=\"" +
                          UrlUtil::UrlEncode(language_code) + "\" gnd_code=\"" + gnd_code + "\" translation=\"" + translation + "\" ");
}


std::string CreateEditableRowEntry(const std::string &token, const std::string &label, const std::string language_code,
                           const std::string &category, const std::string db_translator, 
                           const std::string &status = "UNKNOWN", const std::string &gnd_code = "") {
    (void)status;
    std::string term_identifiers(AssembleTermIdentifiers(category, token, language_code, gnd_code, label));
    std::string background_color((GetTranslatorOrEmptyString() == db_translator) ? "RoyalBlue" : "LightBlue");
    return  "<td contenteditable=\"true\" class=\"editable_translation\"" + term_identifiers + "style=\"background-color:" + background_color + "\">" 
                + HtmlUtil::HtmlEscape(label) +"</td>";
}


void GetVuFindTranslationsAsHTMLRowsFromDatabase(DbConnection &db_connection, const std::string &lookfor,
                                                 const std::string &offset, std::vector<std::string> *const rows,
                                                 std::string *const headline, const std::vector<std::string> translator_languages) {
    (void)translator_languages;
    rows->clear();

    const std::string token_where_clause(
            lookfor.empty() ? "" : "WHERE token LIKE '%" + lookfor + "%' OR translation LIKE '%" + lookfor + "%'");
    const std::string token_query(
            "SELECT token FROM vufind_translations " + token_where_clause + " ORDER BY token LIMIT " + offset + ", " +
            std::to_string(ENTRIES_PER_PAGE));
    const std::string query("SELECT token, translation, language_code, translator FROM vufind_translations "
                            "WHERE token IN (SELECT * FROM (" + token_query + ") as t) ORDER BY token, language_code");

    DbResultSet result_set(ExecSqlOrDie(query, db_connection));

    std::vector<std::string> language_codes(GetLanguageCodes(db_connection));
    *headline = "<th>Token</th><th>" + StringUtil::Join(language_codes, "</th><th>") + "</th>";
    if (result_set.empty())
        return;

    DbRow db_row(result_set.getNextRow());
    std::string current_token(db_row["token"]);
    std::vector<std::string> row_values(language_codes.size(), "<td contenteditable=\"true\" class=\"editable_translation\"></td>");
    do {
        std::string token(db_row["token"]);
        if (token != current_token) {
            rows->emplace_back("<td contenteditable=\"true\">" + HtmlUtil::HtmlEscape(current_token) + "</td>" + StringUtil::Join(row_values, ""));
            current_token = token;
            row_values.clear();
            row_values.resize(language_codes.size(), "<td contenteditable=\"true\"></td>");
        }
        const auto index(std::find(language_codes.begin(), language_codes.end(), db_row["language_code"]) -
                     language_codes.begin());
        row_values[index] = CreateEditableRowEntry(current_token, db_row["translation"], db_row["language_code"], 
                                           db_row["translator"], "vufind_translations");
    } while (db_row = result_set.getNextRow());
    rows->emplace_back("<td contenteditable=\"true\">" + HtmlUtil::HtmlEscape(current_token) + "</td>" + StringUtil::Join(row_values, ""));
}


void GetDisplayLanguages(std::vector<std::string> *const display_languages, const std::vector<std::string> &translation_languages,
                         const std::vector<std::string> &additional_view_languages) {

    display_languages->clear();
    // Insert German as Display language in any case
    if (std::find(translation_languages.begin(), translation_languages.end(), "ger") == translation_languages.end())
        display_languages->emplace_back("ger");
    // Insert German Synonyms in any case
    display_languages->emplace_back(SYNONYM_COLUMN_DESCRIPTOR);
    display_languages->insert(display_languages->end(), translation_languages.begin(), translation_languages.end());
    display_languages->insert(display_languages->end(), additional_view_languages.begin(), additional_view_languages.end());
}


int GetColumnIndexForColumnHeading(const std::vector<std::string> &column_headings, const std::vector<std::string> &row_values, const std::string &heading) {
    auto heading_pos(std::find(column_headings.cbegin(), column_headings.cend(), heading));
    if (heading_pos == column_headings.end())
        return NO_INDEX;

    auto index(heading_pos - column_headings.cbegin());
    try {
        row_values.at(index);
    } catch (std::out_of_range& x) {
        return NO_INDEX;
    }
    return index;
}


bool IsTranslatorLanguage(const std::vector<std::string> &translator_languages, const std::string &lang) {
    return std::find(translator_languages.cbegin(), translator_languages.cend(), lang) != translator_languages.cend();
}


std::string CreateNonEditableRowEntry(const std::string &value) {
   return  "<td style=\"background-color:lightgrey\">" +  HtmlUtil::HtmlEscape(value) + "</td>";
}


std::string CreateNonEditableSynonymEntry(const std::vector<std::string> &values, const std::string &separator) {
   std::vector<std::string> html_escaped_values(values.size());
   std::for_each(values.cbegin(), values.cend(), boost::bind(&HtmlUtil::HtmlEscape, _1));
   return "<td style=\"background-color:lightgrey; font-size:small\">" +  StringUtil::Join(values, separator) + "</td>";
}


std::string CreateNonEditableHintEntry(const std::string &value, const std::string gnd_code) {
  return "<td style=\"background-color:lightgrey\"><a href = \"/Keywordchainsearch/Results?lookfor=" + HtmlUtil::HtmlEscape(value) +
                                                   "\" target=\"_blank\">" + HtmlUtil::HtmlEscape(value) + "</a>"
                                                   "<a href=\"http://d-nb.info/gnd/" + HtmlUtil::HtmlEscape(gnd_code) + "\""
                                                   " style=\"float:right\" target=\"_blank\">GND</a></td>";
}


void GetSynonymsForGNDCode(DbConnection &db_connection, const std::string &gnd_code, std::vector<std::string> *const synonyms) {
    synonyms->clear();
    const std::string synonym_query("SELECT translation FROM keyword_translations WHERE gnd_code=\'" + gnd_code + "\' AND status=\'reliable_synonym\'");
    DbResultSet result_set(ExecSqlOrDie(synonym_query, db_connection));
    if (result_set.empty())
        return;

    while (auto db_row = result_set.getNextRow())
        synonyms->emplace_back(db_row["translation"]);
}


void GetKeyWordTranslationsAsHTMLRowsFromDatabase(DbConnection &db_connection, const std::string &lookfor,
                                                  const std::string &offset, std::vector<std::string> *const rows,
                                                  std::string *const headline, 
                                                  const std::vector<std::string> &translator_languages, 
                                                  const std::vector<std::string> &additional_view_languages) {
    rows->clear();

    // For short strings make a prefix search, otherwise search substring
    const std::string searchpattern(lookfor.size() <= 3 ? "\'" + lookfor + "\'" : "\'%" + lookfor+ "%\'");
    const std::string ppn_where_clause(lookfor.empty() ? "" : " WHERE translation LIKE " + searchpattern);
    const std::string subsearch_query("(SELECT * FROM keyword_translations WHERE ppn IN (SELECT ppn FROM keyword_translations " + ppn_where_clause + "))");
    const std::string translation_sort_limiter(lookfor.empty()  ? "LIMIT " + offset + ", " + std::to_string(ENTRIES_PER_PAGE) : "");
    const std::string translation_sort_join("INNER JOIN (SELECT DISTINCT ppn,translation FROM keyword_translations "
                                            "WHERE language_code='ger' AND status='reliable' "
                                            "ORDER BY translation " +
                                            translation_sort_limiter +
                                            ") AS t ON k.ppn = t.ppn ORDER BY t.translation, k.ppn");
    const std::string inner_query("SELECT k.ppn, k.translation, k.language_code, k.gnd_code, k.status, k.translator FROM " + subsearch_query + " AS k " + 
                              translation_sort_join);

    const std::string lookfor_limiter(lookfor.empty() ? "" : "LIMIT " + offset + ", " + std::to_string(ENTRIES_PER_PAGE));

    const std::string query("SELECT * FROM (" + inner_query + ") AS v WHERE status != \"reliable_synonym\" AND status != \"unreliable_synonym\" " 
                            + lookfor_limiter);

std::cerr << query << '\n';
    DbResultSet result_set(ExecSqlOrDie(query, db_connection));

    std::vector<std::string> language_codes(GetLanguageCodes(db_connection));

    std::vector<std::string> display_languages;
    GetDisplayLanguages(&display_languages, translator_languages, additional_view_languages);
    *headline = "<th>" + StringUtil::Join(display_languages, "</th><th>") + "</th>";
    if (result_set.empty())
        return;

    std::vector<std::string> row_values(display_languages.size());
    std::string current_ppn;
    while (auto db_row = result_set.getNextRow()) {

       // Add new entries as long as there is a single PPN
       std::string ppn(db_row["ppn"]);
       std::string translation(db_row["translation"]);
       std::string language_code(db_row["language_code"]);
       std::string status(db_row["status"]);
       std::string translator(db_row["translator"]);
       std::string gnd_code(db_row["gnd_code"]);
       if (current_ppn != ppn){
           if (not current_ppn.empty())
              rows->emplace_back(StringUtil::Join(row_values, ""));

           current_ppn = ppn;
           row_values.clear();
           row_values.resize(display_languages.size(), "<td style=\"background-color:lightgrey\"></td>");
           for (auto translator_language : translator_languages) {
               int index(GetColumnIndexForColumnHeading(display_languages, row_values, translator_language));
               if (index != NO_INDEX) {
                   row_values[index] = CreateEditableRowEntry(current_ppn, "", language_code, "keyword_translations", "", status, gnd_code);
               }
           }
       }

       int index(GetColumnIndexForColumnHeading(display_languages, row_values, language_code));
       if (index == NO_INDEX)
           continue;
       if (IsTranslatorLanguage(translator_languages, language_code)) 
          row_values[index] = CreateEditableRowEntry(current_ppn, translation, language_code, "keyword_translations", translator, status, gnd_code);
       else
          row_values[index] = (language_code == "ger") ? CreateNonEditableHintEntry(translation, gnd_code) :
                                     CreateNonEditableRowEntry(translation);

       // Insert Synonyms
       std::vector<std::string> synonyms;
       GetSynonymsForGNDCode(db_connection, gnd_code, &synonyms);
       int synonym_index(GetColumnIndexForColumnHeading(display_languages, row_values, SYNONYM_COLUMN_DESCRIPTOR));
       if (synonym_index == NO_INDEX)
           continue;
       row_values[synonym_index] = CreateNonEditableSynonymEntry(synonyms, "<br/>");
   }
   rows->emplace_back(StringUtil::Join(row_values, ""));
}


void GenerateDirectJumpTable(std::vector<std::string> *const jump_table) {
    for (char ch('A'); ch <= 'Z'; ++ch)
        jump_table->emplace_back("<td><a href=\"\">" + std::string(1,ch) + "</a></td>");
}


void ShowFrontPage(DbConnection &db_connection, const std::string &lookfor, const std::string &offset, 
                   const std::string &target, const std::string translator,
                   const std::vector<std::string> &translator_languages,
                   const std::vector<std::string> &additional_view_languages) {

    std::map<std::string, std::vector<std::string>> names_to_values_map;
    std::vector<std::string> rows;
    std::string headline;
    std::vector<std::string> jump_entries;
    GenerateDirectJumpTable(&jump_entries);
    names_to_values_map.emplace("direct_jump", jump_entries);

    GetVuFindTranslationsAsHTMLRowsFromDatabase(db_connection, lookfor, offset, &rows, &headline, translator_languages);
    names_to_values_map.emplace("translator", std::vector<std::string> {translator});
    names_to_values_map.emplace("vufind_token_row", rows);
    names_to_values_map.emplace("vufind_token_table_headline", std::vector<std::string> {headline});

    GetKeyWordTranslationsAsHTMLRowsFromDatabase(db_connection, lookfor, offset, &rows, &headline, translator_languages, additional_view_languages);
    names_to_values_map.emplace("keyword_row", rows);
    names_to_values_map.emplace("keyword_table_headline", std::vector<std::string> {headline});

 
    names_to_values_map.emplace("lookfor", std::vector<std::string> {lookfor});
    names_to_values_map.emplace("prev_offset", std::vector<std::string>
                                               {std::to_string(std::max(0, std::stoi(offset) - ENTRIES_PER_PAGE))});
    names_to_values_map.emplace("next_offset",
                                std::vector<std::string> {std::to_string(std::stoi(offset) + ENTRIES_PER_PAGE)});

    names_to_values_map.emplace("target_language_code", std::vector<std::string> {""});
    names_to_values_map.emplace("target_translation_scope", std::vector<std::string> {target});

    std::ifstream translate_html("/var/lib/tuelib/translate_chainer/translation_front_page.html", std::ios::binary);
    MiscUtil::ExpandTemplate(translate_html, std::cout, names_to_values_map);
}




void GetTranslatorLanguages(const IniFile &ini_file, const std::string &translator, std::vector<std::string> *const translator_languages) {
    // If user is an administrator all languages are open for editing, otherwise only the specified ones
    const std::string ini_administrators(ini_file.getString(USER_SECTION, "administrators"));
    std::vector<std::string> administrators;
    StringUtil::Split(ini_administrators, ",", &administrators);
    std::for_each(administrators.begin(), administrators.end(), boost::bind(&boost::trim<std::string>, _1, std::locale()));

    std::string ini_translator_languages;
    if (std::find(administrators.begin(), administrators.end(), translator) != administrators.end())
        ini_translator_languages = ini_file.getString(LANGUAGES_SECTION, ALL_SUPPORTED_LANGUAGES);
    else
        ini_translator_languages = ini_file.getString(TRANSLATION_LANGUAGES_SECTION, translator);

    StringUtil::Split(ini_translator_languages, ",", translator_languages);
    std::for_each(translator_languages->begin(), translator_languages->end(),  
                   boost::bind(&boost::trim<std::string>, _1, std::locale()));
}


void GetAdditionalViewLanguages(const IniFile &ini_file, std::vector<std::string> *const additional_view_languages, const std::string &translator) {
    const std::string ini_additional_view_languages(ini_file.getString(ADDITIONAL_VIEW_LANGUAGES, translator));
    StringUtil::Split(ini_additional_view_languages, ",", additional_view_languages);
    std::for_each(additional_view_languages->begin(), additional_view_languages->end(),
                   boost::bind(&boost::trim<std::string>, _1, std::locale()));
}

const std::string CONF_FILE_PATH("/var/lib/tuelib/translations.conf");

int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        std::multimap<std::string, std::string> cgi_args;
        WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);

        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("Database", "sql_database"));
        const std::string sql_username(ini_file.getString("Database", "sql_username"));
        const std::string sql_password(ini_file.getString("Database", "sql_password"));
        DbConnection db_connection(sql_database, sql_username, sql_password);

        const std::string translator(GetTranslatorOrEmptyString());

        if (translator.empty()) {
            ShowErrorPage("Error - No Valid User", "Not valid user selected");
            std::exit(0);
        }

        // Read in the views for the respective users
        std::vector<std::string> translator_languages;
        GetTranslatorLanguages(ini_file, translator, &translator_languages);
        if (translator_languages.size() == 0)
            ShowErrorPage("Error - No languages", "No languages specified for user " + translator , "Contact your administrator");
        std::vector<std::string> additional_view_languages;
        GetAdditionalViewLanguages(ini_file, &additional_view_languages, translator);

        std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";

        const std::string lookfor(GetCGIParameterOrDefault(cgi_args, "lookfor", ""));
        const std::string offset(GetCGIParameterOrDefault(cgi_args, "offset", "0"));
        const std::string translation_target(GetCGIParameterOrDefault(cgi_args, "target", "keywords"));
        ShowFrontPage(db_connection, lookfor, offset, translation_target, translator, translator_languages, additional_view_languages);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
