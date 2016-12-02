/** \file   DbResultSet.cc
 *  \brief  Implementation of the DbResultSet class.
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
#include "DbResultSet.h"
#include <stdexcept>


DbResultSet::DbResultSet(MYSQL_RES * const result_set): result_set_(result_set) {
    const MYSQL_FIELD * const fields(::mysql_fetch_fields(result_set_));
    const int COLUMN_COUNT(::mysql_num_fields(result_set));
    for (int col_no(0); col_no < COLUMN_COUNT; ++col_no)
        field_name_to_index_map_.insert(std::pair<std::string, unsigned>(fields[col_no].name, col_no));
}


DbResultSet::DbResultSet(DbResultSet &&other) {
    if (&other != this) {
        result_set_ = other.result_set_;
        other.result_set_ = nullptr;
    }
}


DbResultSet::~DbResultSet() {
    if (result_set_ != nullptr)
        ::mysql_free_result(result_set_);
}


DbRow DbResultSet::getNextRow() {
    const MYSQL_ROW row(::mysql_fetch_row(result_set_));

    unsigned long *field_sizes;
    unsigned field_count;
    if (row == nullptr) {
        field_sizes = nullptr;
        field_count = 0;
    } else {
        field_sizes = ::mysql_fetch_lengths(result_set_);
        field_count = ::mysql_num_fields(result_set_);
    }

    return DbRow(row, field_sizes, field_count, field_name_to_index_map_);
}
