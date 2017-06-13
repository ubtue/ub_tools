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
#include <set>
#include <sstream>
#include <string>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "ExecUtil.h"
#include "EmailSender.h"
#include "FileUtil.h"
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
const std::string EMAIL_SECTION("Email");
const std::string ALL_SUPPORTED_LANGUAGES("all");
const std::string SYNONYM_COLUMN_DESCRIPTOR("syn");
const std::string TOKEN_COLUMN_DESCRIPTOR("token");
const std::string MACS_COLUMN_DESCRIPTOR("macs");
const int NO_INDEX(-1);
const unsigned int LOOKFOR_PREFIX_LIMIT(3);

enum Category { VUFIND, KEYWORDS };


DbResultSet ExecSqlAndReturnResultsOrDie(const std::string &select_statement, DbConnection * const db_connection) {
    db_connection->queryOrDie(select_statement);
    return db_connection->getLastResultSet();
}


std::vector<std::string> GetLanguageCodesFromTable(DbConnection &db_connection, const std::string &table_name) {
    const std::string query("SELECT DISTINCT language_code from " + table_name + " ORDER BY language_code;");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(query, &db_connection));
    std::vector <std::string> language_codes;
    while (const DbRow db_row = result_set.getNextRow())
        language_codes.emplace_back(db_row["language_code"]);

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
                                          const std::string &language_code, const std::string &gnd_code = "",
                                          const std::string &translation = "") {

       return std::string(" category=\"" +  UrlUtil::UrlEncode(category) + "\" index=\"" + UrlUtil::UrlEncode(index)
                          + "\" language_code=\"" + UrlUtil::UrlEncode(language_code) + "\" gnd_code=\"" + gnd_code
                          + "\" translation=\"" + translation + "\" ");
}


std::string CreateEditableRowEntry(const std::string &token, const std::string &label,
                                   const std::string language_code, const std::string &category,
                                   const std::string db_translator, const std::string &gnd_code = "")
{
    std::string term_identifiers(AssembleTermIdentifiers(category, token, language_code, gnd_code, label));
    std::string background_color((GetTranslatorOrEmptyString() == db_translator) ? "RoyalBlue" : "LightBlue");
    const bool translator_exists(not db_translator.empty() ? true : false);
    return "<td contenteditable=\"true\" class=\"editable_translation\"" + term_identifiers
           + "style=\"background-color:" + background_color +  "\""
           + (translator_exists ? " translator_exists=\"1\"" : "") + ">"
           + HtmlUtil::HtmlEscape(label) +"</td>";
}


void GetDisplayLanguages(std::vector<std::string> *const display_languages, const std::vector<std::string> &translation_languages,
                         const std::vector<std::string> &additional_view_languages, enum Category category = KEYWORDS) {

    display_languages->clear();

    if (category == VUFIND)
        display_languages->emplace_back(TOKEN_COLUMN_DESCRIPTOR);

    // Insert German as Display language in any case
    if (std::find(translation_languages.cbegin(), translation_languages.cend(), "ger") == translation_languages.end())
        display_languages->emplace_back("ger");

    display_languages->insert(display_languages->end(), translation_languages.begin(), translation_languages.end());
    display_languages->insert(display_languages->end(), additional_view_languages.begin(), additional_view_languages.end());

    // For Keywords show also MACS and the synonyms
    if (category == KEYWORDS){
        display_languages->emplace_back(MACS_COLUMN_DESCRIPTOR);
        display_languages->emplace(std::find(display_languages->begin(), display_languages->end(), "ger") + 1, SYNONYM_COLUMN_DESCRIPTOR);
    }
}


bool IsTranslatorLanguage(const std::vector<std::string> &translator_languages, const std::string &lang) {
    return std::find(translator_languages.cbegin(), translator_languages.cend(), lang) != translator_languages.cend();
}


std::string CreateNonEditableRowEntry(const std::string &value) {
   return  "<td style=\"background-color:lightgrey\">" +  HtmlUtil::HtmlEscape(value) + "</td>";
}


std::string CreateNonEditableSynonymEntry(std::vector<std::string> values, const std::string &separator) {
    for (auto &value : values)
        HtmlUtil::HtmlEscape(&value);
    return "<td style=\"background-color:lightgrey; font-size:small\">" +  StringUtil::Join(values, separator)
           + "</td>";
}


