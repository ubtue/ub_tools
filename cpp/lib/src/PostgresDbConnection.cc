/** \file   PostgresDbConnection.cc
 *  \brief  Implementation of the PostgresDbConnection class.
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
#include "PostgresDbConnection.h"
//#include <cstdlib>
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "UBTools.h"
#include "UrlUtil.h"
#include "util.h"


PostgresDbConnection::~PostgresDbConnection() {
    if (initialised_) {
        ::PQfinish(pg_conn_);
        pg_conn_ = nullptr;
    }
}


unsigned PostgresDbConnection::getNoOfAffectedRows() const {
    if (pg_result_ == nullptr)
        LOG_ERROR("no result set available!");
    const char * const no_of_affected_rows_as_string(::PQcmdTuples(pg_result_));
    if (unlikely(*no_of_affected_rows_as_string == '\0'))
        return 0;
    return std::strtoul(no_of_affected_rows_as_string, nullptr, 10);
}


int PostgresDbConnection::getLastErrorCode() const {
    // Postgres uses alphanumeric error codes!
    LOG_ERROR("cannot be implemented for Postgres!");
}


bool PostgresDbConnection::query(const std::string &query_statement) {
    if (MiscUtil::SafeGetEnv("UTIL_LOG_DEBUG") == "true")
        FileUtil::AppendString(UBTools::GetTueFindLogPath() + "sql_debug.log",
                               std::string(::program_invocation_name) + ": " + query_statement + '\n');

    pg_result_ = ::PQexec(pg_conn_, query_statement.c_str());
    if (pg_result_ == nullptr)
        LOG_ERROR("out of memory or failure to send the query to the server!");

    switch (::PQresultStatus(pg_result_)) {
    case PGRES_BAD_RESPONSE:
    case PGRES_FATAL_ERROR:
        ::PQclear(pg_result_);
        pg_result_ = nullptr;
        return false;
    default:
        return true;
    }
}


bool PostgresDbConnection::queryFile(const std::string &filename) {
    std::string statements;
    if (not FileUtil::ReadString(filename, &statements))
        return false;

    const auto individual_statements(SplitPostgresStatements(statements));
    for (const auto &statement : individual_statements) {
        if (pg_result_ != nullptr)
            ::PQclear(pg_result_);
        pg_result_ = ::PQexec(pg_conn_, statement.c_str());
        if (pg_result_ == nullptr)
            LOG_ERROR("out of memory or failure to send the query to the server!");

        switch (::PQresultStatus(pg_result_)) {
        case PGRES_BAD_RESPONSE:
        case PGRES_FATAL_ERROR:
            ::PQclear(pg_result_);
            pg_result_ = nullptr;
            return false;
        default:
            /* do nothing */;
        }
    }

    return true;
}


DbResultSet PostgresDbConnection::getLastResultSet() {
    const auto temp_pg_result = pg_result_;
    pg_result_ = nullptr;
    return DbResultSet(new PostgresResultSet(temp_pg_result));
}


std::string PostgresDbConnection::escapeString(const std::string &unescaped_string, const bool add_quotes,
                                               const bool return_null_on_empty_string) {
    if (unescaped_string.empty() and return_null_on_empty_string)
        return "NULL";

    char * const escaped_c_string(::PQescapeLiteral(pg_conn_, unescaped_string.c_str(), unescaped_string.size()));
    if (unlikely(escaped_c_string == nullptr))
        LOG_ERROR("failed to escape string! (" + getLastErrorMessage() + ")");

    std::string escaped_string;
    if (add_quotes)
        escaped_string = "'" + std::string(escaped_c_string) + "'";
    else
        escaped_string = escaped_c_string;
    ::PQfreemem(reinterpret_cast<void *>(escaped_c_string));

    return escaped_string;
}


bool PostgresDbConnection::tableExists(const std::string &database_name, const std::string &table_name) {
    queryOrDie("SELECT EXISTS (SELECT FROM pg_tables WHERE schemaname = '" + database_name + "' AND tablename = '" + table_name + "'");
    DbResultSet result_set(getLastResultSet());
    return result_set.getNextRow()[0] == "t";
}
