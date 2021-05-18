/** \file   ControlNumberGuesser.h
 *  \brief  Interface for the ControlNumberGuesser class.
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
#pragma once


#include <set>
#include <string>
#include <unordered_map>
#include "BSZUtil.h"
#include "DbConnection.h"


class ControlNumberGuesser {
    static const std::set<std::string> EMPTY_SET;
    static const std::string DATABASE_PATH;
    static const std::string INSTALLER_SCRIPT_PATH;

    const size_t MAX_CONTROL_NUMBER_LENGTH;
    std::unique_ptr<DbConnection> db_connection_;
    mutable std::unique_ptr<DbResultSet> title_cursor_, author_cursor_, year_cursor_;
    DbTransaction *db_transaction_;
public:
    explicit ControlNumberGuesser()
        : MAX_CONTROL_NUMBER_LENGTH(BSZUtil::PPN_LENGTH_NEW), db_connection_(new DbConnection(DATABASE_PATH, DbConnection::CREATE)),
          db_transaction_(nullptr) { }
    ~ControlNumberGuesser();
public:
    void clearDatabase();
    void beginUpdate();
    void endUpdate();

    void insertTitle(const std::string &title, const std::string &control_number);
    void insertAuthors(const std::set<std::string> &authors, const std::string &control_number);
    void insertYear(const std::string &year, const std::string &control_number);
    void insertDOI(const std::string &doi, const std::string &control_number);
    void insertISSN(const std::string &issn, const std::string &control_number);
    void insertISBN(const std::string &isbn, const std::string &control_number);

    /** \warning You *must* pass in complete titles for "title"!  If you obtain the title from a MARC record,
                 you can use getCompleteTitle() on the MARC:Record instance. */
    std::set<std::string> getGuessedControlNumbers(const std::string &title, const std::set<std::string> &authors,
                                                   const std::string &year = "", const std::set<std::string> &dois = {},
                                                   const std::set<std::string> &issns = {},
                                                   const std::set<std::string> &isbns = {}) const;

    bool getNextTitle(std::string * const title, std::set<std::string> * const control_numbers) const;
    bool getNextAuthor(std::string * const author_name, std::set<std::string> * const control_numbers) const;
    bool getNextYear(std::string * const year, std::unordered_set<std::string> * const control_numbers) const;

    void lookupTitle(const std::string &title, std::set<std::string> * const control_numbers) const;
    void lookupAuthor(const std::string &author_name, std::set<std::string> * const control_numbers) const;
    void lookupYear(const std::string &year, std::unordered_set<std::string> * const control_numbers) const;
    void lookupDOI(const std::string &doi, std::set<std::string> * const control_numbers) const;
    void lookupISSN(const std::string &issn, std::set<std::string> * const control_numbers) const;
    void lookupISBN(const std::string &isbn, std::set<std::string> * const control_numbers) const;

    /** For testing purposes. */
    static std::string NormaliseTitle(const std::string &title);
    static std::string NormaliseAuthorName(const std::string &author_name);
private:
    void insertNewControlNumber(const std::string &table, const std::string &column_name, const std::string &column_value,
                                const std::string &control_number);
    bool lookupControlNumber(const std::string &table, const std::string &column_name, const std::string &column_value,
                             std::string * const control_numbers) const;
    void splitControlNumbers(const std::string &concatenated_control_numbers, std::unordered_set<std::string> * const control_numbers) const;
    unsigned swapControlNumbers(const std::string &table_name, const std::string &primary_key,
                                const std::unordered_map<std::string, std::string> &old_to_new_map);
};