std::string CreateNonEditableHintEntry(const std::string &value, const std::string gnd_code) {
  return "<td style=\"background-color:lightgrey\"><a href = \"/Keywordchainsearch/Results?lookfor=" + HtmlUtil::HtmlEscape(value) +
                                                   "\" target=\"_blank\">" + HtmlUtil::HtmlEscape(value) + "</a>"
                                                   "<a href=\"http://d-nb.info/gnd/" + HtmlUtil::HtmlEscape(gnd_code) + "\""
                                                   " style=\"float:right\" target=\"_blank\">GND</a></td>";
}


void GetSynonymsForGNDCode(DbConnection &db_connection, const std::string &gnd_code,
                           std::vector<std::string> *const synonyms)
{
    synonyms->clear();
    const std::string synonym_query("SELECT translation FROM keyword_translations WHERE gnd_code=\'" + gnd_code
                                    + "\' AND status=\'reliable_synonym\'");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(synonym_query, &db_connection));
    if (result_set.empty())
        return;

    while (auto db_row = result_set.getNextRow())
        synonyms->emplace_back(db_row["translation"]);
}


void GetMACSTranslationsForGNDCode(DbConnection &db_connection, const std::string &gnd_code,
                                   std::vector<std::string> *const translations)
{
    translations->clear();
    const std::string macs_query("SELECT translation FROM keyword_translations WHERE gnd_code=\'" + gnd_code + "\' "
                                 "AND origin=750 AND status=\'unreliable\'");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(macs_query, &db_connection));
    if (result_set.empty())
        return;

    while (auto db_row = result_set.getNextRow())
        translations->emplace_back(db_row["translation"]);
}


int GetColumnIndexForColumnHeading(const std::vector<std::string> &column_headings,
                                   const std::vector<std::string> &row_values, const std::string &heading)
{
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


void GetVuFindTranslationsAsHTMLRowsFromDatabase(DbConnection &db_connection, const std::string &lookfor,
                                                 const std::string &offset, std::vector<std::string> * const rows,
                                                 std::string * const headline,
                                                 const std::vector<std::string> translator_languages,
                                                 const std::vector<std::string> &additional_view_languages)
{
    rows->clear();

    const std::string searchpattern(lookfor.size() <=
                                    LOOKFOR_PREFIX_LIMIT ? "LIKE \'" + lookfor + "%\'" : "LIKE \'%" + lookfor + "%\'");
    const std::string token_where_clause(
            lookfor.empty() ? "" : "WHERE token " + searchpattern);
    const std::string token_query("SELECT token FROM vufind_translations " + token_where_clause + " ORDER BY token");
    const std::string query("SELECT token, translation, language_code, translator FROM vufind_translations "
                            "WHERE token IN (SELECT * FROM (" + token_query
                            + ") as t) ORDER BY token, language_code");

    const std::string create_vufind_ger_sorted("CREATE TEMPORARY TABLE vufind_ger_sorted AS (" + query + ")");
    db_connection.queryOrDie(create_vufind_ger_sorted);

    const std::string create_sort_limit("CREATE TEMPORARY TABLE vufind_sort_limit AS (SELECT token FROM vufind_ger_sorted WHERE language_code='ger'"
                                 " ORDER BY token LIMIT " + offset + ", " + std::to_string(ENTRIES_PER_PAGE) +  ")");
    db_connection.queryOrDie(create_sort_limit);


    const std::string create_result_with_limit("SELECT  token, translation, language_code, translator FROM vufind_ger_sorted AS v"
                                       " INNER JOIN vufind_sort_limit AS u USING (token)");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(create_result_with_limit, &db_connection));

    std::vector<std::string> language_codes(GetLanguageCodes(db_connection));
    std::vector<std::string> display_languages;
    GetDisplayLanguages(&display_languages, translator_languages, additional_view_languages, VUFIND);
    *headline = "<th>" + StringUtil::Join(display_languages, "</th><th>") + "</th>";
    if (result_set.empty())
        return;

    std::vector<std::string> row_values(display_languages.size());
    std::string current_token;
    while (auto db_row = result_set.getNextRow()) {
       std::string token(db_row["token"]);
       std::string translation(db_row["translation"]);
       std::string language_code(db_row["language_code"]);
       std::string translator(db_row["translator"]);
       if (current_token != token){
           if (not current_token.empty())
              rows->emplace_back(StringUtil::Join(row_values, ""));

           current_token = token;
           row_values.clear();
           row_values.resize(display_languages.size(), "<td style=\"background-color:lightgrey\"></td>");
           int token_index(GetColumnIndexForColumnHeading(display_languages, row_values, TOKEN_COLUMN_DESCRIPTOR));
           if (token_index == NO_INDEX)
               continue;
           row_values[token_index] = CreateNonEditableRowEntry(token);
           for (auto translator_language : translator_languages) {
               int index(GetColumnIndexForColumnHeading(display_languages, row_values, translator_language));
               if (index != NO_INDEX) {
                   row_values[index] = CreateEditableRowEntry(current_token, "", language_code, "vufind_translations",
                                                              translator);
               }
           }
       }

       int index(GetColumnIndexForColumnHeading(display_languages, row_values, language_code));
       if (index == NO_INDEX)
           continue;
       if (IsTranslatorLanguage(translator_languages, language_code))
          row_values[index] = CreateEditableRowEntry(current_token, translation, language_code, "vufind_translations",
                                                     translator);
       else
          row_values[index] = CreateNonEditableRowEntry(translation);
   }
   rows->emplace_back(StringUtil::Join(row_values, ""));
   // We may not use a ';' here within a query to prevent mysql from getting out of sync
   const std::string drop_ger_sorted("DROP TEMPORARY TABLE vufind_ger_sorted");
   const std::string drop_sort_limit("DROP TEMPORARY TABLE vufind_sort_limit");
   db_connection.queryOrDie(drop_ger_sorted);
   db_connection.queryOrDie(drop_sort_limit);
}


