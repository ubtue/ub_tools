/** \file    translator.cc
 *  \brief   A CGI-tool for translating vufind tokens and keywords.
 *  \author  Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author  Johannes Riedl
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
#include <tuple>
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
#include "WebUtil.h"
#include "util.h"


namespace {


const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "translations.conf");
const int ENTRIES_PER_PAGE(30);
const std::string LANGUAGES_SECTION("Languages");
const std::string TRANSLATION_LANGUAGES_SECTION("TranslationLanguages");
const std::string ADDITIONAL_VIEW_LANGUAGES("AdditionalViewLanguages");
const std::string USER_SECTION("Users");
const std::string EMAIL_SECTION("Email");
const std::string CONFIGURATION_SECTION("Configuration");
const std::string ALL_SUPPORTED_LANGUAGES("all");
const std::string SYNONYM_COLUMN_DESCRIPTOR("syn");
const std::string TOKEN_COLUMN_DESCRIPTOR("token");
const std::string MACS_COLUMN_DESCRIPTOR("macs");
const std::string WIKIDATA_COLUMN_DESCRIPTOR("wikidata");
const std::string DISABLE_TRANSLATIONS_SECTION("DisableTranslations");
const std::string DISABLE_TRANSLATION_COLUMN_DESCRIPTOR("disabled");
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
    std::vector<std::string> language_codes;
    while (const DbRow db_row = result_set.getNextRow())
        language_codes.emplace_back(db_row["language_code"]);

    return language_codes;
}


std::vector<std::string> GetLanguageCodes(DbConnection &db_connection) {
    std::vector<std::string> language_codes(GetLanguageCodesFromTable(db_connection, "vufind_translations"));
    for (auto &language_code : GetLanguageCodesFromTable(db_connection, "keyword_translations")) {
        if (std::find(language_codes.begin(), language_codes.end(), language_code) == language_codes.end())
            language_codes.emplace_back(language_code);
    }
    return language_codes;
}


void ShowErrorPageAndDie(const std::string &title, const std::string &error_message, const std::string &description = "") {
    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";
    std::cout << "<!DOCTYPE html><html><head><title>" + title + "</title></head>"
              << "<body>"
              << "  <h1>" + error_message + "</h1>"
              << "  <h3>" + description + "</h3>"
              << "</body>"
              << "</html>";
    std::exit(EXIT_SUCCESS);
}


const std::string GetTranslatorOrEmptyString() {
    return (std::getenv("REMOTE_USER") != nullptr) ? std::getenv("REMOTE_USER") : "";
}


const std::string AssembleTermIdentifiers(const std::string &category, const std::string &index, const std::string &language_code,
                                          const std::string &gnd_code = "", const std::string &translation = "") {
    return std::string(" category=\"" + HtmlUtil::HtmlEscape(category) + "\" index=\"" + HtmlUtil::HtmlEscape(index) + "\" language_code=\""
                       + HtmlUtil::HtmlEscape(language_code) + "\" gnd_code=\"" + gnd_code + "\" comparable=\""
                       + HtmlUtil::HtmlEscape(index) + "\" translation=\"" + HtmlUtil::HtmlEscape(translation) + "\" ");
}


std::string CreateEditableRowEntry(const std::string &token, const std::string &label, const std::string language_code,
                                   const std::string &category, const std::string db_translator, const std::string &gnd_code = "") {
    std::string term_identifiers(AssembleTermIdentifiers(category, token, language_code, gnd_code, label));
    std::string background_color((GetTranslatorOrEmptyString() == db_translator) ? "RoyalBlue" : "LightBlue");
    const bool translator_exists(not db_translator.empty() ? true : false);
    return "<td contenteditable=\"true\" class=\"editable_translation\"" + term_identifiers + "style=\"background-color:" + background_color
           + "\"" + (translator_exists ? " translator_exists=\"1\"" : "") + ">" + HtmlUtil::HtmlEscape(label) + "</td>";
}


void GetDisplayLanguages(std::vector<std::string> * const display_languages, const std::vector<std::string> &translation_languages,
                         const std::vector<std::string> &additional_view_languages, enum Category category,
                         const bool show_macs_col = false, const bool show_wikidata_col = false,
                         const bool show_disable_translations_col = false) {
    display_languages->clear();

    if (category == VUFIND)
        display_languages->emplace_back(TOKEN_COLUMN_DESCRIPTOR);

    // Insert German as Display language in any case
    if (std::find(translation_languages.cbegin(), translation_languages.cend(), "ger") == translation_languages.end())
        display_languages->emplace_back("ger");

    display_languages->insert(display_languages->end(), translation_languages.begin(), translation_languages.end());
    display_languages->insert(display_languages->end(), additional_view_languages.begin(), additional_view_languages.end());

    // For Keywords show also MACS and the synonyms
    if (category == KEYWORDS) {
        if (show_macs_col)
            display_languages->emplace_back(MACS_COLUMN_DESCRIPTOR);

        if (show_wikidata_col)
            display_languages->emplace_back(WIKIDATA_COLUMN_DESCRIPTOR);

        if (show_disable_translations_col)
            display_languages->emplace_back(DISABLE_TRANSLATION_COLUMN_DESCRIPTOR);

        display_languages->emplace(std::find(display_languages->begin(), display_languages->end(), "ger") + 1, SYNONYM_COLUMN_DESCRIPTOR);
    }
}


bool IsTranslatorLanguage(const std::vector<std::string> &translator_languages, const std::string &lang) {
    return std::find(translator_languages.cbegin(), translator_languages.cend(), lang) != translator_languages.cend();
}


std::string CreateNonEditableRowEntry(const std::string &value) {
    return "<td style=\"background-color:lightgrey\">" + HtmlUtil::HtmlEscape(value) + "</td>";
}


std::string CreateNonEditableSynonymEntry(std::vector<std::string> values, const std::string &separator) {
    for (auto &value : values)
        HtmlUtil::HtmlEscape(&value);
    return "<td style=\"background-color:lightgrey; font-size:small\">" + StringUtil::Join(values, separator) + "</td>";
}

using TranslationLangAndWikiID = std::tuple<std::string, std::string, std::string>;

std::string CreateNonEditableWikidataEntry(std::vector<TranslationLangAndWikiID> wikidata_translations) {
    if (wikidata_translations.empty())
        return "<td style=\"background-color:lightgrey; font-size:small\"></td>";

    const auto wiki_id(std::get<2>(wikidata_translations[0]));
    std::vector<std::string> translations_and_langs;
    for (auto &translation_lang_id : wikidata_translations) {
        translations_and_langs.emplace_back(
            HtmlUtil::HtmlEscape(std::get<0>(translation_lang_id) + "(" + std::get<1>(translation_lang_id) + ")"));
    }
    return "<td style=\"background-color:lightgrey; font-size:small\"><a href=\"https://wikidata.org/entity/" + wiki_id
           + "\" target=\"_blank\">" + StringUtil::Join(translations_and_langs, "<br/>") + "</a></td>";
}


std::string CreateEditableDisableTranslationEntry(const std::string &ppn, const bool disabled) {
    return "<td class=\"disable_translation\" index=\"" + HtmlUtil::HtmlEscape(ppn)
           + "\" style=\"background-color:MediumSpringGreen\"><input type=\"checkbox\" class=\"disable_translation_checkbox\""
           + (disabled ? " checked=\"checked\"" : "") + "></td>";
}


std::string ReplaceAngleBracketsByOrdinaryBrackets(const std::string &value) {
    return StringUtil::Map(value, "<>", "()");
}


std::string GetSearchBaseLink(const bool use_subject_link) {
    return use_subject_link ? "/Search/Results?type=Subject&lookfor=" : "/Keywordchainsearch/Results?lookfor=";
}


std::string GetGNDLink(const std::string &gnd_code) {
    if (gnd_code == "0")
        return "";

    return "<a href=\"http://d-nb.info/gnd/" + HtmlUtil::HtmlEscape(gnd_code) + "\""
          " style=\"float:right\" target=\"_blank\">GND</a>";
}


std::string CreateNonEditableHintEntry(const std::string &value, const std::string gnd_code, const bool use_subject_link = false,
                                       const std::string background_color = "lightgrey") {
    return "<td style=\"background-color:" + background_color + "\"  gnd_code=\"" + gnd_code + "\"><a href = \""
           + GetSearchBaseLink(use_subject_link) + UrlUtil::UrlEncode(HtmlUtil::HtmlEscape(ReplaceAngleBracketsByOrdinaryBrackets(value)))
           + "\" target=\"_blank\">" + HtmlUtil::HtmlEscape(value) + "</a>" + GetGNDLink(gnd_code) + "</td>";
}


std::string CreateNonEditableHighlightHintEntry(const std::string &value, const std::string gnd_code, const bool use_subject_link) {
    return CreateNonEditableHintEntry(value, gnd_code, use_subject_link, "lime");
}


void GetSynonymsForGNDCode(DbConnection &db_connection, const std::string &gnd_code, std::vector<std::string> * const synonyms) {
    synonyms->clear();
    if (gnd_code == "0")
        return;
    const std::string synonym_query("SELECT translation FROM keyword_translations WHERE gnd_code='" + gnd_code
                                    + "' AND status='reliable_synonym' AND language_code='ger'");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(synonym_query, &db_connection));
    if (result_set.empty())
        return;

    while (const auto db_row = result_set.getNextRow())
        synonyms->emplace_back(db_row["translation"]);
}


void GetMACSTranslationsForGNDCode(DbConnection &db_connection, const std::string &gnd_code,
                                   std::vector<std::string> * const translations) {
    translations->clear();
    if (gnd_code == "0")
        return;
    const std::string macs_query("SELECT translation FROM keyword_translations WHERE gnd_code='" + gnd_code + "' "
                                 "AND origin=750 AND status='unreliable'");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(macs_query, &db_connection));
    if (result_set.empty())
        return;

    while (const auto db_row = result_set.getNextRow())
        translations->emplace_back(db_row["translation"]);
}


void GetQuotedDisplayLanguagesAsSet(const std::vector<std::string> &translator_languages,
                                    const std::vector<std::string> &additional_view_languages, std::set<std::string> * const target) {
    target->clear();
    for (const auto &languages : { translator_languages, additional_view_languages })
        std::transform(languages.begin(), languages.end(), std::inserter(*target, target->begin()),
                       [](const std::string &lang) { return "'" + lang + "'"; });
}


std::string GetQuotedDisplayLanguagesAsString(const std::vector<std::string> &translator_languages,
                                              const std::vector<std::string> &additional_view_languages) {
    std::set<std::string> target;
    GetQuotedDisplayLanguagesAsSet(translator_languages, additional_view_languages, &target);
    return StringUtil::Join(target, ", ");
}


void GetWikidataTranslationsForGNDCode(DbConnection &db_connection, const std::string &gnd_code,
                                       std::vector<TranslationLangAndWikiID> * const translations_langs_and_wiki_id,
                                       const std::vector<std::string> &translator_languages,
                                       const std::vector<std::string> &additional_view_languages) {
    translations_langs_and_wiki_id->clear();
    if (gnd_code == "0")
        return;
    const std::string wikidata_query("SELECT translation, language_code, wikidata_id FROM keyword_translations WHERE gnd_code='" + gnd_code + "' "
                                 "AND status='unreliable_cat2' AND language_code IN (" +
                                 GetQuotedDisplayLanguagesAsString(translator_languages, additional_view_languages) + ")"
                                 " ORDER BY language_code");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(wikidata_query, &db_connection));
    if (result_set.empty())
        return;
    while (const auto db_row = result_set.getNextRow())
        translations_langs_and_wiki_id->emplace_back(db_row["translation"], db_row["language_code"], db_row["wikidata_id"]);
}


int GetColumnIndexForColumnHeading(const std::vector<std::string> &column_headings, const std::vector<std::string> &row_values,
                                   const std::string &heading) {
    auto heading_pos(std::find(column_headings.cbegin(), column_headings.cend(), heading));
    if (heading_pos == column_headings.end())
        return NO_INDEX;

    auto index(heading_pos - column_headings.cbegin());
    try {
        row_values.at(index);
    } catch (std::out_of_range &x) {
        return NO_INDEX;
    }
    return index;
}


bool IsEmptyEntryWithoutTranslator(const std::string &entry) {
    return (StringUtil::EndsWith(entry, "></td>") and (entry.find("translator_exists") == std::string::npos));
}


bool IsMacsColumnVisible(const IniFile &ini_file) {
    return ini_file.getBool(CONFIGURATION_SECTION, "show_macs_col", false);
}


bool IsWikidataColumnVisible(const IniFile &ini_file) {
    return ini_file.getBool(CONFIGURATION_SECTION, "show_wikidata_col", false);
}


bool IsUseSubjectSearchLink(const IniFile &ini_file) {
    return ini_file.getBool(CONFIGURATION_SECTION, "use_subject_search_link", false);
}


bool TranslatorIsAdministrator(const IniFile &ini_file, const std::string &translator) {
    std::vector<std::string> administrators;
    const std::string ini_administrators(ini_file.getString(USER_SECTION, "administrators"));
    StringUtil::SplitThenTrimWhite(ini_administrators, ',', &administrators);
    return std::find(administrators.begin(), administrators.end(), translator) != administrators.end();
}


bool IsDisableTranslationColVisible(const IniFile &ini_file, const std::string &translator) {
    std::vector<std::string> administrators;
    if (TranslatorIsAdministrator(ini_file, translator))
        return true;
    const std::string ini_disable_translations_users(ini_file.getString(DISABLE_TRANSLATIONS_SECTION, "users"));
    std::vector<std::string> disable_translations_users;
    StringUtil::SplitThenTrimWhite(ini_disable_translations_users, ',', &disable_translations_users);
    return std::find(disable_translations_users.begin(), disable_translations_users.end(), translator) != disable_translations_users.end();
}


bool TranslationDisabledEntryToBool(const std::string &disabled_entry) {
    if (disabled_entry == "1")
        return true;
    if (StringUtil::ASCIIToLower(disabled_entry) == "true")
        return true;
    return false;
}


void GetTranslatorLanguages(const IniFile &ini_file, const std::string &translator, std::vector<std::string> * const translator_languages) {
    // If user is an administrator all languages are open for editing, otherwise only the specified ones
    std::string ini_translator_languages;
    ini_translator_languages = TranslatorIsAdministrator(ini_file, translator)
                                   ? ini_file.getString(LANGUAGES_SECTION, ALL_SUPPORTED_LANGUAGES)
                                   : ini_file.getString(TRANSLATION_LANGUAGES_SECTION, translator);

    StringUtil::Split(ini_translator_languages, ',', translator_languages, /*n suppress_empty_components = */ true);
    std::for_each(translator_languages->begin(), translator_languages->end(),
                  [](std::string &translator_language) { StringUtil::TrimWhite(&translator_language); });
}


