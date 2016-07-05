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


#include <map>
#include <string>
#include <mysql/mysql.h>


/** \warning It is unsafe to access DbRow instances after the DbResultSet that it belongs to has been deleted
 *           or gone out of scope! */
class DbRow {
    friend class DbResultSet;
    MYSQL_ROW row_;
    unsigned long *field_sizes_;
    unsigned field_count_;
    const std::map<std::string, unsigned>  *field_name_to_index_map_;
private:
    explicit DbRow(MYSQL_ROW row, unsigned long * const field_sizes, const unsigned field_count,
                   const std::map<std::string, unsigned> &field_name_to_index_map)
        : row_(row), field_sizes_(field_sizes), field_count_(field_count), field_name_to_index_map_(&field_name_to_index_map) { }
public:
    DbRow(): row_(nullptr), field_sizes_(nullptr), field_count_(0) { }
    DbRow(DbRow &&other);

    DbRow &operator=(const DbRow &rhs) = default;

    /** \return The number of fields in the row. */
    size_t size() const { return field_count_; }

    /** \brief Tests a DbRow for being non-nullptr. */
    explicit operator bool() const { return row_ != nullptr; }

    /** \brief Retrieve the i-th field from the row.  (The index is 0-based.) */
    std::string operator[](const size_t i) const;

    /** \brief Retrieves the field w/ column name "column_name" from the row. */
    std::string operator[](const std::string &column_name) const;
};


#endif // ifndef DB_ROW_H
