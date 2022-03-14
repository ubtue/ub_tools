/** \file   ControlNumberGuesser.cc
 *  \brief  Implementation of the ControlNumberGuesser class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
 *
 *  \copyright 2018-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "ControlNumberGuesser.h"
#include <algorithm>
#include <iterator>
#include <unordered_set>
#include <vector>
#include "Compiler.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UBTools.h"


const std::set<std::string> ControlNumberGuesser::EMPTY_SET;
const std::string ControlNumberGuesser::DATABASE_PATH(UBTools::GetTuelibPath() + "control_number_guesser.sq3");
const std::string ControlNumberGuesser::INSTALLER_SCRIPT_PATH("/usr/local/ub_tools/cpp/data/installer/control_number_guesser.sql");


ControlNumberGuesser::~ControlNumberGuesser() {
    delete db_transaction_;
}


void ControlNumberGuesser::clearDatabase() {
    db_connection_.sqliteResetDatabase(INSTALLER_SCRIPT_PATH);
}


void ControlNumberGuesser::beginUpdate() {
    db_transaction_ = new DbTransaction(&db_connection_);
}


void ControlNumberGuesser::endUpdate() {
    delete db_transaction_;
    db_transaction_ = nullptr;
}


void ControlNumberGuesser::insertTitle(const std::string &title, const std::string &control_number) {
    const auto normalised_title(NormaliseTitle(title));
    LOG_DEBUG("normalised_title=\"" + normalised_title + "\".");
    if (unlikely(normalised_title.empty()))
        LOG_WARNING("Empty normalised title in record w/ control number: " + control_number);
    else
        db_connection_.queryOrDie("INSERT OR IGNORE INTO normalised_titles (title, control_number) VALUES('" + db_connection_.escapeString(normalised_title) + "', '" + control_number + "')");
}


void ControlNumberGuesser::insertAuthors(const std::set<std::string> &authors, const std::string &control_number) {
    for (const auto &author : authors) {
        const auto normalised_author_name(TextUtil::UTF8ToLower(NormaliseAuthorName(author)));
        LOG_DEBUG("normalised_author_name=\"" + normalised_author_name + "\".");
        if (unlikely(normalised_author_name.empty()))
            LOG_WARNING("Empty normalised author in record w/ control number: " + control_number + " (orig. \"" + author + "\")");
        else
            db_connection_.queryOrDie("INSERT OR IGNORE INTO normalised_authors (author, control_number) VALUES('" + db_connection_.escapeString(normalised_author_name) + "', '" + control_number + "')");
    }
}


void ControlNumberGuesser::insertYear(const std::string &year, const std::string &control_number) {
    if (unlikely(control_number.length() > MAX_CONTROL_NUMBER_LENGTH))
        LOG_ERROR("\"" + control_number + "\" is too large to fit!");

    db_connection_.queryOrDie("INSERT OR IGNORE INTO publication_year (year, control_number) VALUES('" + year + "', '" + control_number + "')");
}


void ControlNumberGuesser::insertDOI(const std::string &doi, const std::string &control_number) {
    std::string normalised_doi;
    MiscUtil::NormaliseDOI(doi, &normalised_doi);

    LOG_DEBUG("normalised_doi=\"" + normalised_doi + "\".");
    if (unlikely(normalised_doi.empty()))
        LOG_WARNING("Empty normalised doi in record w/ control number: " + control_number + ": " + doi);
    else
        db_connection_.queryOrDie("INSERT OR IGNORE INTO doi (doi, control_number) VALUES('" + normalised_doi + "', '" + control_number + "')");
}


void ControlNumberGuesser::insertISSN(const std::string &issn, const std::string &control_number) {
    std::string normalised_issn;
    MiscUtil::NormaliseISSN(issn, &normalised_issn);

    LOG_DEBUG("normalised_issn=\"" + normalised_issn + "\".");
    if (unlikely(normalised_issn.empty()))
        LOG_WARNING("Empty normalised ISSN in record w/ control number: " + control_number + ": " + issn);
    else
        db_connection_.queryOrDie("INSERT OR IGNORE INTO issn (issn, control_number) VALUES('" + normalised_issn + "', '" + control_number + "')");
}


void ControlNumberGuesser::insertISBN(const std::string &isbn, const std::string &control_number) {
    std::string normalised_isbn;
    MiscUtil::NormaliseISBN(isbn, &normalised_isbn);

    LOG_DEBUG("normalised_isbn=\"" + normalised_isbn + "\".");
    if (unlikely(normalised_isbn.empty()))
        LOG_WARNING("Empty normalised ISBN in record w/ control number: " + control_number + ": " + isbn);
    else
        db_connection_.queryOrDie("INSERT OR IGNORE INTO isbn (isbn, control_number) VALUES('" + normalised_isbn + "', '" + control_number + "')");
}


std::set<std::string> ControlNumberGuesser::getGuessedControlNumbers(const std::string &title, const std::set<std::string> &authors,
                                                                     const std::string &year, const std::set<std::string> &dois,
                                                                     const std::set<std::string> &issns,
                                                                     const std::set<std::string> &isbns) const {
    std::set<std::string> control_numbers;

    for (const auto &doi : dois) {
        std::set<std::string> doi_control_numbers;
        lookupDOI(doi, &doi_control_numbers);
        control_numbers.insert(doi_control_numbers.cbegin(), doi_control_numbers.cend());
    }
    if (not control_numbers.empty())
        return control_numbers;

    for (const auto &issn : issns) {
        std::set<std::string> issn_control_numbers;
        lookupISSN(issn, &issn_control_numbers);
        control_numbers.insert(issn_control_numbers.cbegin(), issn_control_numbers.cend());
    }
    if (not control_numbers.empty())
        return control_numbers;

    for (const auto &isbn : isbns) {
        std::set<std::string> isbn_control_numbers;
        lookupISBN(isbn, &isbn_control_numbers);
        control_numbers.insert(isbn_control_numbers.cbegin(), isbn_control_numbers.cend());
    }
    if (not control_numbers.empty())
        return control_numbers;

    const auto normalised_title(NormaliseTitle(title));
    std::set<std::string> title_control_numbers, all_author_control_numbers, doi_control_numbers;
    std::set<std::string> year_control_numbers;

    lookupTitle(normalised_title, &title_control_numbers);
    if (title_control_numbers.empty()) {
        LOG_DEBUG("no entries found for normalised title \"" + normalised_title + "\"");
        return {};
    }

    std::set<std::string> normalised_authors;
    for (const auto &author : authors) {
        const auto normalised_author(NormaliseAuthorName(author));
        normalised_authors.insert(normalised_author);

        std::set<std::string> author_control_numbers;
        lookupAuthor(normalised_author, &author_control_numbers);
        all_author_control_numbers.insert(author_control_numbers.begin(), author_control_numbers.end());
    }

    if (all_author_control_numbers.empty()) {
        LOG_DEBUG("no entries found for normalised authors \"" + StringUtil::Join(normalised_authors, ',') + "\"");
        return {};
    }

    control_numbers = MiscUtil::Intersect(title_control_numbers, all_author_control_numbers);

    if (year.empty())
        return control_numbers;

    lookupYear(year, &year_control_numbers);
    return MiscUtil::Intersect(control_numbers, year_control_numbers);
}


void ControlNumberGuesser::getTitlesAndControlnumbers(std::unordered_map<std::string, std::set<std::string>> * const title_to_control_numbers_map) {
    title_to_control_numbers_map->clear();
    db_connection_.queryOrDie("SELECT * FROM normalised_titles");
    auto query_result(db_connection_.getLastResultSet());
    while (const auto db_row = query_result.getNextRow()) {
        std::string title = db_row["title"];
        std::string control_number = db_row["control_number"];
        const auto title_and_control_number(title_to_control_numbers_map->find(title));
        if (title_and_control_number == title_to_control_numbers_map->end())
            (*title_to_control_numbers_map)[title] = std::set<std::string>{ control_number };
        else
            title_and_control_number->second.emplace(control_number);
    }
}


void ControlNumberGuesser::getControlnumbersAndAuthors(std::unordered_map<std::string, std::set<std::string>> * const control_numbers_to_author_map) {
    control_numbers_to_author_map->clear();
    db_connection_.queryOrDie("SELECT * FROM normalised_authors");
    auto query_result(db_connection_.getLastResultSet());
    while (const auto db_row = query_result.getNextRow()) {
        std::string author = db_row["author"];
        std::string control_number = db_row["control_number"];
        const auto control_number_and_author(control_numbers_to_author_map->find(control_number));
        if (control_number_and_author == control_numbers_to_author_map->end())
            (*control_numbers_to_author_map)[control_number] = std::set<std::string>{ author };
        else
            control_number_and_author->second.emplace(author);
    }
}


void ControlNumberGuesser::lookupTitle(const std::string &title, std::set<std::string> * const control_numbers) const {
    control_numbers->clear();

    const auto normalised_title(TextUtil::UTF8ToLower(NormaliseTitle(title)));
    lookupControlNumber("normalised_titles", "title", normalised_title, control_numbers);
}


void ControlNumberGuesser::lookupAuthor(const std::string &author_name, std::set<std::string> * const control_numbers) const {
    control_numbers->clear();

    const auto normalised_author_name(TextUtil::UTF8ToLower(NormaliseAuthorName(author_name)));
    lookupControlNumber("normalised_authors", "author", normalised_author_name, control_numbers);
}


void ControlNumberGuesser::lookupYear(const std::string &year, std::set<std::string> * const control_numbers) const {
    control_numbers->clear();

    lookupControlNumber("publication_year", "year", year, control_numbers);
}


void ControlNumberGuesser::lookupDOI(const std::string &doi, std::set<std::string> * const control_numbers) const {
    control_numbers->clear();

    std::string normalised_doi;
    MiscUtil::NormaliseDOI(doi, &normalised_doi);
    lookupControlNumber("doi", "doi", normalised_doi, control_numbers);
}


void ControlNumberGuesser::lookupISSN(const std::string &issn, std::set<std::string> * const control_numbers) const {
    control_numbers->clear();

    std::string normalised_issn;
    MiscUtil::NormaliseISSN(issn, &normalised_issn);
    lookupControlNumber("issn", "issn", normalised_issn, control_numbers);
}


void ControlNumberGuesser::lookupISBN(const std::string &isbn, std::set<std::string> * const control_numbers) const {
    control_numbers->clear();

    std::string normalised_isbn;
    MiscUtil::NormaliseISBN(isbn, &normalised_isbn);
    lookupControlNumber("isbn", "isbn", normalised_isbn, control_numbers);
}


std::string ControlNumberGuesser::NormaliseTitle(const std::string &title) {
    std::wstring wtitle;
    if (unlikely(not TextUtil::UTF8ToWCharString(title, &wtitle)))
        LOG_ERROR("failed to convert \"" + title + "\" to a wide character string!");

    std::wstring normalised_title;
    bool space_separator_seen(true);
    for (const auto ch : wtitle) {
        if (TextUtil::IsPunctuationCharacter(ch) or ch == '-' or TextUtil::IsSpace(ch)) {
            if (not space_separator_seen)
                normalised_title += ' ';
            space_separator_seen = true;
        } else {
            space_separator_seen = false;
            normalised_title += ch;
        }
    }
    if (not normalised_title.empty() and TextUtil::IsSpace(normalised_title.back()))
        normalised_title.resize(normalised_title.size() - 1);
    normalised_title = TextUtil::ExpandLigatures(normalised_title);

    normalised_title = TextUtil::RemoveDiacritics(normalised_title);
    normalised_title = TextUtil::NormaliseQuotationMarks(normalised_title);
    TextUtil::ToLower(&normalised_title);

    std::string utf8_normalised_title;
    if (unlikely(not TextUtil::WCharToUTF8String(normalised_title, &utf8_normalised_title)))
        LOG_ERROR("failed to convert a wstring to an UTF8 string!");

    return utf8_normalised_title;
}


std::string ControlNumberGuesser::NormaliseAuthorName(const std::string &author_name) {
    auto trimmed_author_name(StringUtil::TrimWhite(author_name));
    const auto comma_pos(trimmed_author_name.find(','));
    if (comma_pos != std::string::npos)
        trimmed_author_name =
            StringUtil::TrimWhite(trimmed_author_name.substr(comma_pos + 1) + " " + trimmed_author_name.substr(0, comma_pos));
    std::wstring wtrimmed_author_name;
    if (unlikely(not TextUtil::UTF8ToWCharString(trimmed_author_name, &wtrimmed_author_name)))
        LOG_ERROR("failed to convert trimmed_author_name to a wstring!");

    std::wstring normalised_author_name;
    bool space_seen(false);
    unsigned non_space_sequence_length(0);
    for (const wchar_t ch : wtrimmed_author_name) {
        if (ch == '.') {
            if (non_space_sequence_length == 1)
                normalised_author_name.resize(normalised_author_name.length() - 1);
            else
                normalised_author_name += ch;
            if (normalised_author_name.empty())
                space_seen = false;
            else {
                if (not TextUtil::IsSpace(normalised_author_name.back()))
                    normalised_author_name += ' ';
                space_seen = true;
            }
            non_space_sequence_length = 0;
        } else if (TextUtil::IsSpace(ch)) {
            if (not space_seen)
                normalised_author_name += ' ';
            space_seen = true;
            non_space_sequence_length = 0;
        } else {
            normalised_author_name += ch;
            space_seen = false;
            ++non_space_sequence_length;
        }
    }
    normalised_author_name = TextUtil::ExpandLigatures(TextUtil::RemoveDiacritics(normalised_author_name));

    // Only keep the first name and the last name:
    std::vector<std::wstring> parts;
    StringUtil::Split(normalised_author_name, ' ', &parts, /* suppress_empty_components = */ true);
    if (unlikely(parts.empty()))
        return "";
    normalised_author_name = parts.front();
    if (parts.size() > 1)
        normalised_author_name += L" " + parts.back();

    TextUtil::ToLower(&normalised_author_name);

    std::string utf8_normalised_author_name;
    if (unlikely(not TextUtil::WCharToUTF8String(normalised_author_name, &utf8_normalised_author_name)))
        LOG_ERROR("failed to convert normalised_author_name to a UTF8 string!");

    return utf8_normalised_author_name;
}