std::string GetTranslatedTokensFilterQuery(const bool filter_untranslated, const std::string &lang_untranslated,
                                           const std::vector<std::string> &translator_languages) {
    std::set<std::string> quoted_languages_to_evaluate_set;
    if (filter_untranslated) {
        if (lang_untranslated == "all")
            GetQuotedDisplayLanguagesAsSet(translator_languages, {}, &quoted_languages_to_evaluate_set);
        else
            quoted_languages_to_evaluate_set.emplace("'" + lang_untranslated + "'");

        const std::string languages_to_evaluate(StringUtil::Join(quoted_languages_to_evaluate_set, ", "));
        return "SELECT token FROM vufind_newest WHERE language_code IN (" + languages_to_evaluate + ") "
               "GROUP BY (token) HAVING COUNT(DISTINCT language_code)="
                + std::to_string(quoted_languages_to_evaluate_set.size());
    }
    return "SELECT NULL LIMIT 0";
}


void GetVuFindTranslationsAsHTMLRowsFromDatabase(DbConnection &db_connection, const std::string &lookfor, const std::string &offset,
                                                 std::vector<std::string> * const rows, std::string * const headline,
                                                 const std::vector<std::string> translator_languages,
                                                 const std::vector<std::string> &additional_view_languages, const bool filter_untranslated,
                                                 const std::string &lang_untranslated) {
    rows->clear();

    std::string search_pattern;
    std::string token_search_clause("next_version_id IS NULL");
    if (lookfor.empty()) {
        ;
    } else if (lookfor.size() <= LOOKFOR_PREFIX_LIMIT) {
        search_pattern = "LIKE '" + lookfor + "%'";
        token_search_clause += " AND (token " + search_pattern + ")";
    } else {
        search_pattern = "LIKE '%" + lookfor + "%'";
        token_search_clause += " AND token " + search_pattern + " OR translation " + search_pattern + ")";
    }


    const std::string create_result_with_limit(
            "WITH vufind_newest AS (SELECT * FROM vufind_translations WHERE next_version_id IS NULL),"
            "translated_tokens_for_untranslated_filter AS (" + GetTranslatedTokensFilterQuery(filter_untranslated, lang_untranslated,
                 translator_languages) + "), "
            "tokens AS (SELECT DISTINCT token FROM vufind_translations "
                 "WHERE " +  token_search_clause + " AND token NOT IN (SELECT token FROM translated_tokens_for_untranslated_filter) "
                 "ORDER BY token LIMIT " + offset +  ", " + std::to_string(ENTRIES_PER_PAGE) + "),"
            "result_set AS (SELECT * from vufind_newest WHERE token IN (SELECT * from tokens)) "
            "SELECT token, translation, language_code, translator FROM result_set "
                 "WHERE language_code IN (" + GetQuotedDisplayLanguagesAsString(translator_languages, additional_view_languages) + ", 'ger')");

    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(create_result_with_limit, &db_connection));

    std::vector<std::string> language_codes(GetLanguageCodes(db_connection));
    std::vector<std::string> display_languages;
    GetDisplayLanguages(&display_languages, translator_languages, additional_view_languages, VUFIND);
    *headline = "<th>" + StringUtil::Join(display_languages, "</th><th>") + "</th>";
    if (result_set.empty())
        return;

    std::vector<std::string> row_values(display_languages.size());
    std::string current_token;
    while (const auto db_row = result_set.getNextRow()) {
        std::string token(db_row["token"]);
        std::string translation(db_row["translation"]);
        std::string language_code(db_row["language_code"]);
        std::string translator(db_row["translator"]);
        if (current_token != token) {
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
                if (index != NO_INDEX)
                    row_values[index] = CreateEditableRowEntry(current_token, "", translator_language, "vufind_translations", "");
            }
        }

        int index(GetColumnIndexForColumnHeading(display_languages, row_values, language_code));
        if (index == NO_INDEX)
            continue;
        if (IsTranslatorLanguage(translator_languages, language_code))
            row_values[index] = CreateEditableRowEntry(current_token, translation, language_code, "vufind_translations", translator);
        else
            row_values[index] = CreateNonEditableRowEntry(translation);
    }
    rows->emplace_back(StringUtil::Join(row_values, ""));
}