void GetKeyWordTranslationsAsHTMLRowsFromDatabase(DbConnection &db_connection, const std::string &lookfor,
                                                  const std::string &offset, std::vector<std::string> *const rows,
                                                  std::string *const headline,
                                                  const std::vector<std::string> &translator_languages,
                                                  const std::vector<std::string> &additional_view_languages) {
    rows->clear();

    // For short strings make a prefix search, otherwise search substring
    const std::string searchpattern(lookfor.size() <= LOOKFOR_PREFIX_LIMIT ?
                                    "AND k.translation LIKE \'" + lookfor + "%\'" :
                                    "AND l.ppn IN (SELECT ppn from keyword_translations WHERE translation LIKE \'%" + lookfor+ "%\')");

    const std::string search_clause(lookfor.empty() ? "" : searchpattern);
    const std::string query("SELECT l.ppn, l.translation, l.language_code, l.gnd_code, l.status, l.translator FROM keyword_translations AS k"
                            " INNER JOIN keyword_translations AS l ON k.language_code=\'ger\' AND k.status=\'reliable\'"
                            " AND k.ppn=l.ppn AND l.status!=\'reliable_synonym\' AND l.status != \'unreliable_synonym\'" +
                            search_clause + " ORDER BY k.translation");

    // The LIMIT parameter can only work with constants, but we want entries per page to be lines, i.e. german translations in our table
    // so we have to generate a dynamic limit using temporary tables

    const std::string create_keywords_ger_sorted("CREATE TEMPORARY TABLE keywords_ger_sorted AS (" + query + ")");
    db_connection.queryOrDie(create_keywords_ger_sorted);

    const std::string create_sort_limit("CREATE TEMPORARY TABLE sort_limit AS (SELECT ppn FROM keywords_ger_sorted "
                                        "WHERE language_code='ger' ORDER BY translation  LIMIT " + offset + ", "
                                        + std::to_string(ENTRIES_PER_PAGE) +  ")");
    db_connection.queryOrDie(create_sort_limit);

    const std::string create_result_with_limit("SELECT  ppn, translation, language_code, gnd_code, status, translator FROM keywords_ger_sorted AS v"
                                       " INNER JOIN sort_limit AS u USING (ppn)");

    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(create_result_with_limit, &db_connection));

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
               if (index != NO_INDEX)
                   row_values[index] = (translator_language == "ger") ? CreateNonEditableRowEntry("") :
                                       CreateEditableRowEntry(current_ppn, "", language_code, "keyword_translations",
                                                              "", gnd_code);
           }
       }

       int index(GetColumnIndexForColumnHeading(display_languages, row_values, language_code));
       if (index == NO_INDEX)
           continue;
       if (IsTranslatorLanguage(translator_languages, language_code))
          row_values[index] = (language_code == "ger") ? CreateNonEditableRowEntry(translation) :
                              CreateEditableRowEntry(current_ppn, translation, language_code, "keyword_translations",
                                                     translator, gnd_code);
       else
           row_values[index] = (language_code == "ger") ? CreateNonEditableHintEntry(translation, gnd_code)
                                                        : CreateNonEditableRowEntry(translation);

       // Insert Synonyms
       std::vector<std::string> synonyms;
       GetSynonymsForGNDCode(db_connection, gnd_code, &synonyms);
       int synonym_index(GetColumnIndexForColumnHeading(display_languages, row_values, SYNONYM_COLUMN_DESCRIPTOR));
       if (synonym_index == NO_INDEX)
           continue;
       row_values[synonym_index] = CreateNonEditableSynonymEntry(synonyms, "<br/>");

       // Insert MACS Translations display table
       std::vector<std::string> macs_translations;
       GetMACSTranslationsForGNDCode(db_connection, gnd_code, &macs_translations);
       int macs_index(GetColumnIndexForColumnHeading(display_languages, row_values, MACS_COLUMN_DESCRIPTOR));
       if (macs_index == NO_INDEX)
           continue;
       row_values[macs_index] = CreateNonEditableSynonymEntry(macs_translations, "<br/>");
    }
    rows->emplace_back(StringUtil::Join(row_values, ""));
    const std::string drop_get_sorted("DROP TEMPORARY TABLE keywords_ger_sorted");
    const std::string drop_sort_limit("DROP TEMPORARY TABLE sort_limit");
    db_connection.queryOrDie(drop_get_sorted);
    db_connection.queryOrDie(drop_sort_limit);
}


