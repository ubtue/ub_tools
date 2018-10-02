/** \file   ControlNumberGuesser.h
 *  \brief  Interface for the ControlNumberGuesser class.
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
#pragma once


#include <set>
#include <string>
#include <kchashdb.h>


class ControlNumberGuesser {
    kyotocabinet::HashDB *titles_db_, *authors_db_;
public:
    enum OpenMode { CLEAR_DATABASES, DO_NOT_CLEAR_DATABASES };
public:
    explicit ControlNumberGuesser(const OpenMode open_mode);
    ~ControlNumberGuesser() { delete titles_db_; delete authors_db_; }

    void insertTitle(const std::string &title, const std::string &control_number);
    void insertAuthors(const std::set<std::string> &authors, const std::string &control_number);
    std::set<std::string> getGuessedControlNumbers(const std::string &title, const std::vector<std::string> &authors) const;

    /** For testing purposes. */
    static std::string NormaliseTitle(const std::string &title);
    static std::string NormaliseAuthorName(const std::string &author_name);
};