std::string GetTranslatedPPNsFilterQuery(const bool use_untranslated_filter, const std::string &lang_untranslated,
                                         const std::vector<std::string> &translator_languages) {
    std::set<std::string> quoted_languages_to_evaluate_set;
    if (use_untranslated_filter) {
        if (lang_untranslated == "all")
            GetQuotedDisplayLanguagesAsSet(translator_languages, {}, &quoted_languages_to_evaluate_set);
        else
            quoted_languages_to_evaluate_set.emplace("'" + lang_untranslated + "'");

        const std::string languages_to_evaluate(StringUtil::Join(quoted_languages_to_evaluate_set, ", "));
        return "SELECT ppn FROM keywords_newest WHERE language_code IN (" + languages_to_evaluate + ") AND (translator IS NOT NULL "
                   "OR status IN ('reliable', 'unreliable_cat2', 'unreliable')) "
                   "GROUP BY (ppn) HAVING COUNT(DISTINCT language_code)="
                   + std::to_string(quoted_languages_to_evaluate_set.size());
    }
    return "SELECT NULL LIMIT 0";
}


std::string GetDisabledTranslationsPPNFilterQuery(const bool show_disable_translation_col) {
    // Suppress display only if this disabled_translations is _not_ editable
    if (not show_disable_translation_col)
        return "SELECT DISTINCT ppn FROM keywords_newest WHERE translation_disabled=TRUE";
    return "SELECT NULL LIMIT 0";
}