void GenerateDirectJumpTable(std::vector<std::string> * const jump_table, enum Category category = KEYWORDS) {
    for (char ch('A'); ch <= 'Z'; ++ch) {
        // We use buttons an style them as link conform to post semantics
        std::string post_link(
         R"END(<form action="/cgi-bin/translator" method="POST">
            <button type="submit" class="link-button">)END" + std::string(1,ch) + "</button>"
         R"END(<input type="hidden" name="lookfor" value=")END" + std::string(1,ch) + "\">"
         R"END(<input type="hidden" name="target" value=")END" + (category == VUFIND ? "vufind" : "keywords") + "\">"
         "</form>");
        jump_table->emplace_back("<td style=\"border:none;\">" + post_link + "</td>");
    }
}


void ShowFrontPage(DbConnection &db_connection, const std::string &lookfor, const std::string &offset,
                   const std::string &target, const std::string translator,
                   const std::vector<std::string> &translator_languages,
                   const std::vector<std::string> &additional_view_languages)
{
    std::map<std::string, std::vector<std::string>> names_to_values_map;
    std::vector<std::string> rows;
    std::string headline;
    std::vector<std::string> jump_entries_keywords;
    GenerateDirectJumpTable(&jump_entries_keywords, KEYWORDS);
    names_to_values_map.emplace("direct_jump_keywords", jump_entries_keywords);

    std::vector<std::string> jump_entries_vufind;
    GenerateDirectJumpTable(&jump_entries_vufind, VUFIND);
    names_to_values_map.emplace("direct_jump_vufind", jump_entries_vufind);

    GetVuFindTranslationsAsHTMLRowsFromDatabase(db_connection, lookfor, offset, &rows, &headline,
                                                translator_languages, additional_view_languages);
    names_to_values_map.emplace("translator", std::vector<std::string> {translator});
    names_to_values_map.emplace("vufind_token_row", rows);
    names_to_values_map.emplace("vufind_token_table_headline", std::vector<std::string> {headline});

    GetKeyWordTranslationsAsHTMLRowsFromDatabase(db_connection, lookfor, offset, &rows, &headline,
                                                 translator_languages, additional_view_languages);
    names_to_values_map.emplace("keyword_row", rows);
    names_to_values_map.emplace("keyword_table_headline", std::vector<std::string> {headline});


    names_to_values_map.emplace("lookfor", std::vector<std::string> {lookfor});
    names_to_values_map.emplace("prev_offset", std::vector<std::string>
                                               { std::to_string(std::max(0, std::stoi(offset) - ENTRIES_PER_PAGE)) });
    names_to_values_map.emplace("next_offset",
                                std::vector<std::string> {std::to_string(std::stoi(offset) + ENTRIES_PER_PAGE)});

    names_to_values_map.emplace("current_offset", std::vector<std::string> {offset});

    names_to_values_map.emplace("target_language_code", std::vector<std::string> {""});
    names_to_values_map.emplace("target_translation_scope", std::vector<std::string> {target});

    std::ifstream translate_html("/var/lib/tuelib/translate_chainer/translation_front_page.html", std::ios::binary);
    MiscUtil::ExpandTemplate(translate_html, std::cout, names_to_values_map);
}


