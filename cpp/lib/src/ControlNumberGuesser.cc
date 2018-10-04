/** \file   ControlNumberGuesser.cc
 *  \brief  Implementation of the ControlNumberGuesser class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <vector>
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


static kyotocabinet::HashDB *CreateOrOpenKeyValueDB(const std::string &db_path) {
    auto db(new kyotocabinet::HashDB());
    if (not (db->open(db_path, kyotocabinet::HashDB::OWRITER | kyotocabinet::HashDB::OCREATE)))
        LOG_ERROR("failed to open or create \"" + db_path + "\"!");
    return db;
}


static const std::string MATCH_DB_PREFIX("/usr/local/var/lib/tuelib/normalised_");


ControlNumberGuesser::ControlNumberGuesser(const OpenMode open_mode) : title_cursor_(nullptr), author_cursor_(nullptr) {
    const std::string TITLES_DB_PATH(MATCH_DB_PREFIX + "titles.db");
    const std::string AUTHORS_DB_PATH(MATCH_DB_PREFIX + "authors.db");

    if (open_mode == CLEAR_DATABASES) {
        ::unlink(TITLES_DB_PATH.c_str());
        ::unlink(AUTHORS_DB_PATH.c_str());
    }

    titles_db_  = CreateOrOpenKeyValueDB(TITLES_DB_PATH);
    authors_db_ = CreateOrOpenKeyValueDB(AUTHORS_DB_PATH);
}


void ControlNumberGuesser::insertTitle(const std::string &title, const std::string &control_number) {
    const auto normalised_title(NormaliseTitle(title));
    LOG_DEBUG("normalised_title=\"" + normalised_title + "\".");
    if (unlikely(normalised_title.empty()))
        LOG_WARNING("Empty normalised title in record w/ control number: " + control_number);
    else {
        std::string control_numbers;
        if (titles_db_->get(normalised_title, &control_numbers)) {
            control_numbers += '\0';
            control_numbers += control_number;
        } else
            control_numbers = control_number;
        if (unlikely(not titles_db_->set(normalised_title, control_numbers)))
            LOG_ERROR("failed to insert normalised title into the database!");
    }
}


void ControlNumberGuesser::insertAuthors(const std::set<std::string> &authors, const std::string &control_number) {
    for (const auto author : authors) {
        const auto normalised_author_name(TextUtil::UTF8ToLower(NormaliseAuthorName(author)));
        LOG_DEBUG("normalised_author_name=\"" + normalised_author_name + "\".");
        std::string control_numbers;
        if (authors_db_->get(normalised_author_name, &control_numbers)) {
            control_numbers += '\0';
            control_numbers += control_number;
        } else
            control_numbers = control_number;
        if (unlikely(not authors_db_->set(normalised_author_name, control_numbers)))
            LOG_ERROR("failed to insert normalised author into the database!");
    }
}


std::set<std::string> ControlNumberGuesser::getGuessedControlNumbers(const std::string &title, const std::vector<std::string> &authors) const
{
    const auto normalised_title(NormaliseTitle(title));
    if (logger->getMinimumLogLevel() >= Logger::LL_DEBUG)
        LOG_DEBUG("in ControlNumberGuesser::getGuessedControlNumbers: normalised_title=\"" + normalised_title + "\".");
    std::string concatenated_title_control_numbers;
    std::vector<std::string> title_control_numbers;
    if (not titles_db_->get(normalised_title, &concatenated_title_control_numbers)
        or StringUtil::Split(concatenated_title_control_numbers, '\0', &title_control_numbers) == 0)
        return { };

    std::vector<std::string> all_author_control_numbers;
    for (const auto &author : authors) {
        const auto normalised_author(NormaliseAuthorName(author));
        if (logger->getMinimumLogLevel() >= Logger::LL_DEBUG)
            LOG_DEBUG("in ControlNumberGuesser::getGuessedControlNumbers: normalised_author=\"" + normalised_author + "\".");
        std::string concatenated_author_control_numbers;
        std::set<std::string> author_control_numbers;
        if (authors_db_->get(normalised_author, &concatenated_author_control_numbers)) {
            StringUtil::Split(concatenated_author_control_numbers, '\0', &author_control_numbers);
            for (const auto author_control_number : author_control_numbers)
                all_author_control_numbers.emplace_back(author_control_number);
        }
    }
    if (all_author_control_numbers.empty())
        return { };

    std::sort(title_control_numbers.begin(), title_control_numbers.end());
    std::sort(all_author_control_numbers.begin(), all_author_control_numbers.end());
    std::vector<std::string> common_control_numbers;
    std::set_intersection(title_control_numbers.begin(), title_control_numbers.end(),
                          all_author_control_numbers.begin(), all_author_control_numbers.end(),
                          std::back_inserter(common_control_numbers));

    return std::set<std::string>(common_control_numbers.cbegin(), common_control_numbers.cend());
}


bool ControlNumberGuesser::getNextTitle(std::string * const title, std::string * const control_number) const {
    if (title_cursor_ == nullptr) {
        title_cursor_ = titles_db_->cursor();
        title_cursor_->jump();
    }

    if (title_cursor_->get(title, control_number, /* Move cursor to the next record */true))
        return true;
    else {
        delete title_cursor_;
        title_cursor_ = nullptr;
        return false;
    }
}