void ControlNumberGuesser::lookupControlNumber(const std::string &table, const std::string &column_name, const std::string &column_value,
                                               std::set<std::string> * const control_numbers) const {
    control_numbers->clear();
    db_connection_.queryOrDie("SELECT control_number FROM " + table + " WHERE " + column_name + "='"
                              + db_connection_.escapeString(column_value) + "'");

    auto query_result(db_connection_.getLastResultSet());
    while (const auto db_row = query_result.getNextRow()) {
        control_numbers->emplace(db_row["control_number"]);
    }
}


void ControlNumberGuesser::splitControlNumbers(const std::string &concatenated_control_numbers,
                                               std::unordered_set<std::string> * const control_numbers) const {
    size_t count(concatenated_control_numbers.size() / (MAX_CONTROL_NUMBER_LENGTH + 1 /* terminating pipe char */));
    control_numbers->reserve(count);
    for (unsigned i(0); i < count; ++i) {
        auto control_number(concatenated_control_numbers.substr(i * (MAX_CONTROL_NUMBER_LENGTH + 1), MAX_CONTROL_NUMBER_LENGTH));
        control_number.erase(std::remove(control_number.begin(), control_number.end(), '|'), control_number.end());
        control_numbers->emplace(control_number);
    }
}


unsigned ControlNumberGuesser::swapControlNumbers(const std::string &table_name, const std::string &primary_key,
                                                  const std::unordered_map<std::string, std::string> &old_to_new_map) {
    unsigned changed_row_count(0);
    db_connection_.queryOrDie("SELECT " + primary_key + ", control_numbers FROM " + table_name);
    DbResultSet result_set(db_connection_.getLastResultSet());

    while (const DbRow row = result_set.getNextRow()) {
        std::unordered_set<std::string> control_numbers;
        splitControlNumbers(row["control_numbers"], &control_numbers);

        std::map<std::string, std::string> replacements;
        for (const auto &control_number : control_numbers) {
            const auto old_and_new_control_numbers(old_to_new_map.find(control_number));
            if (old_and_new_control_numbers != old_to_new_map.cend())
                replacements[control_number] = old_and_new_control_numbers->second;
        }

        if (replacements.empty())
            continue;

        for (const auto &replacement : replacements) {
            control_numbers.erase(replacement.first);
            control_numbers.emplace(replacement.second);
        }

        db_connection_.queryOrDie("UPDATE " + table_name + " SET control_numbers='" + StringUtil::Join(control_numbers, '|') + "' WHERE "
                                  + primary_key + "=" + db_connection_.escapeAndQuoteString(row[primary_key]));

        ++changed_row_count;
    }

    return changed_row_count;
}
