/** \file   MySQLDbConnection.cc
 *  \brief  Implementation of the MySQLDbConnection class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MySQLDbConnection.h"
#include <cerrno>
#include <cstdlib>
#include "FileUtil.h"
#include "IniFile.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "UrlUtil.h"
#include "util.h"


MySQLDbConnection::MySQLDbConnection(const TimeZone time_zone)
    : MySQLDbConnection(IniFile(DEFAULT_CONFIG_FILE_PATH), "Database", time_zone) {
}


MySQLDbConnection::MySQLDbConnection(const std::string &mysql_url, const Charset charset, const TimeZone time_zone) {
    static RegexMatcher * const mysql_url_matcher(RegexMatcher::RegexMatcherFactory("mysql://([^:]+):([^@]+)@([^:/]+)(\\d+:)?/(.+)"));
    std::string err_msg;
    if (not mysql_url_matcher->matched(mysql_url, &err_msg))
        LOG_ERROR("\"" + mysql_url + "\" does not look like an expected MySQL URL! (" + err_msg + ")");

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

    init(db_name, user, passwd, host, port, charset, time_zone);
}


MySQLDbConnection::MySQLDbConnection(const IniFile &ini_file, const std::string &ini_file_section, const TimeZone time_zone) {
    const auto db_section(ini_file.getSection(ini_file_section));
    if (db_section == ini_file.end())
        LOG_ERROR("DbConnection section \"" + ini_file_section + "\" not found in config file \"" + ini_file.getFilename() + "\"!");

    const std::string host(db_section->getString("sql_host", "localhost"));
    const std::string database(db_section->getString("sql_database"));
    const std::string user(db_section->getString("sql_username"));
    const std::string password(db_section->getString("sql_password"));
    const unsigned port(db_section->getUnsigned("sql_port", MYSQL_PORT));

    const std::map<std::string, int> string_to_value_map{
        { "UTF8MB3", UTF8MB3 },
        { "UTF8MB4", UTF8MB4 },
    };
    const Charset charset(static_cast<Charset>(db_section->getEnum("sql_charset", string_to_value_map, UTF8MB4)));

    init(database, user, password, host, port, charset, time_zone);
}


MySQLDbConnection::~MySQLDbConnection() {
    if (initialised_)
        ::mysql_close(&mysql_);
}


void MySQLDbConnection::init(const std::string &database_name, const std::string &user, const std::string &passwd, const std::string &host,
                             const unsigned port, const DbConnection::Charset charset, const DbConnection::TimeZone time_zone) {
    initialised_ = false;

    if (::mysql_init(&mysql_) == nullptr)
        LOG_ERROR("mysql_init() failed! (1)");

    if (::mysql_real_connect(&mysql_, host.c_str(), user.c_str(), passwd.c_str(), database_name.c_str(), port,
                             /* unix_socket = */ nullptr, /* client_flag = */ CLIENT_MULTI_STATEMENTS)
        == nullptr)

        // password is intentionally omitted here!
        LOG_ERROR("mysql_real_connect() failed! (" + getLastErrorMessage() + ", host=\"" + host + "\", user=\"" + user + "\", passwd=\""
                  + passwd + "\", database_name=\"" + database_name + "\", port=" + std::to_string(port) + ")");
    if (::mysql_set_character_set(&mysql_, (charset == UTF8MB4) ? "utf8mb4" : "utf8") != 0)
        LOG_ERROR("mysql_set_character_set() failed! (" + getLastErrorMessage() + ")");
    errno = 0; // Don't ask unless you want to cry!

    initialised_ = true;
    setTimeZone(time_zone);
    database_name_ = database_name;
    user_ = user;
    passwd_ = passwd;
    host_ = host;
    port_ = port;
    charset_ = charset;
}


