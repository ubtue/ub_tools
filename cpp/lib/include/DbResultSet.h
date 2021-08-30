/** \file   DbResultSet.h
 *  \brief  Interface for the DbResultSet class.
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


#include <algorithm>
#include <map>
#include <unordered_set>
#include <libpq-fe.h>
#include <mysql/mysql.h>
#include <sqlite3.h>
#include "DbRow.h"


class DbResultSet {
    friend class MySQLDbConnection;
    friend class Sqlite3DbConnection;
    friend class PostgresDbConnection;
    PGresult *pg_result_;
    MYSQL_RES *mysql_res_;
    sqlite3_stmt *stmt_handle_;
    size_t no_of_rows_, column_count_, current_row_;
    std::map<std::string, unsigned> field_name_to_index_map_;
private:
    explicit DbResultSet(MYSQL_RES * const mysql_res);
    explicit DbResultSet(sqlite3_stmt * const stmt_handle);
    explicit DbResultSet(PGresult * const pg_result);
public:
    DbResultSet(DbResultSet &&other);

    /** \note If you need to instantiate a new DbResultSet instance while another is still live,
              you need to call this destructor explicitly on the live instance! (Yes, this is an evil hack!) */
    ~DbResultSet();

    /** \return The number of rows in the result set. */
    inline size_t size() const { return no_of_rows_; }

    /** \return The number of columns in a row. */
    inline size_t getColumnCount() const { return column_count_; }

    inline bool empty() const { return size() == 0; }

    /** Typically you would call this in a loop like:
     *
     *  DbRow row;
     *  while (row = result_set.getNextRow())
     *      ProcessRow(row);
     *
     */
    DbRow getNextRow();

    bool hasColumn(const std::string &column_name) const;

    /** \return The set of all values in column "column" contained in this result set. */
    std::unordered_set<std::string> getColumnSet(const std::string &column);

    inline const std::map<std::string, unsigned> getColumnNamesAndIndices() const { return field_name_to_index_map_; }
};
