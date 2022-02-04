/** \file   DbRow.h
 *  \brief  Interface for the DbRow class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include <map>
#include <string>
#include <libpq-fe.h>
#include <mysql/mysql.h>
#include <sqlite3.h>


/** \warning It is unsafe to access DbRow instances after the DbResultSet that it belongs to has been deleted
 *           or gone out of scope! */
class DbRow {
    friend class MySQLResultSet;
    friend class Sqlite3ResultSet;
    friend class PostgresResultSet;
    PGresult *pg_result_;
    int pg_row_number_;
    MYSQL_ROW row_;
    unsigned long *field_sizes_;
    unsigned field_count_;
    sqlite3_stmt *stmt_handle_;
    const std::map<std::string, unsigned> *field_name_to_index_map_;

private:
    DbRow(MYSQL_ROW row, unsigned long * const field_sizes, const unsigned field_count,
          const std::map<std::string, unsigned> &field_name_to_index_map)
        : pg_result_(nullptr), row_(row), field_sizes_(field_sizes), field_count_(field_count), stmt_handle_(nullptr),
          field_name_to_index_map_(&field_name_to_index_map) { }
    DbRow(sqlite3_stmt *stmt_handle, const std::map<std::string, unsigned> &field_name_to_index_map)
        : pg_result_(nullptr), row_(nullptr), field_sizes_(nullptr), field_count_(field_name_to_index_map.size()),
          stmt_handle_(stmt_handle), field_name_to_index_map_(&field_name_to_index_map) { }
    DbRow(PGresult * const pg_result, const int pg_row_number, const unsigned field_count,
          const std::map<std::string, unsigned> &field_name_to_index_map)
        : pg_result_(pg_result), pg_row_number_(pg_row_number), row_(nullptr), field_sizes_(nullptr), field_count_(field_count),
          stmt_handle_(nullptr), field_name_to_index_map_(&field_name_to_index_map) { }

public:
    DbRow(): pg_result_(nullptr), row_(nullptr), field_sizes_(nullptr), field_count_(0), stmt_handle_(nullptr) { }
    DbRow(DbRow &&other);

    DbRow &operator=(const DbRow &rhs) = default;

    /** \return The number of fields in the row. */
    inline size_t size() const { return field_count_; }

    /** \return True if the row contains no columns, else false. */
    inline bool empty() const { return field_name_to_index_map_->empty(); }

    /** \brief Tests a DbRow for being non-empty. */
    explicit operator bool() const { return not empty(); }

    /** \brief Retrieve the i-th field from the row.  (The index is 0-based.)
     *  \throws std::out_of_range if the index "i" refers to an invalid column or the column's value is NULL.
     */
    std::string operator[](const size_t i) const;

    /** \brief Retrieves the field w/ column name "column_name" from the row.
     *  \throws std::out_of_range if the "column_name" refers to a non-existent column name or the column's value is NULL.
     */
    std::string operator[](const std::string &column_name) const;

    /** \brief Retrieves the field w/ column name "column_name" from the row.
     *  \throws std::out_of_range if the "column_name" refers to a non-existent column name.
     *  \return The column's value or "default_value" if the colum value is NULL.
     */
    std::string getValue(const std::string &column_name, const std::string &default_value = "") const;

    /** \throws std::out_of_range if the index "i" refers to an invalid column. */
    bool isNull(const size_t i) const;

    /** \throws std::out_of_range if the "column_name" refers to a non-existent column name. */
    bool isNull(const std::string &column_name) const;
};
