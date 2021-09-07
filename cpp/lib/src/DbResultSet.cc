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


DbResultSet::DbResultSet(DbResultSet &&other) {
    if (&other != this) {
        delete db_result_set_;
        db_result_set_ = other.db_result_set_;
        other.db_result_set_ = nullptr;
        no_of_rows_ = other.no_of_rows_;
        column_count_ = other.column_count_;
        field_name_to_index_map_.clear();
        field_name_to_index_map_.swap(other.field_name_to_index_map_);
    }
}


DbResultSet::~DbResultSet() {
    field_name_to_index_map_.clear();
    delete db_result_set_;
    db_result_set_ = nullptr;
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


DbResultSet::DbResultSet(DbResultSet * const db_result_set): db_result_set_(db_result_set) {
    db_result_set_->init(&no_of_rows_, &column_count_, &field_name_to_index_map_);
}


void MySQLResultSet::init(size_t * const no_of_rows, size_t * const column_count,
                          std::map<std::string, unsigned> * const field_name_to_index_map)
{
    *no_of_rows   = ::mysql_num_rows(mysql_res_);
    *column_count = ::mysql_num_fields(mysql_res_);

    const MYSQL_FIELD * const fields(::mysql_fetch_fields(mysql_res_));
    for (unsigned col_no(0); col_no < column_count_; ++col_no)
        field_name_to_index_map->insert(std::pair<std::string, unsigned>(fields[col_no].name, col_no));
}


DbRow MySQLResultSet::getNextRow() {
    const MYSQL_ROW row(::mysql_fetch_row(mysql_res_));

    unsigned long *field_sizes;
    unsigned field_count;
    if (row == nullptr) {
        field_sizes = nullptr;
        field_count = 0;
        field_name_to_index_map_.clear();
    } else {
        field_sizes = ::mysql_fetch_lengths(mysql_res_);
        field_count = ::mysql_num_fields(mysql_res_);
    }

    return DbRow(row, field_sizes, field_count, field_name_to_index_map_);
}


MySQLResultSet::~MySQLResultSet() {
    if (mysql_res_ != nullptr) {
        ::mysql_free_result(mysql_res_);
        mysql_res_ = nullptr;
    }
}


void Sqlite3ResultSet::init(size_t * const no_of_rows, size_t * const column_count,
                            std::map<std::string, unsigned> * const field_name_to_index_map)
{
    *no_of_rows   = ::sqlite3_data_count(stmt_handle_);
    *column_count = ::sqlite3_column_count(stmt_handle_);

    for (unsigned col_no(0); col_no < *column_count; ++col_no) {
        const char * const column_name(::sqlite3_column_name(stmt_handle_, col_no));
        if (column_name == nullptr)
            LOG_ERROR("sqlite3_column_name() failed for index " + std::to_string(col_no) + "!");
        field_name_to_index_map->insert(std::pair<std::string, unsigned>(column_name, col_no));
    }

    if (::sqlite3_reset(stmt_handle_) != SQLITE_OK)
        LOG_ERROR("sqlite3_reset failed!");
}


Sqlite3ResultSet::~Sqlite3ResultSet() {
    if (stmt_handle_ != nullptr) {
        if (sqlite3_finalize(stmt_handle_) != SQLITE_OK)
            LOG_ERROR("failed to finalise an Sqlite3 statement!");
        stmt_handle_ = nullptr;
    }
}


DbRow Sqlite3ResultSet::getNextRow() {
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
}


void PostgresResultSet::init(size_t * const no_of_rows, size_t * const column_count,
                             std::map<std::string, unsigned> * const field_name_to_index_map)
{
    if (pg_result_ == nullptr)
        *no_of_rows = *column_count = 0;
    else {
        *no_of_rows   = (pg_result_ == nullptr) ? 0 : ::PQntuples(pg_result_);
        *column_count = ::PQnfields(pg_result_);

        for (unsigned col_no(0); col_no < column_count_; ++col_no)
            field_name_to_index_map->insert(std::pair<std::string, unsigned>(::PQfname(pg_result_, col_no), col_no));
    }
}


DbRow PostgresResultSet::getNextRow() {
    ++pg_row_number_;
    if (pg_row_number_ == static_cast<int>(no_of_rows_)) {
        field_name_to_index_map_.clear();
        return DbRow(nullptr, pg_row_number_, /* field_count = */0, field_name_to_index_map_);
    }

    const int column_count(::PQnfields(pg_result_));
    return DbRow(pg_result_, pg_row_number_, column_count, field_name_to_index_map_);
}