void GetTranslatorLanguages(const IniFile &ini_file, const std::string &translator,
                            std::vector<std::string> * const translator_languages)
{
    // If user is an administrator all languages are open for editing, otherwise only the specified ones
    const std::string ini_administrators(ini_file.getString(USER_SECTION, "administrators"));
    std::vector<std::string> administrators;
    StringUtil::Split(ini_administrators, ",", &administrators);
    std::for_each(administrators.begin(), administrators.end(),
                  boost::bind(&boost::trim<std::string>, _1, std::locale()));

    std::string ini_translator_languages;
    if (std::find(administrators.begin(), administrators.end(), translator) != administrators.end())
        ini_translator_languages = ini_file.getString(LANGUAGES_SECTION, ALL_SUPPORTED_LANGUAGES);
    else
        ini_translator_languages = ini_file.getString(TRANSLATION_LANGUAGES_SECTION, translator);

    StringUtil::Split(ini_translator_languages, ',', translator_languages);
    std::for_each(translator_languages->begin(), translator_languages->end(),
                   boost::bind(&boost::trim<std::string>, _1, std::locale()));
}


void GetAdditionalViewLanguages(const IniFile &ini_file, std::vector<std::string> *const additional_view_languages,
                                const std::string &translator)
{
    const std::string ini_additional_view_languages(ini_file.getString(ADDITIONAL_VIEW_LANGUAGES, translator, ""));
    StringUtil::Split(ini_additional_view_languages, ",", additional_view_languages);
    std::for_each(additional_view_languages->begin(), additional_view_languages->end(),
                   boost::bind(&boost::trim<std::string>, _1, std::locale()));
}


void GetAsciiTableForQuery(DbConnection &db_connection,
                           std::vector<std::string> *const rows, const std::string &query,
                           std::vector<std::string> &display_languages, enum Category category)
{
    rows->clear();
    // Create Heading
    rows->emplace_back("<th>" + StringUtil::Join(display_languages, "</th><th>") + "</th>");

    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(query, &db_connection));
    if (result_set.empty())
        return;

    std::vector<std::string> row_values(display_languages.size(), "<td></td>");
    DbRow db_row(result_set.getNextRow());
    std::string current_id(category == KEYWORDS ? db_row["ppn"] : db_row["token"]);
    do {
        const std::string id(category == KEYWORDS ? db_row["ppn"] : db_row["token"]);
        const std::string language_code(db_row["language_code"]);
        const std::string db_translator(db_row["translator"]);
        if (id != current_id) {
            rows->emplace_back(StringUtil::Join(row_values, ""));
            row_values.clear();
            row_values.resize(display_languages.size(), "<td></td>");
            current_id = id;
        }
        if (language_code != "ger" and db_translator != GetTranslatorOrEmptyString())
            continue;
        int index(GetColumnIndexForColumnHeading(display_languages, row_values, language_code));
        if (index == NO_INDEX)
            continue;
        row_values[index] = "<td>" + db_row["translation"] + "</td>";

    } while (db_row = result_set.getNextRow());
    rows->emplace_back(StringUtil::Join(row_values, ""));
}


