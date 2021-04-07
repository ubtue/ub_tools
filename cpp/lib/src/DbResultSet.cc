/** \file   DbResultSet.cc
 *  \brief  Implementation of the DbResultSet class.
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
#include "DbResultSet.h"
#include <stdexcept>
#include "util.h"


DbResultSet::DbResultSet(MYSQL_RES * const result_set)
    : result_set_(result_set), stmt_handle_(nullptr), column_count_(::mysql_num_fields(result_set))
{
    const MYSQL_FIELD * const fields(::mysql_fetch_fields(result_set_));
    for (unsigned col_no(0); col_no < column_count_; ++col_no)
        field_name_to_index_map_.insert(std::pair<std::string, unsigned>(fields[col_no].name, col_no));
    no_of_rows_ = ::mysql_num_rows(result_set_);
}


DbResultSet::DbResultSet(sqlite3_stmt * const stmt_handle)
    : result_set_(nullptr), stmt_handle_(stmt_handle), column_count_(::sqlite3_column_count(stmt_handle))
{
    for (unsigned col_no(0); col_no < column_count_; ++col_no) {
        const char * const column_name(::sqlite3_column_name(stmt_handle_, col_no));
        if (column_name == nullptr)
            LOG_ERROR("sqlite3_column_name() failed for index " + std::to_string(col_no) + "!");
        field_name_to_index_map_.insert(std::pair<std::string, unsigned>(column_name, col_no));
    }
    no_of_rows_ = ::sqlite3_data_count(stmt_handle_);
    if (::sqlite3_reset(stmt_handle_) != SQLITE_OK)
        LOG_ERROR("sqlite3_reset failed!");
}


DbResultSet::DbResultSet(DbResultSet &&other) {
    if (&other != this) {
        result_set_ = other.result_set_;
        other.result_set_ = nullptr;
        stmt_handle_ = other.stmt_handle_;
        other.stmt_handle_ = nullptr;
    }
}


DbResultSet::~DbResultSet() {
    field_name_to_index_map_.clear();
    if (result_set_ != nullptr) {
        ::mysql_free_result(result_set_);
        result_set_ = nullptr;
    } else {
        if (stmt_handle_ != nullptr) {
            if (sqlite3_finalize(stmt_handle_) != SQLITE_OK)
                LOG_ERROR("failed to finalise an Sqlite3 statement!");
            stmt_handle_ = nullptr;
        }
    }
}


DbRow DbResultSet::getNextRow() {
    if (stmt_handle_ != nullptr) {
        switch (::sqlite3_step(stmt_handle_)) {
        case SQLITE_DONE:
        case SQLITE_OK:
            if (::sqlite3_finalize(stmt_handle_) != SQLITE_OK)
                LOG_ERROR("failed to finalise an Sqlite3 statement!");
            stmt_handle_ = nullptr;
            field_name_to_index_map_.clear();
            break;
        case SQLITE_ROW:
            break;
        default:
            LOG_ERROR("an unknown error occurred while calling sqlite3_step()!");
        }

        return DbRow(stmt_handle_, field_name_to_index_map_);
    } else {
        const MYSQL_ROW row(::mysql_fetch_row(result_set_));

        unsigned long *field_sizes;
        unsigned field_count;
        if (row == nullptr) {
            field_sizes = nullptr;
            field_count = 0;
            field_name_to_index_map_.clear();
        } else {
            field_sizes = ::mysql_fetch_lengths(result_set_);
            field_count = ::mysql_num_fields(result_set_);
        }

        return DbRow(row, field_sizes, field_count, field_name_to_index_map_);
    }
}


bool DbResultSet::hasColumn(const std::string &column_name) const {
    return field_name_to_index_map_.find(column_name) != field_name_to_index_map_.cend();
}


std::unordered_set<std::string> DbResultSet::getColumnSet(const std::string &column) {
    std::unordered_set<std::string> set;

    while (const DbRow row = getNextRow())
        set.emplace(row[column]);

    return set;
}