void MySQLDbConnection::init(const std::string &user, const std::string &passwd, const std::string &host, const unsigned port,
                             const DbConnection::Charset charset, const DbConnection::TimeZone time_zone) {
    initialised_ = false;

    if (::mysql_init(&mysql_) == nullptr)
        LOG_ERROR("mysql_init() failed! (2)");

    if (::mysql_real_connect(&mysql_, host.c_str(), user.c_str(), passwd.c_str(), nullptr, port,
                             /* unix_socket = */ nullptr, /* client_flag = */ CLIENT_MULTI_STATEMENTS)
        == nullptr)
        LOG_ERROR("::mysql_real_connect() failed! (" + getLastErrorMessage() + ")");
    if (::mysql_set_character_set(&mysql_, CharsetToString(charset).c_str()) != 0)
        LOG_ERROR("::mysql_set_character_set() failed! (" + getLastErrorMessage() + ")");

    initialised_ = true;
    setTimeZone(time_zone);
    user_ = user;
    passwd_ = passwd;
    host_ = host;
    port_ = port;
    charset_ = charset;
}


void MySQLDbConnection::setTimeZone(const TimeZone time_zone) {
    switch (time_zone) {
    case TZ_SYSTEM:
        /* Default => Do nothing! */
        break;
    case TZ_UTC:
        if (::mysql_query(&mysql_, "SET time_zone = '+0:00'") != 0)
            LOG_ERROR("failed to set the connection time zone to UTC! (" + std::string(::mysql_error(&mysql_)) + ")");
        break;
    }
    time_zone_ = time_zone;
}


bool MySQLDbConnection::query(const std::string &query_statement) {
    if (MiscUtil::SafeGetEnv("UTIL_LOG_DEBUG") == "true")
        FileUtil::AppendString(UBTools::GetTueFindLogPath() + "sql_debug.log",
                               std::string(::program_invocation_name) + ": " + query_statement + '\n');

    const auto statements(SplitMySQLStatements(query_statement));
    for (const auto &statement : statements) {
        if (::mysql_query(&mysql_, statement.c_str()) != 0) {
            LOG_WARNING("Could not successfully execute statement \"" + statement
                        + "\": SQL error code:" + std::to_string(::mysql_errno(&mysql_)));
            return false;
        }
    }
    return true;
}


bool MySQLDbConnection::queryFile(const std::string &filename) {
    std::string statements;
    if (not FileUtil::ReadString(filename, &statements))
        return false;

    const auto individual_statements(SplitMySQLStatements(statements));
    for (const auto &statement : individual_statements) {
        if (::mysql_query(&mysql_, statement.c_str()) != 0) {
            LOG_WARNING("Could not successfully execute statement \"" + statement
                        + "\": SQL error code:" + std::to_string(::mysql_errno(&mysql_)));
            return false;
        }
    }

    return true;
}


DbResultSet MySQLDbConnection::getLastResultSet() {
    MYSQL_RES * const result_set(::mysql_store_result(&mysql_));
    if (result_set == nullptr)
        LOG_ERROR("::mysql_store_result() failed! (" + getLastErrorMessage() + ")");

    return DbResultSet(new MySQLResultSet(result_set));
}


std::string MySQLDbConnection::escapeString(const std::string &unescaped_string, const bool add_quotes,
                                            const bool return_null_on_empty_string) {
    if (unescaped_string.empty() and return_null_on_empty_string)
        return "NULL";

    char * const buffer(reinterpret_cast<char *>(std::malloc(unescaped_string.size() * 2 + 1)));
    size_t escaped_length;

    escaped_length = ::mysql_real_escape_string(&mysql_, buffer, unescaped_string.data(), unescaped_string.size());

    std::string escaped_string;
    escaped_string.reserve(escaped_length + (add_quotes ? 2 : 0));
    if (add_quotes)
        escaped_string += '\'';

    escaped_string.append(buffer, escaped_length);
    std::free(buffer);

    if (add_quotes)
        escaped_string += '\'';
    return escaped_string;
}


bool MySQLDbConnection::tableExists(const std::string &database_name, const std::string &table_name) {
    queryOrDie("SELECT EXISTS(SELECT * FROM information_schema.tables WHERE table_schema = '" + escapeString(database_name)
               + "' AND table_name = '" + escapeString(table_name) + "')");
    DbResultSet result_set(getLastResultSet());
    return result_set.getNextRow()[0] == "1";
}
