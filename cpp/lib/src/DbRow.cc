/** \file   DbRow.cc
 *  \brief  Implementation of the DbRow class.
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
#include "DbRow.h"
#include <stdexcept>
#include "Compiler.h"


DbRow::DbRow(DbRow &&other) {
    row_ = other.row_;
    other.row_ = nullptr;
    field_sizes_ = other.field_sizes_;
    field_count_ = other.field_count_;
    other.field_count_ = 0;
}


std::string DbRow::operator[](const size_t i) const {
    if (i >= field_count_)
        throw std::out_of_range("index out of range in DbRow::operator[]: max. index is "
                                + std::to_string(field_count_ - 1) + ", actual index was "
                                + std::to_string(i) + "!");

    return std::string(row_[i], field_sizes_[i]);
}


std::string DbRow::operator[](const std::string &column_name) const {
    const auto name_and_index_iter(field_name_to_index_map_->find(column_name));
    if (unlikely(name_and_index_iter == field_name_to_index_map_->cend()))
        throw std::runtime_error("in DbRow::operator[](const std::string&): invalid column name \"" + column_name + "\"!");

    return std::string(row_[name_and_index_iter->second], field_sizes_[name_and_index_iter->second]);
}
