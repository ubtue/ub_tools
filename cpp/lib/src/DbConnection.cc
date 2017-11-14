/** \file   DbConnection.cc
 *  \brief  Implementation of the DbConnection class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <cstdlib>
#include "FileUtil.h"
#include "MiscUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "util.h"


DbConnection::DbConnection(const std::string &mysql_url) {
    static const RegexMatcher * const mysql_url_matcher(
        RegexMatcher::RegexMatcherFactory("mysql://([^:]+):([^@]+)@([^:/]+)(\\d+:)?/(.+)"));
    std::string err_msg;
    if (not mysql_url_matcher->matched(mysql_url, &err_msg))
        throw std::runtime_error("\"" + mysql_url + "\" does not look like an expected MySQL URL! (" + err_msg + ")");

    const std::string user(UrlUtil::UrlDecode((*mysql_url_matcher)[1]));
    const std::string passwd((*mysql_url_matcher)[2]);
    const std::string host(UrlUtil::UrlDecode((*mysql_url_matcher)[3]));
    const std::string db_name(UrlUtil::UrlDecode((*mysql_url_matcher)[5]));

    const std::string port_plus_colon((*mysql_url_matcher)[4]);
    unsigned port;
    if (port_plus_colon.empty())
        port = MYSQL_PORT;
    else
        port = StringUtil::ToUnsigned(port_plus_colon.substr(0, port_plus_colon.length() - 1));

    init(db_name, user, passwd, host, port);
}


DbConnection::~DbConnection() {
    if (initialised_)
        ::mysql_close(&mysql_);
}


bool DbConnection::query(const std::string &query_statement) {
    if (MiscUtil::SafeGetEnv("UTIL_LOG_DEBUG") == "true")
        FileUtil::AppendString("/usr/local/var/log/tuefind/sql_debug.log",
                               std::string(::progname) + ": " +  query_statement);

    return ::mysql_query(&mysql_, query_statement.c_str()) == 0;
}


void DbConnection::queryOrDie(const std::string &query_statement) {
    if (not query(query_statement))
        logger->error("in DbConnection::queryOrDie: \"" + query_statement + "\" failed: " + getLastErrorMessage());
}


DbResultSet DbConnection::getLastResultSet() {
    MYSQL_RES * const result_set(::mysql_store_result(&mysql_));
    if (result_set == nullptr)
        throw std::runtime_error("in DbConnection::getLastResultSet: mysql_store_result() failed! ("
                                 + getLastErrorMessage() + ")");

    return DbResultSet(result_set);
}


std::string DbConnection::escapeString(const std::string &unescaped_string) {
    char * const buffer(reinterpret_cast<char * const>(std::malloc(unescaped_string.size() * 2 + 1)));
    const size_t escaped_length(::mysql_real_escape_string(&mysql_, buffer, unescaped_string.data(),
                                                           unescaped_string.size()));
    const std::string escaped_string(buffer, escaped_length);
    std::free(buffer);
    return escaped_string;
}


void DbConnection::init(const std::string &database_name, const std::string &user, const std::string &passwd,
                        const std::string &host, const unsigned port)
{
    initialised_ = false;

    if (::mysql_init(&mysql_) == nullptr)
        throw std::runtime_error("in DbConnection::init: mysql_init() failed!");

    if (::mysql_real_connect(&mysql_, host.c_str(), user.c_str(), passwd.c_str(), database_name.c_str(), port,
                             /* unix_socket = */nullptr, /* client_flag = */CLIENT_MULTI_STATEMENTS) == nullptr)
        throw std::runtime_error("in DbConnection::init: mysql_real_connect() failed! (" + getLastErrorMessage()
                                 + ")");
    if (::mysql_set_character_set(&mysql_, "utf8mb4") != 0)
        throw std::runtime_error("in DbConnection::init: mysql_set_character_set() failed! (" + getLastErrorMessage()
                                 + ")");

    initialised_ = true;
}