void GetKeyWordTranslationsAsHTMLRowsFromDatabase(DbConnection &db_connection, const std::string &lookfor, const std::string &offset,
                                                  std::vector<std::string> * const rows, std::string * const headline,
                                                  const std::vector<std::string> &translator_languages,
                                                  const std::vector<std::string> &additional_view_languages,
                                                  const bool use_untranslated_filter, const std::string &lang_untranslated,
                                                  const bool show_macs_col, const bool use_subject_link, const bool show_wikidata_col,
                                                  const bool show_disable_translation_col) {
    rows->clear();

    // For short strings make a prefix search, otherwise search substring
    const std::string search_pattern(
        lookfor.size() <= LOOKFOR_PREFIX_LIMIT
            ? "translation LIKE '" + lookfor + "%'"
            : "ppn IN (SELECT ppn from keyword_translations WHERE next_version_id IS NULL AND translation LIKE '%" + lookfor + "%')");

    const std::string search_clause(lookfor.empty() ? "" : search_pattern + " AND ");

    const std::string create_result_with_limit(
             "WITH keywords_newest AS (SELECT * FROM keyword_translations WHERE next_version_id IS NULL),"
                 // for filter_ppns
                 "translated_ppns_for_untranslated_filter AS ("+ GetTranslatedPPNsFilterQuery(use_untranslated_filter, lang_untranslated,
                     translator_languages) + "), "
                 "disabled_translations_filter AS (" + GetDisabledTranslationsPPNFilterQuery(show_disable_translation_col) + "), "
             "filter_ppns AS (SELECT * FROM translated_ppns_for_untranslated_filter UNION SELECT * FROM disabled_translations_filter), "
             "ppns AS (SELECT ppn FROM keyword_translations "
                  "WHERE " + search_clause +
                  "language_code='ger' AND status='reliable' AND ppn NOT IN (SELECT * FROM filter_ppns) "
                  "ORDER BY translation LIMIT " + offset +  ", " + std::to_string(ENTRIES_PER_PAGE) + "),"
             "result_set AS (SELECT * FROM keywords_newest WHERE ppn IN (SELECT * FROM ppns))"
             "SELECT l.ppn, l.translation, l.language_code, l.gnd_code, l.status, l.translator, l.german_updated, "
                     "l.priority_entry, l.translation_disabled FROM "
             "result_set AS l INNER JOIN result_set AS k ON k.language_code='ger' AND k.status='reliable' AND "
             "k.ppn=l.ppn AND l.status!='reliable_synonym' AND l.status !='unreliable_synonym' "
             " WHERE l.language_code IN (" + GetQuotedDisplayLanguagesAsString(translator_languages, additional_view_languages) + ", 'ger')");

    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(create_result_with_limit, &db_connection));

    std::vector<std::string> language_codes(GetLanguageCodes(db_connection));

    std::vector<std::string> display_languages;
    GetDisplayLanguages(&display_languages, translator_languages, additional_view_languages, KEYWORDS, show_macs_col, show_wikidata_col,
                        show_disable_translation_col);
    *headline = "<th>" + StringUtil::Join(display_languages, "</th><th>") + "</th>";
    if (result_set.empty())
        return;

    std::vector<std::string> row_values(display_languages.size());
    std::string current_ppn;


    while (const auto db_row = result_set.getNextRow()) {
        // Add new entries as long as there is a single PPN
        std::string ppn(db_row["ppn"]);
        std::string translation(db_row["translation"]);
        std::string language_code(db_row["language_code"]);
        std::string status(db_row["status"]);
        std::string translator(db_row["translator"]);
        std::string gnd_code(db_row["gnd_code"]);
        std::string german_updated(db_row["german_updated"]);
        std::string priority_entry(db_row["priority_entry"]);
        std::string disabled_entry(db_row["translation_disabled"]);
        if (current_ppn != ppn) {
            if (not current_ppn.empty())
                rows->emplace_back(StringUtil::Join(row_values, ""));

            current_ppn = ppn;
            row_values.clear();
            row_values.resize(display_languages.size(), "<td style=\"background-color:lightgrey\"></td>");
            for (auto translator_language : translator_languages) {
                int index(GetColumnIndexForColumnHeading(display_languages, row_values, translator_language));
                if (index != NO_INDEX)
                    row_values[index] = (translator_language == "ger") ? CreateNonEditableRowEntry("")
                                                                       : CreateEditableRowEntry(current_ppn, "", translator_language,
                                                                                                "keyword_translations", "", gnd_code);
            }
            // Insert Synonyms
            std::vector<std::string> synonyms;
            GetSynonymsForGNDCode(db_connection, gnd_code, &synonyms);
            int synonym_index(GetColumnIndexForColumnHeading(display_languages, row_values, SYNONYM_COLUMN_DESCRIPTOR));
            if (synonym_index == NO_INDEX)
                continue;
            row_values[synonym_index] = CreateNonEditableSynonymEntry(synonyms, "<br/>");

            // Insert MACS Translations display table
            if (show_macs_col) {
                std::vector<std::string> macs_translations;
                GetMACSTranslationsForGNDCode(db_connection, gnd_code, &macs_translations);
                int macs_index(GetColumnIndexForColumnHeading(display_languages, row_values, MACS_COLUMN_DESCRIPTOR));
                if (macs_index == NO_INDEX)
                    continue;
                row_values[macs_index] = CreateNonEditableSynonymEntry(macs_translations, "<br/>");
            }


            // Insert Wikidata translations
            if (show_wikidata_col) {
                std::vector<TranslationLangAndWikiID> wikidata_translations;
                GetWikidataTranslationsForGNDCode(db_connection, gnd_code, &wikidata_translations, translator_languages,
                                                  additional_view_languages);
                int wikidata_index(GetColumnIndexForColumnHeading(display_languages, row_values, WIKIDATA_COLUMN_DESCRIPTOR));
                if (wikidata_index == NO_INDEX)
                    continue;
                row_values[wikidata_index] = CreateNonEditableWikidataEntry(wikidata_translations);
            }

            // Insert Disable Translations Selector
            if (show_disable_translation_col) {
                int disabled_translations_index(
                    GetColumnIndexForColumnHeading(display_languages, row_values, DISABLE_TRANSLATION_COLUMN_DESCRIPTOR));
                if (disabled_translations_index == NO_INDEX)
                    continue;
                row_values[disabled_translations_index] =
                    CreateEditableDisableTranslationEntry(current_ppn, TranslationDisabledEntryToBool(disabled_entry));
            }
        }

        int index(GetColumnIndexForColumnHeading(display_languages, row_values, language_code));
        if (index == NO_INDEX)
            continue;
        if (IsTranslatorLanguage(translator_languages, language_code)) {
            // We can have several translations in one language, i.e. from MACS, IxTheo (reliable) or translated by this tool (new)
            // Since we are iteratring over a single column, make sure sure we select the correct translation (reliable or new)
            if (IsEmptyEntryWithoutTranslator(row_values[index]) or status == "new" or status == "reliable") {
                if (language_code == "ger")
                    row_values[index] = (german_updated == "1" or priority_entry == "1")
                                            ? CreateNonEditableHighlightHintEntry(translation, gnd_code, use_subject_link)
                                            : CreateNonEditableHintEntry(translation, gnd_code, use_subject_link);
                // 20220131: for community version changes in "final" translations shall be possible
                else if (status == "reliable")
                    row_values[index] =
                        CreateEditableRowEntry(current_ppn, translation, language_code, "keyword_translations", translator, gnd_code);
                else
                    row_values[index] =
                        CreateEditableRowEntry(current_ppn, translation, language_code, "keyword_translations", translator, gnd_code);
            }
        } else if (language_code == "ger") {
            // Use a special display mode for values that must be highlighted
            row_values[index] = (german_updated == "1" or priority_entry == "1")
                                    ? CreateNonEditableHighlightHintEntry(translation, gnd_code, use_subject_link)
                                    : CreateNonEditableHintEntry(translation, gnd_code, use_subject_link);
        } else if (language_code == "eng") {
            // Special case for colliding English unaltered MACS and Ixtheo translations from authortity data
            if ((row_values[index] != CreateNonEditableRowEntry("")) and status == "unreliable")
                continue;
            row_values[index] = CreateNonEditableRowEntry(translation);
        } else
            row_values[index] = CreateNonEditableRowEntry(translation);
    }
    rows->emplace_back(StringUtil::Join(row_values, ""));
}


