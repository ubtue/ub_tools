/** \file   DbResultSet.h
 *  \brief  Interface for the DbResultSet class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2016 Universitätsbiblothek Tübingen.  All rights reserved.
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
#ifndef DB_RESULT_SET_H
#define DB_RESULT_SET_H


#include <algorithm>
#include <map>
#include <mysql/mysql.h>
#include "DbRow.h"


class DbResultSet {
    friend class DbConnection;
    MYSQL_RES *result_set_;
    std::map<std::string, unsigned> field_name_to_index_map_;
private:
    explicit DbResultSet(MYSQL_RES * const result_set);
public:
    DbResultSet(DbResultSet &&other);
    ~DbResultSet();

    /** \return The number of rows in the result set. */
    size_t size() const { return ::mysql_num_rows(result_set_); }

    /** \return The number of columns in a row. */
    unsigned getColumnCount() const { return ::mysql_num_fields(result_set_); }

    bool empty() const  { return ::mysql_num_rows(result_set_) == 0; }

    /** Typically you would call this in a loop like:
     *
     *  DbRow row;
     *  while (row = result_set.getNextRow())
     *      ProcessRow(row);
     *
     */
    DbRow getNextRow();
};


#endif // ifndef DB_RESULT_SET_H
