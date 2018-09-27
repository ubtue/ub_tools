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


ControlNumberGuesser::ControlNumberGuesser(const OpenMode open_mode) {
    const std::string TITLES_DB_PATH(MATCH_DB_PREFIX + "titles.db");
    const std::string AUTHORS_DB_PATH(MATCH_DB_PREFIX + "authors.db");

    if (open_mode == CLEAR_DATABASES) {
        ::unlink(TITLES_DB_PATH.c_str());
        ::unlink(AUTHORS_DB_PATH.c_str());
    }

    titles_db_  = CreateOrOpenKeyValueDB(TITLES_DB_PATH);
    authors_db_ = CreateOrOpenKeyValueDB(AUTHORS_DB_PATH);
}


static std::string NormaliseTitle(const std::string &title) {
    std::string normalised_title;
    bool space_seen(false);
    for (const char ch : title) {
        if (std::ispunct(ch) or std::isspace(ch)) {
            if (not space_seen)
                normalised_title += ' ';
            space_seen = true;
        } else {
            space_seen = false;
            normalised_title += ch;
        }
    }

    return StringUtil::TrimWhite(&normalised_title);
}


void ControlNumberGuesser::insertTitle(const std::string &title, const std::string &control_number) {
    auto normalised_title(TextUtil::UTF8ToLower(NormaliseTitle(title)));
    if (unlikely(normalised_title.empty()))
        LOG_WARNING("Empty normalised title in record w/ control number: " + control_number);
    else {
        std::string control_numbers;
        if (titles_db_->get(normalised_title, &control_numbers))
            control_numbers += "\0" + control_number;
        else
            control_numbers = control_number;
        if (unlikely(not titles_db_->set(normalised_title, control_numbers)))
            LOG_ERROR("failed to insert normalised title into the database!");
    }
}


static std::string NormaliseAuthorName(std::string author_name) {
    author_name = StringUtil::TrimWhite(author_name);
    const auto comma_pos(author_name.find(','));
    if (comma_pos != std::string::npos)
        author_name = StringUtil::TrimWhite(author_name.substr(comma_pos + 1) + " " + author_name.substr(0, comma_pos));

    std::string normalised_author_name;
    bool space_seen(false);
    for (const char ch : author_name) {
        switch (ch) {
        case '.':
            normalised_author_name += ch;
            normalised_author_name += ' ';
            space_seen = true;
            break;
        case ' ':
            if (not space_seen)
                normalised_author_name += ' ';
            space_seen = true;
            break;
        default:
            normalised_author_name += ch;
            space_seen = false;
        }
    }

    return normalised_author_name;
}


void ControlNumberGuesser::insertAuthors(const std::set<std::string> &authors, const std::string &control_number) {
    for (const auto author : authors) {
        const auto normalised_author_name(TextUtil::UTF8ToLower(NormaliseAuthorName(author)));
        std::string control_numbers;
        if (authors_db_->get(normalised_author_name, &control_numbers))
            control_numbers += "\0" + control_number;
        else
            control_numbers = control_number;
        if (unlikely(not authors_db_->set(normalised_author_name, control_numbers)))
            LOG_ERROR("failed to insert normalised author into the database!");
    }
}