void GenerateDirectJumpTable(std::vector<std::string> * const jump_table, enum Category category = KEYWORDS,
                             const bool filter_untranslated = false, const std::string &lang_untranslated = ALL_SUPPORTED_LANGUAGES) {
    for (char ch('A'); ch <= 'Z'; ++ch) {
        // We use buttons an style them as link conform to post semantics
        std::string post_link(
         R"END(<form action="/cgi-bin/translator" method="POST">
            <button type="submit" class="link-button">)END" + std::string(1,ch) + "</button>"
         R"END(<input type="hidden" name="lookfor" value=")END" + std::string(1,ch) + "\">"
         R"END(<input type="hidden" name="target" value=")END" + (category == VUFIND ? "vufind" : "keywords") + "\">"
         R"END(<input type="hidden" name="filter_untranslated" value=)END" + (filter_untranslated ? " checked" : "") + ">"
         R"END(<input type="hidden" name="lang_untranslated" value=)END" + lang_untranslated + ">"
         "</form>");
        jump_table->emplace_back("<td style=\"border:none;\">" + post_link + "</td>");
    }
}


unsigned GetTotalNumberOfEntries(DbConnection &db_connection, enum Category category) {
    std::string query(
        category == VUFIND
            ? "SELECT COUNT(DISTINCT token) AS number_total FROM vufind_translations"
            : "SELECT COUNT(DISTINCT ppn) AS number_total FROM keyword_translations WHERE language_code='ger' AND status='reliable'");

    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(query, &db_connection));
    if (result_set.size() != 1)
        LOG_ERROR("Invalid number of rows when querying total number of entries");
    return std::atoi(result_set.getNextRow()["number_total"].c_str());
}


