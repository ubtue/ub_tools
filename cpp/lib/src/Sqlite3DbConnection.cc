/** \file   Sqlite3DbConnection.cc
 *  \brief  Implementation of the Sqlite3DbConnection class.
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
#include "Sqlite3DbConnection.h"
#include <cerrno>
//#include <cstdlib>
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


enum ParseState { NORMAL, IN_DOUBLE_DASH_COMMENT, IN_C_STYLE_COMMENT, IN_STRING_CONSTANT };


// A helper function for SplitSqliteStatements().
// CREATE TRIGGER statements end with a semicolon followed by END.  As we usually treat semicolons as statement
// separators we need special handling for this case.
void AddStatement(const std::string &statement_candidate, std::vector<std::string> * const individual_statements) {
    static RegexMatcher *create_trigger_matcher(
        RegexMatcher::RegexMatcherFactoryOrDie("^CREATE\\s+(TEMP|TEMPORARY)?\\s+TRIGGER",
                                               RegexMatcher::ENABLE_UTF8 | RegexMatcher::CASE_INSENSITIVE));

    if (individual_statements->empty())
        individual_statements->emplace_back(statement_candidate);
    else if (::strcasecmp(statement_candidate.c_str(), "END") == 0 or ::strcasecmp(statement_candidate.c_str(), "END;") == 0) {
        if (individual_statements->empty() or not create_trigger_matcher->matched(individual_statements->back()))
            individual_statements->emplace_back(statement_candidate);
        else
            individual_statements->back() += statement_candidate;
    } else
        individual_statements->emplace_back(statement_candidate);
}


// Splits a compound Sqlite SQL statement into individual statements and eliminates comments.
void SplitSqliteStatements(const std::string &compound_statement, std::vector<std::string> * const individual_statements) {
    ParseState parse_state(NORMAL);
    std::string current_statement;
    char last_ch('\0');
    for (const char ch : compound_statement) {
        if (parse_state == IN_DOUBLE_DASH_COMMENT) {
            if (ch == '\n')
                parse_state = NORMAL;
        } else if (parse_state == IN_C_STYLE_COMMENT) {
            if (ch == '/' and last_ch == '*')
                parse_state = NORMAL;
        } else if (parse_state == IN_STRING_CONSTANT) {
            if (ch == '\'')
                parse_state = NORMAL;
            current_statement += ch;
        } else { // state == NORMAL
            if (ch == '-' and last_ch == '-') {
                current_statement.resize(current_statement.size() - 1); // Remove the trailing dash.
                parse_state = IN_DOUBLE_DASH_COMMENT;
            } else if (ch == '*' and last_ch == '/') {
                current_statement.resize(current_statement.size() - 1); // Remove the trailing slash.
                parse_state = IN_C_STYLE_COMMENT;
            } else if (ch == ';') {
                StringUtil::TrimWhite(&current_statement);
                if (not current_statement.empty()) {
                    current_statement += ';';
                    AddStatement(current_statement, individual_statements);
                    current_statement.clear();
                }
            } else if (ch == '\'') {
                parse_state = IN_STRING_CONSTANT;
                current_statement += ch;
            } else
                current_statement += ch;
        }

        last_ch = ch;
    }

    if (parse_state == IN_C_STYLE_COMMENT)
        LOG_ERROR("unterminated C-style comment in SQL statement sequence: \"" + compound_statement + "\"!");
    else if (parse_state == IN_STRING_CONSTANT)
        LOG_ERROR("unterminated string constant in SQL statement sequence: \"" + compound_statement + "\"!");
    else if (parse_state == NORMAL) {
        StringUtil::TrimWhite(&current_statement);
        if (not current_statement.empty()) {
            current_statement += ';';
            AddStatement(current_statement, individual_statements);
        }
    }
}


} // unnamed namespace


Sqlite3DbConnection::Sqlite3DbConnection(const std::string &database_path, const OpenMode open_mode)
    : stmt_handle_(nullptr), database_path_(database_path)
{
    int flags(0);
    switch (open_mode) {
    case READONLY:
        flags = SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX;
        break;
    case READWRITE:
        flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX;
        break;
    case CREATE:
        flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
        break;
    }

    if (::sqlite3_open_v2(database_path.c_str(), &sqlite3_, flags, nullptr) != SQLITE_OK)
        LOG_ERROR("failed to create or open an Sqlite3 database with path \"" + database_path + "\"!");
    errno = 0; // It seems that sqlite3_open_v2 internally tries something that fails but doesn't clear errno afterwards.
    initialised_ = true;
}


Sqlite3DbConnection::~Sqlite3DbConnection() {
    if (initialised_) {
        if (stmt_handle_ != nullptr) {
            const int result_code(::sqlite3_finalize(stmt_handle_));
            if (result_code != SQLITE_OK)
                LOG_ERROR("failed to finalise an Sqlite3 statement! (" + getLastErrorMessage() + ", code was "
                          + std::to_string(result_code) + ")");
        }
        if (::sqlite3_close(sqlite3_) != SQLITE_OK)
            LOG_ERROR("failed to cleanly close an Sqlite3 database!");
    }
}


bool Sqlite3DbConnection::query(const std::string &query_statement) {
    if (MiscUtil::SafeGetEnv("UTIL_LOG_DEBUG") == "true")
        FileUtil::AppendString(UBTools::GetTueFindLogPath() + "sql_debug.log",
                               std::string(::program_invocation_name) + ": " +  query_statement + '\n');

    if (stmt_handle_ != nullptr) {
        const int result_code(::sqlite3_finalize(stmt_handle_));
        if (result_code != SQLITE_OK) {
            LOG_WARNING("failed to finalise an Sqlite3 statement! (" + getLastErrorMessage() + ", code was "
                        + std::to_string(result_code) + ")");
            return false;
        }
    }

    const char *rest;
    if (::sqlite3_prepare_v2(sqlite3_, query_statement.c_str(), query_statement.length(), &stmt_handle_, &rest) != SQLITE_OK)
        return false;
    if (rest != nullptr and *rest != '\0')
        LOG_ERROR("junk after SQL statement (" + query_statement + "): \"" + std::string(rest) + "\"!");
    switch (::sqlite3_step(stmt_handle_)) {
    case SQLITE_DONE:
    case SQLITE_OK:
        if (::sqlite3_finalize(stmt_handle_) != SQLITE_OK)
            LOG_ERROR("failed to finalise an Sqlite3 statement! (" + getLastErrorMessage() + ")");
        stmt_handle_ = nullptr;
        break;
    case SQLITE_ROW:
        break;
    default:
        return false;
    }

    return true;
}


bool Sqlite3DbConnection::queryFile(const std::string &filename) {
    std::string statements;
    if (not FileUtil::ReadString(filename, &statements))
        return false;

    std::vector<std::string> individual_statements;
    SplitSqliteStatements(statements, &individual_statements);
    for (const auto &statement : individual_statements) {
        if (not query(statement))
            return false;
    }

    return true;
}


DbResultSet Sqlite3DbConnection::getLastResultSet() {
    const auto temp_handle(stmt_handle_);
    stmt_handle_ = nullptr;
    return DbResultSet(new Sqlite3ResultSet(temp_handle));
}


std::string Sqlite3DbConnection::escapeString(const std::string &unescaped_string, const bool add_quotes,
                                              const bool return_null_on_empty_string)
{
    if (unescaped_string.empty() and return_null_on_empty_string)
        return "NULL";

    char * const buffer(reinterpret_cast<char *>(std::malloc(unescaped_string.size() * 2 + 1)));
    size_t escaped_length;

    char *cp(buffer);
    for (char ch : unescaped_string) {
        if (ch == '\'')
            *cp++ = '\'';
        *cp++ = ch;
    }

    escaped_length = cp - buffer;

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


bool Sqlite3DbConnection::tableExists(const std::string &database_name, const std::string &table_name) {
    Sqlite3DbConnection connection(database_name, READONLY);
    connection.queryOrDie("SELECT name FROM sqlite_master WHERE type='table' AND name='"
                          + connection.escapeString(table_name) + "'");
    return not connection.getLastResultSet().empty();
}


bool Sqlite3DbConnection::backup(const std::string &output_filename, std::string * const err_msg) {
    sqlite3 *sqlite3_backup_file;
    int return_code;
    if ((return_code = ::sqlite3_open(output_filename.c_str(), &sqlite3_backup_file)) != SQLITE_OK) {
        *err_msg = "failed to create backup to \"" + output_filename + "\": "
                   + std::string(::sqlite3_errmsg(sqlite3_backup_file));
        return false;
    }

    sqlite3_backup *backup_handle(::sqlite3_backup_init(sqlite3_backup_file, "main", sqlite3_, "main"));
    if (backup_handle == nullptr) {
        ::sqlite3_close(sqlite3_backup_file);
        *err_msg = "failed to initialize Sqlite3 backup to \"" + output_filename
                   + "\": there is already a read or read-write transaction open on the destination database!";
        return false;
    }

    const int DATABASE_PAGE_COUNT(500); // How many pages to copy to the backup at each iteration.
    const int SLEEP_INTERVAL(100);      // in milliseconds.
    bool backup_incomplete;
    do {
        return_code = ::sqlite3_backup_step(backup_handle, DATABASE_PAGE_COUNT);
        backup_incomplete = return_code == SQLITE_OK or return_code == SQLITE_BUSY or return_code == SQLITE_LOCKED;
        if (backup_incomplete)
            ::sqlite3_sleep(SLEEP_INTERVAL);
    } while (backup_incomplete);
    ::sqlite3_backup_finish(backup_handle);

    return_code = ::sqlite3_errcode(sqlite3_backup_file);
    ::sqlite3_close(sqlite3_backup_file);
    if (return_code != SQLITE_OK) {
        *err_msg = "an error occurred during the backup to \"" + output_filename + "\": "
                   + std::string(::sqlite3_errmsg(sqlite3_backup_file));
        return false;
    }

    return true;
}
