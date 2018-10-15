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
#include <unordered_map>
#include <kchashdb.h>


class ControlNumberGuesser {
    static const std::set<std::string> EMPTY_SET;
    const size_t MAX_CONTROL_NUMBER_LENGTH;
    kyotocabinet::HashDB *titles_db_, *authors_db_, *years_db_;
    mutable kyotocabinet::DB::Cursor *title_cursor_, *author_cursor_, *year_cursor_;

    // A map that, given a conmtrol number, allows lookups of all control numbers of items that have the same normalised title
    // and at least one common normalized author.
    mutable std::unordered_map<std::string, std::set<std::string> *> control_number_to_control_number_set_map_;

    //  A map that, given a conmtrol number, allows lookups of all control numbers of items that share the same publication year.
    mutable std::unordered_map<std::string, std::unordered_set<std::string> *> control_number_to_year_control_number_set_map_;
public:
    enum OpenMode { CLEAR_DATABASES, DO_NOT_CLEAR_DATABASES };
public:
    explicit ControlNumberGuesser(const OpenMode open_mode);
    ~ControlNumberGuesser();

    void insertTitle(const std::string &title, const std::string &control_number);
    void insertAuthors(const std::set<std::string> &authors, const std::string &control_number);
    void insertYear(const std::string &year, const std::string &control_number);
    std::set<std::string> getGuessedControlNumbers(const std::string &title, const std::vector<std::string> &authors,
                                                   const std::string &year = "") const;

    bool getNextTitle(std::string * const title, std::set<std::string> * const control_numbers) const;
    bool getNextAuthor(std::string * const author_name, std::set<std::string> * const control_numbers) const;
    bool getNextYear(std::string * const year, std::unordered_set<std::string> * const control_numbers) const;

    /** \return The control numbers of objects w/ the same title and at least one common author.
     *  \note   If we found any partners, "control_number" will also be included in the returned set.
     */
    std::set<std::string> getControlNumberPartners(const std::string &control_number, const bool also_use_years = false) const;

    /** For testing purposes. */
    static std::string NormaliseTitle(const std::string &title);
    static std::string NormaliseAuthorName(const std::string &author_name);
private:
    void FindDups(const std::unordered_map<std::string, std::set<std::string>> &title_to_control_numbers_map,
                  const std::unordered_map<std::string, std::set<std::string>> &control_number_to_authors_map) const;
    void InitControlNumberToControlNumberSetMap() const;
    void InitControlNumberToYearControlNumberSetMap() const;
};