unsigned GetNumberOfTranslatedKeywordEntriesForLanguage(DbConnection &db_connection, const std::string &language,
                                                        const std::vector<std::string> &translator_languages) {
    const std::string query(
         "WITH keywords_newest AS (SELECT * FROM keyword_translations WHERE next_version_id IS NULL),"
         "translated_ppns AS (" +
               GetTranslatedPPNsFilterQuery(true /* constant here */, language, translator_languages)
               + ") "
         "SELECT COUNT(DISTINCT ppn) AS number_translated FROM translated_ppns");
    LOG_WARNING("PPPP" + query);
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(query, &db_connection));
    if (result_set.size() != 1)
        LOG_ERROR("Invalid number of rows when querying translated number of entries for Keywords");
    return std::atoi(result_set.getNextRow()["number_translated"].c_str());
}


unsigned GetNumberOfTranslatedVuFindEntriesForLanguage(DbConnection &db_connection, const std::string &language,
                                                       const std::vector<std::string> &translator_languages) {
    const std::string query(
            "WITH vufind_newest AS (SELECT * FROM vufind_translations WHERE next_version_id IS NULL),"
            "translated_tokens AS (" +
                GetTranslatedTokensFilterQuery(true /* constant here */, language, translator_languages)
                + ") "
            "SELECT COUNT(DISTINCT token) AS number_translated FROM translated_tokens");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(query, &db_connection));
    if (result_set.size() != 1)
        LOG_ERROR("Invalid number of rows when querying translated number of entries for VuFind");
    return std::atoi(result_set.getNextRow()["number_translated"].c_str());
}


bool GetNumberOfUntranslatedByLanguage(DbConnection &db_connection, enum Category category, const std::string &language,
                                       std::vector<std::string> &translator_languages_foreign, int * const number_untranslated,
                                       int * const number_total) {
    if (category == VUFIND) {
        *number_total = GetTotalNumberOfEntries(db_connection, VUFIND);
        const unsigned number_translated =
            GetNumberOfTranslatedVuFindEntriesForLanguage(db_connection, language, translator_languages_foreign);
        *number_untranslated = *number_total - number_translated;
        return true;
    }

    if (category == KEYWORDS) {
        *number_total = GetTotalNumberOfEntries(db_connection, KEYWORDS);
        const unsigned number_translated =
            GetNumberOfTranslatedKeywordEntriesForLanguage(db_connection, language, translator_languages_foreign);
        *number_untranslated = *number_total - number_translated;
        return true;
    }

    LOG_ERROR("Invalid category");
}