bool ControlNumberGuesser::getNextAuthor(std::string * const author_name, std::set<std::string> * const control_numbers) const {
    if (author_cursor_ == nullptr) {
        author_cursor_ = authors_db_->cursor();
        author_cursor_->jump();
    }

    std::string concatenated_control_numbers;
    if (author_cursor_->get(author_name, &concatenated_control_numbers, /* Move cursor to the next record */true)) {
        StringUtil::Split(concatenated_control_numbers, '\0', control_numbers);
        return true;
    } else {
        delete author_cursor_;
        author_cursor_ = nullptr;
        return false;
    }
}


std::string ControlNumberGuesser::NormaliseTitle(const std::string &title) {
    std::wstring wtitle;
    if (unlikely(not TextUtil::UTF8ToWCharString(title, &wtitle)))
        LOG_ERROR("failed to convert \"" + title + "\" to a wide character string!");

    std::wstring normalised_title;
    bool space_separator_seen(true);
    for (const auto ch : wtitle) {
        if (TextUtil::IsGeneralPunctuationCharacter(ch) or ch == '-' or TextUtil::IsSpaceSeparatorCharacter(ch)) {
            if (not space_separator_seen)
                normalised_title += ' ';
            space_separator_seen = true;
        } else {
            space_separator_seen = false;
            normalised_title += ch;
        }
    }
    if (not normalised_title.empty() and TextUtil::IsSpaceSeparatorCharacter(normalised_title.back()))
        normalised_title.resize(normalised_title.size() - 1);

    TextUtil::ToLower(&normalised_title);

    std::string utf8_normalised_title;
    if (unlikely(not TextUtil::WCharToUTF8String(normalised_title, &utf8_normalised_title)))
        LOG_ERROR("failed to convert a wstring to an UTF8 string!");

    return utf8_normalised_title;
}


std::string ControlNumberGuesser::NormaliseAuthorName(const std::string &author_name) {
    auto trimmed_author_name = StringUtil::TrimWhite(author_name);
    const auto comma_pos(trimmed_author_name.find(','));
    if (comma_pos != std::string::npos)
        trimmed_author_name = StringUtil::TrimWhite(trimmed_author_name.substr(comma_pos + 1) + " "
                                                    + trimmed_author_name.substr(0, comma_pos));

    std::string normalised_author_name;
    bool space_seen(false);
    unsigned non_space_sequence_length(0);
    for (const char ch : trimmed_author_name) {
        switch (ch) {
        case '.':
            if (non_space_sequence_length == 1)
                normalised_author_name.resize(normalised_author_name.length() - 1);
            else
                normalised_author_name += ch;
            if (normalised_author_name.empty())
                space_seen = false;
            else {
                if (normalised_author_name.back() != ' ')
                    normalised_author_name += ' ';
                space_seen = true;
            }
            non_space_sequence_length = 0;
            break;
        case ' ':
            if (not space_seen)
                normalised_author_name += ' ';
            space_seen = true;
            non_space_sequence_length = 0;
            break;
        default:
            normalised_author_name += ch;
            space_seen = false;
            ++non_space_sequence_length;
        }
    }

    return TextUtil::UTF8ToLower(normalised_author_name);
}
