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
#include <unordered_set>
#include <vector>
#include "Compiler.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


const std::set<std::string> ControlNumberGuesser::EMPTY_SET;


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


ControlNumberGuesser::~ControlNumberGuesser() {
    delete title_cursor_; delete author_cursor_; delete titles_db_; delete authors_db_;

    std::unordered_set<std::set<std::string> *> already_deleted;
    for (auto &control_number_and_set_ptr : control_number_to_control_number_set_map_) {
        if (already_deleted.find(control_number_and_set_ptr.second) == already_deleted.end()) {
            delete control_number_and_set_ptr.second;
            already_deleted.emplace(control_number_and_set_ptr.second);
        }
    }
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
    {
        LOG_DEBUG("no entries found for normalised title");
        return { };
    }

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


bool ControlNumberGuesser::getNextTitle(std::string * const title, std::set<std::string> * const control_numbers) const {
    if (title_cursor_ == nullptr) {
        title_cursor_ = titles_db_->cursor();
        title_cursor_->jump();
    }

    std::string concatenated_control_numbers;
    if (title_cursor_->get(title, &concatenated_control_numbers, /* Move cursor to the next record */true)) {
        StringUtil::Split(concatenated_control_numbers, '\0', control_numbers);
        return true;
    } else {
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


void ControlNumberGuesser::FindDups(const std::unordered_map<std::string, std::set<std::string>> &title_to_control_numbers_map,
                                    const std::unordered_map<std::string, std::set<std::string>> &control_number_to_authors_map) const
{
    for (const auto &title_and_control_numbers : title_to_control_numbers_map) {
        if (title_and_control_numbers.second.size() < 2)
            continue;

        // Collect all control numbers for all authors of the current title:
        std::map<std::string, std::set<std::string>> author_to_control_numbers_map;
        for (const auto &control_number : title_and_control_numbers.second) {
            const auto control_number_and_authors(control_number_to_authors_map.find(control_number));
            if (control_number_and_authors == control_number_to_authors_map.cend())
                continue;

            for (const auto &author : control_number_and_authors->second) {
                auto author_and_control_numbers(author_to_control_numbers_map.find(author));
                if (author_and_control_numbers == author_to_control_numbers_map.end())
                    author_to_control_numbers_map[author] = std::set<std::string>{ control_number };
                else
                    author_and_control_numbers->second.emplace(control_number);
            }
        }

        // Record those cases where we found multiple control numbers for the same author for a single title:
        std::unordered_set<std::string> already_processed_control_numbers;
        for (const auto &author_and_control_numbers : author_to_control_numbers_map) {
            if (author_and_control_numbers.second.size() >= 2) {
                bool skip_author(false);

                // We may have multiple authors for the same work but only wish to report each duplicate work once:
                for (const auto &control_number : author_and_control_numbers.second) {
                    if (already_processed_control_numbers.find(control_number) != already_processed_control_numbers.cend()) {
                        skip_author = true;
                        break;
                    }
                }

                if (not skip_author) {
                    std::set<std::string> *new_set(new std::set<std::string>());
                    for (const auto &control_number : author_and_control_numbers.second) {
                        already_processed_control_numbers.emplace(control_number);
                        new_set->emplace(control_number);
                        control_number_to_control_number_set_map_[control_number] = new_set;
                    }
                }
            }
        }
    }
}


void ControlNumberGuesser::InitControlNumberToControlNumberSetMap() const {
    std::unordered_map<std::string, std::set<std::string>> title_to_control_numbers_map;
    std::string title;
    std::set<std::string> control_numbers;
    while (getNextTitle(&title, &control_numbers))
        title_to_control_numbers_map.emplace(title, control_numbers);

    std::unordered_map<std::string, std::set<std::string>> control_number_to_authors_map;
    std::string author;
    while (getNextAuthor(&author, &control_numbers)) {
        for (const auto &control_number : control_numbers) {
            auto control_number_and_authors(control_number_to_authors_map.find(control_number));
            if (control_number_and_authors == control_number_to_authors_map.end())
                control_number_to_authors_map[control_number] = std::set<std::string>{ author };
            else
                control_number_and_authors->second.emplace(author);
        }
    }

    FindDups(title_to_control_numbers_map, control_number_to_authors_map);
}


const std::set<std::string> &ControlNumberGuesser::getControlNumberPartners(const std::string &control_number) const {
    if (control_number_to_control_number_set_map_.empty())
        InitControlNumberToControlNumberSetMap();

    const auto control_number_and_set_ptr(control_number_to_control_number_set_map_.find(control_number));
    if (control_number_and_set_ptr == control_number_to_control_number_set_map_.cend())
        return EMPTY_SET;
    return *control_number_and_set_ptr->second;
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
        trimmed_author_name = StringUtil::TrimWhite(trimmed_author_name.substr(comma_pos + 1) + " "
                                                    + trimmed_author_name.substr(0, comma_pos));
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
    StringUtil::Split(normalised_author_name, ' ', &parts);
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