void ShowFrontPage(DbConnection &db_connection, const std::string &lookfor, const std::string &offset, const std::string &target,
                   const std::string translator, const std::vector<std::string> &translator_languages,
                   const std::vector<std::string> &additional_view_languages, const bool filter_untranslated,
                   const std::string &lang_untranslated, const bool show_macs_col, const bool use_subject_link,
                   const bool show_wikidata_col, const bool show_disable_translation_col) {
    Template::Map names_to_values_map;
    std::vector<std::string> rows;
    std::string headline;
    std::vector<std::string> jump_entries_keywords;
    GenerateDirectJumpTable(&jump_entries_keywords, KEYWORDS, filter_untranslated, lang_untranslated);
    names_to_values_map.insertArray("direct_jump_keywords", jump_entries_keywords);
    std::vector<std::string> jump_entries_vufind;
    GenerateDirectJumpTable(&jump_entries_vufind, VUFIND, filter_untranslated, lang_untranslated);
    names_to_values_map.insertArray("direct_jump_vufind", jump_entries_vufind);
    names_to_values_map.insertScalar("translator", translator);

    if (target == "vufind")
        GetVuFindTranslationsAsHTMLRowsFromDatabase(db_connection, lookfor, offset, &rows, &headline, translator_languages,
                                                    additional_view_languages, filter_untranslated, lang_untranslated);
    else if (target == "keywords")
        GetKeyWordTranslationsAsHTMLRowsFromDatabase(db_connection, lookfor, offset, &rows, &headline, translator_languages,
                                                     additional_view_languages, filter_untranslated, lang_untranslated, show_macs_col,
                                                     use_subject_link, show_wikidata_col, show_disable_translation_col);
    else
        ShowErrorPageAndDie("Error - Invalid Target", "No valid target selected");

    names_to_values_map.insertArray("vufind_token_row", rows);
    names_to_values_map.insertScalar("vufind_token_table_headline", headline);

    names_to_values_map.insertArray("keyword_row", rows);
    names_to_values_map.insertScalar("keyword_table_headline", headline);

    names_to_values_map.insertScalar("lookfor", lookfor);
    names_to_values_map.insertScalar("prev_offset", std::to_string(std::max(0, std::stoi(offset) - ENTRIES_PER_PAGE)));
    names_to_values_map.insertScalar("next_offset", std::to_string(std::stoi(offset) + ENTRIES_PER_PAGE));

    names_to_values_map.insertScalar("current_offset", offset);

    names_to_values_map.insertScalar("target_language_code", "");
    names_to_values_map.insertScalar("target_translation_scope", target);
    names_to_values_map.insertScalar("filter_untranslated", filter_untranslated ? "checked" : "");

    names_to_values_map.insertScalar("lang_untranslated", lang_untranslated);

    std::vector<std::string> translator_languages_foreign;
    std::copy_if(translator_languages.begin(), translator_languages.end(), std::back_inserter(translator_languages_foreign),
                 [](const std::string &lang) { return lang != "ger"; });
    names_to_values_map.insertArray("translator_languages_foreign", translator_languages_foreign);

    int number_untranslated, number_total;
    bool success_number_translated =
        GetNumberOfUntranslatedByLanguage(db_connection, target == "vufind" ? VUFIND : KEYWORDS, lang_untranslated,
                                          translator_languages_foreign, &number_untranslated, &number_total);

    names_to_values_map.insertScalar(
        "number_untranslated", success_number_translated ? std::to_string(number_untranslated) + "/" + std::to_string(number_total) : "");

    std::ifstream translate_html(UBTools::GetTuelibPath() + "translate_chainer/translation_front_page.html", std::ios::binary);
    Template::ExpandTemplate(translate_html, std::cout, names_to_values_map);
}


void GetAdditionalViewLanguages(const IniFile &ini_file, std::vector<std::string> * const additional_view_languages,
                                const std::string &translator) {
    const std::string ini_additional_view_languages(ini_file.getString(ADDITIONAL_VIEW_LANGUAGES, translator, ""));
    StringUtil::Split(ini_additional_view_languages, ',', additional_view_languages, /*n suppress_empty_components = */ true);
    std::for_each(additional_view_languages->begin(), additional_view_languages->end(),
                  [](std::string &additional_view_language) { StringUtil::TrimWhite(&additional_view_language); });
}


void GetTableForQuery(DbConnection &db_connection, std::vector<std::string> * const rows, const std::string &query,
                      std::vector<std::string> &display_languages, enum Category category) {
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
        row_values[index] = "<td>" + HtmlUtil::HtmlEscape(db_row["translation"]) + "</td>";

    } while ((db_row = result_set.getNextRow()));
    rows->emplace_back(StringUtil::Join(row_values, ""));
}


bool AssembleMyTranslationsData(DbConnection &db_connection, const IniFile &ini_file, Template::Map * const names_to_values_map,
                                const std::string &translator) {
    // Insert Translator
    names_to_values_map->insertScalar("translator", translator);

    // Get Translator Languages
    std::vector<std::string> translator_languages;
    GetTranslatorLanguages(ini_file, translator, &translator_languages);

    std::vector<std::string> display_languages(translator_languages);
    if (std::find(display_languages.begin(), display_languages.end(), "ger") == display_languages.end())
        display_languages.emplace(display_languages.begin(), "ger");

    // Get Vufind Translations
    const std::string vufind_query(
        "SELECT token, translation, language_code, translator FROM vufind_translations "
        "WHERE next_version_id IS NULL AND token IN (SELECT * FROM (SELECT token FROM vufind_translations WHERE "
        "translator='"
        + translator + "') as t) ORDER BY token, language_code;");

    std::vector<std::string> vufind_rows;
    GetTableForQuery(db_connection, &vufind_rows, vufind_query, display_languages, VUFIND);
    names_to_values_map->insertArray("vufind_translations", vufind_rows);

    // Get Keyword Translations
    const std::string keyword_query(
        "SELECT l.ppn, l.translation, l.language_code, l.translator FROM "
        "keyword_translations AS k INNER JOIN keyword_translations AS l ON "
        "k.language_code='ger' AND k.status='reliable' AND k.ppn=l.ppn AND "
        "l.status!='reliable_synonym' AND l.status != 'unreliable_synonym'"
        " AND k.next_version_id IS NULL"
        " AND l.ppn IN (SELECT ppn from keyword_translations WHERE translator='"
        + translator + "') ORDER BY k.translation;");

    std::vector<std::string> keyword_rows;
    GetTableForQuery(db_connection, &keyword_rows, keyword_query, display_languages, KEYWORDS);
    names_to_values_map->insertArray("keyword_translations", keyword_rows);
    return true;
}


