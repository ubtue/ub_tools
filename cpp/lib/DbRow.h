/** \file   DbRow.h
 *  \brief  Interface for the DbRow class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#ifndef DB_ROW_H
#define DB_ROW_H


#include <string>
#include <mysql/mysql.h>


/** \warning It is unsafe to access DbRow instances after the DbResultSet that it belongs to has been deleted
 *           or gone out of scope! */
class DbRow {
    friend class DbResultSet;
    MYSQL_ROW row_;
    unsigned long *field_sizes_;
    unsigned field_count_;
private:
    explicit DbRow(MYSQL_ROW row, unsigned long * const field_sizes, const unsigned field_count)
	: row_(row), field_sizes_(field_sizes), field_count_(field_count) { }
public:
    DbRow(): row_(nullptr), field_sizes_(nullptr), field_count_(0) { }
    DbRow(DbRow &&other);

    DbRow &operator=(const DbRow &rhs) = default;

    /** \return The number of fields in the row. */
    size_t size() const { return field_count_; }

    /** \brief Tests a DbRow for being non-NULL. */
    explicit operator bool() const { return row_ != nullptr; }

    /** \brief Retrieve the i-th field from the row.  (The index is 0-based.) */
    std::string operator[](const size_t i) const;
};


#endif // ifndef DB_ROW_H