bool AssembleMyTranslationsData(DbConnection &db_connection, const IniFile &ini_file,
                                std::map<std::string, std::vector<std::string>> *const names_to_values_map,
                                const std::string &translator)
{
    // Insert Translator
    names_to_values_map->emplace("translator", std::vector<std::string> {translator});

    // Get Translator Languages
    std::vector<std::string> translator_languages;
    GetTranslatorLanguages(ini_file, translator, &translator_languages);

    std::vector<std::string> display_languages(translator_languages);
    if (std::find(display_languages.begin(), display_languages.end(), "ger") == display_languages.end())
       display_languages.emplace(display_languages.begin(), "ger");

    // Get Vufind Translations
    const std::string vufind_query("SELECT token, translation, language_code, translator FROM vufind_translations "
                                   "WHERE token IN (SELECT * FROM (SELECT token FROM vufind_translations WHERE "
                                   "translator=\'" + translator + "\') as t) ORDER BY token, language_code;");

    std::vector<std::string> vufind_rows;
    GetAsciiTableForQuery(db_connection, &vufind_rows, vufind_query, display_languages, VUFIND);
    names_to_values_map->emplace("vufind_translations", vufind_rows);

    // Get Keyword Translations
    const std::string keyword_query("SELECT l.ppn, l.translation, l.language_code, l.translator FROM "
                                    "keyword_translations AS k INNER JOIN keyword_translations AS l ON "
                                    "k.language_code=\'ger\' AND k.status=\'reliable\' AND k.ppn=l.ppn AND "
                                    "l.status!=\'reliable_synonym\' AND l.status != \'unreliable_synonym\'"
                                    " AND l.ppn IN (SELECT ppn from keyword_translations WHERE translator=\'"
                                    + translator + "\') ORDER BY k.translation;");

    std::vector<std::string> keyword_rows;
    GetAsciiTableForQuery(db_connection, &keyword_rows, keyword_query, display_languages, KEYWORDS);
    names_to_values_map->emplace("keyword_translations", keyword_rows);
    return true;
}


void MailMyTranslations(DbConnection &db_connection, const IniFile &ini_file, const std::string translator) {
    std::map<std::string, std::vector<std::string>> names_to_values_map;
    if (unlikely(not AssembleMyTranslationsData(db_connection, ini_file, &names_to_values_map, translator)))
        Error("Could not send mail");

    //Expand Template
    std::stringstream mail_content;
    std::ifstream mytranslations_template("/var/lib/tuelib/translate_chainer/mytranslations_template.msg",
                                          std::ios::binary);
    MiscUtil::ExpandTemplate(mytranslations_template, mail_content, names_to_values_map);

    // Get Mail address
    const std::string email(ini_file.getString(EMAIL_SECTION, translator, ""));
    if (email.empty())
        return;

    if (unlikely(not SendEmail("no-reply-ixtheo@uni-tuebingen.de", email, "Your IxTheo Translations",
                               mail_content.str(), EmailSender::DO_NOT_SET_PRIORITY, EmailSender::HTML)))
        Error("Could not send mail");
}


void SaveUserState(DbConnection &db_connection, const std::string &translator, const std::string &translation_target,
                   const std::string &lookfor, const std::string &offset)
{
   const std::string save_statement("REPLACE INTO translators SET translator=\'" + translator +
                                    "\', translation_target=\'" + translation_target + "\', offset=\'" + offset
                                    + "\', lookfor=\'" + lookfor + "\';");

   db_connection.queryOrDie(save_statement);
}


void RestoreUserState(DbConnection &db_connection, const std::string &translator,
                      const std::string &translation_target, std::string * const lookfor, std::string * const offset)
{
   const std::string restore_statement("SELECT lookfor, offset FROM translators WHERE translator=\'" + translator
                                       + "\' AND translation_target=\'" + translation_target + "\';");
   DbResultSet result_set(ExecSqlAndReturnResultsOrDie(restore_statement, &db_connection));
   if (result_set.empty())
       return;

   DbRow row(result_set.getNextRow());
   *lookfor = row["lookfor"];
   *offset  = row["offset"];
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
            ShowErrorPage("Error - No languages", "No languages specified for user " + translator ,
                          "Contact your administrator");
        std::vector<std::string> additional_view_languages;
        GetAdditionalViewLanguages(ini_file, &additional_view_languages, translator);

        std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";

        const std::string mail(GetCGIParameterOrDefault(cgi_args, "mail", ""));
        if (mail == "mytranslations")
           MailMyTranslations(db_connection, ini_file, translator);

        std::string lookfor(GetCGIParameterOrDefault(cgi_args, "lookfor", ""));
        std::string offset(GetCGIParameterOrDefault(cgi_args, "offset", "0"));
        const std::string translation_target(GetCGIParameterOrDefault(cgi_args, "target", "keywords"));

        const std::string save_action(GetCGIParameterOrDefault(cgi_args, "save_action", ""));
        if (save_action == "save")
 	    SaveUserState(db_connection, translator, translation_target, lookfor, offset);
        else if (save_action == "restore")
            RestoreUserState(db_connection, translator, translation_target, &lookfor, &offset);

        ShowFrontPage(db_connection, lookfor, offset, translation_target, translator, translator_languages,
                      additional_view_languages);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