void MailMyTranslations(DbConnection &db_connection, const IniFile &ini_file, const std::string translator) {
    Template::Map names_to_values_map;
    if (unlikely(not AssembleMyTranslationsData(db_connection, ini_file, &names_to_values_map, translator)))
        LOG_ERROR("Could not send mail");

    // Expand Template
    std::stringstream mail_content;
    std::ifstream mytranslations_template(UBTools::GetTuelibPath() + "translate_chainer/mytranslations_template.msg");
    Template::ExpandTemplate(mytranslations_template, mail_content, names_to_values_map);

    // Get Mail address
    const std::string recipient(ini_file.getString(EMAIL_SECTION, translator, ""));
    if (recipient.empty())
        return;

    if (unlikely(not EmailSender::SimplerSendEmail("no-reply@ub.uni-tuebingen.de", { recipient }, "Your IxTheo Translations",
                                                   mail_content.str(), EmailSender::DO_NOT_SET_PRIORITY, EmailSender::HTML)))
        LOG_ERROR("Could not send mail");
}


void SaveUserState(DbConnection &db_connection, const std::string &translator, const std::string &translation_target,
                   const std::string &lookfor, const std::string &offset, const bool filter_untranslated) {
    const std::string save_statement("INSERT INTO translators (translator, translation_target, "
                                     + std::string(filter_untranslated ? "filtered_offset" : "offset") + ", "
                                     + std::string(filter_untranslated ? "filtered_lookfor" : "lookfor") + ") " + "VALUES ('" + translator
                                     + "', '" + translation_target + "', '" + offset + "', '" + lookfor + "') ON DUPLICATE KEY UPDATE "
                                     + std::string(filter_untranslated ? "filtered_lookfor" : "lookfor") + "='" + lookfor + "', "
                                     + std::string(filter_untranslated ? "filtered_offset" : "offset") + "='" + offset + "';");
    db_connection.queryOrDie(save_statement);
}


void RestoreUserState(DbConnection &db_connection, const std::string &translator, const std::string &translation_target,
                      std::string * const lookfor, std::string * const offset, const bool filter_untranslated) {
    const std::string lookfor_type(filter_untranslated ? "filtered_lookfor" : "lookfor");
    const std::string offset_type(filter_untranslated ? "filtered_offset" : "offset");
    const std::string restore_statement("SELECT " + lookfor_type + ", " + offset_type + " FROM translators WHERE translator='" + translator
                                        + "' AND translation_target='" + translation_target + "';");
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(restore_statement, &db_connection));
    if (result_set.empty())
        return;

    DbRow row(result_set.getNextRow());
    *lookfor = row[lookfor_type];
    const std::string offset_candidate(row[offset_type]);
    *offset = (not offset_candidate.empty()) ? offset_candidate : "0";
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

    const std::string translator(GetTranslatorOrEmptyString());

    if (translator.empty()) {
        ShowErrorPageAndDie("Error - No Valid User", "No valid user selected");
    }

    bool show_macs_col = IsMacsColumnVisible(ini_file);
    bool show_wikidata_col = IsWikidataColumnVisible(ini_file);
    bool use_subject_link = IsUseSubjectSearchLink(ini_file);
    bool show_disable_translation_col = IsDisableTranslationColVisible(ini_file, translator);

    // Read in the views for the respective users
    std::vector<std::string> translator_languages;
    GetTranslatorLanguages(ini_file, translator, &translator_languages);
    if (translator_languages.size() == 0)
        ShowErrorPageAndDie("Error - No languages", "No languages specified for user " + translator, "Contact your administrator");
    std::vector<std::string> additional_view_languages;
    GetAdditionalViewLanguages(ini_file, &additional_view_languages, translator);

    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";

    const std::string mail(WebUtil::GetCGIParameterOrDefault(cgi_args, "mail", ""));
    if (mail == "mytranslations")
        MailMyTranslations(db_connection, ini_file, translator);

    std::string lookfor(WebUtil::GetCGIParameterOrDefault(cgi_args, "lookfor", ""));
    std::string offset(WebUtil::GetCGIParameterOrDefault(cgi_args, "offset", "0"));
    const std::string translation_target(WebUtil::GetCGIParameterOrDefault(cgi_args, "target", "keywords"));
    const std::string save_action(WebUtil::GetCGIParameterOrDefault(cgi_args, "save_action", ""));
    const std::string filter_untranslated_value(WebUtil::GetCGIParameterOrDefault(cgi_args, "filter_untranslated", ""));
    const bool filter_untranslated(filter_untranslated_value == "checked");
    const std::string lang_untranslated(WebUtil::GetCGIParameterOrDefault(cgi_args, "lang_untranslated", "all"));
    if (save_action == "save")
        SaveUserState(db_connection, translator, translation_target, lookfor, offset, filter_untranslated);
    else if (save_action == "restore")
        RestoreUserState(db_connection, translator, translation_target, &lookfor, &offset, filter_untranslated);
    ShowFrontPage(db_connection, lookfor, offset, translation_target, translator, translator_languages, additional_view_languages,
                  filter_untranslated, lang_untranslated, show_macs_col, use_subject_link, show_wikidata_col, show_disable_translation_col);

    return EXIT_SUCCESS;
}
