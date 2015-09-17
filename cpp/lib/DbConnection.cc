/** \file   DbConnection.cc
 *  \brief  Implementation of the DbConnection class.
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
#include "DbConnection.h"
#include <stdexcept>


DbRow DbResultSet::getNextRow() {
    const MYSQL_ROW row(::mysql_fetch_row(result_set_));

    unsigned long *field_sizes;
    unsigned field_count;
    if (row == nullptr) {
	field_sizes = NULL;
	field_count = 0;
    } else {
	field_sizes = ::mysql_fetch_lengths(result_set_);
	field_count = ::mysql_num_fields(result_set_);
    }

    return DbRow(row, field_sizes, field_count);
}


DbConnection::DbConnection(const std::string &database_name, const std::string &user, const std::string &passwd,
			   const std::string &host, const unsigned port): initialised_(false)
{
    if (::mysql_init(&mysql_) == nullptr)
	throw std::runtime_error("in DbConnection::DbConnection: mysql_init() failed!");

    if (::mysql_real_connect(&mysql_, host.c_str(), user.c_str(), passwd.c_str(), database_name.c_str(), port,
			     /* unix_socket = */nullptr, /* client_flag = */CLIENT_MULTI_STATEMENTS) == nullptr)
	throw std::runtime_error("in DbConnection::DbConnection: mysql_real_connect() failed!");

    initialised_ = true;
}


DbConnection::~DbConnection() {
    if (initialised_)
	::mysql_close(&mysql_);
}


DbResultSet DbConnection::getLastResultSet() {
    MYSQL_RES * const result_set(::mysql_store_result(&mysql_));
    if (result_set == nullptr)
	throw std::runtime_error("in DbConnection::getLastResultSet: mysql_store_result() failed!");

    return DbResultSet(result_set);
}
