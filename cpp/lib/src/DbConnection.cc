/** \file   DbConnection.cc
 *  \brief  Implementation of the DbConnection class.
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
#include "DbConnection.h"
#include <cerrno>
#include <cstdlib>
#include "FileUtil.h"
#include "IniFile.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "UBTools.h"
#include "util.h"


DbConnection::DbConnection(const std::string &mysql_url, const Charset charset, const TimeZone time_zone)
    : pg_conn_(nullptr), sqlite3_(nullptr), stmt_handle_(nullptr)
{
    static RegexMatcher * const mysql_url_matcher(
        RegexMatcher::RegexMatcherFactory("mysql://([^:]+):([^@]+)@([^:/]+)(\\d+:)?/(.+)"));
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


DbConnection::DbConnection(const TimeZone time_zone): DbConnection(IniFile(DEFAULT_CONFIG_FILE_PATH), "Database", time_zone) {
}


DbConnection::DbConnection(const IniFile &ini_file, const std::string &ini_file_section, const TimeZone time_zone) {
    const auto db_section(ini_file.getSection(ini_file_section));
    if (db_section == ini_file.end())
        LOG_ERROR("DbConnection section \"" + ini_file_section +"\" not found in config file \"" + ini_file.getFilename() + "\"!");

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


DbConnection::DbConnection(const std::string &database_path, const OpenMode open_mode)
    : type_(T_SQLITE), pg_conn_(nullptr), stmt_handle_(nullptr)
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


DbConnection::~DbConnection() {
    if (initialised_) {
        if (type_ == T_MYSQL)
            ::mysql_close(&mysql_);
        else if (type_ == T_SQLITE) {
            if (stmt_handle_ != nullptr) {
                const int result_code(::sqlite3_finalize(stmt_handle_));
                if (result_code != SQLITE_OK)
                    LOG_ERROR("failed to finalise an Sqlite3 statement! (" + getLastErrorMessage() + ", code was "
                              + std::to_string(result_code) + ")");
            }
            if (::sqlite3_close(sqlite3_) != SQLITE_OK)
                LOG_ERROR("failed to cleanly close an Sqlite3 database!");
        } else { // Postgres
            ::PQfinish(pg_conn_);
            pg_conn_ = nullptr;
        }
    }
}


int DbConnection::getLastErrorCode() const {
    if (type_ == T_MYSQL)
        return static_cast<int>(::mysql_errno(&mysql_));
    else if (type_ == T_SQLITE)
        return ::sqlite3_errcode(sqlite3_);
    else
        LOG_ERROR("not implemented for Postgres!");
}


enum CommentFlavour { NO_COMMENT, C_STYLE_COMMENT, END_OF_LINE_COMMENT };


std::vector<std::string> DbConnection::SplitMySQLStatements(const std::string &query) {
    std::vector<std::string> statements;

    std::string statement;
    char current_quote('\0'); // NUL means not currently in a string constant.
    bool escaped(false); // backslash not yet seen
    bool do_not_split_on_semicolons(false);
    CommentFlavour comment_flavour(NO_COMMENT);
    for (auto ch(query.cbegin()); ch != query.cend(); ++ch) {
        if (comment_flavour != NO_COMMENT) {
            if ((comment_flavour == END_OF_LINE_COMMENT and *ch == '\n')
                or (*ch == '/' and likely(ch > query.cbegin()) and *(ch - 1) == '*'))
                comment_flavour = NO_COMMENT;
        } else if (current_quote != '\0') {
            if (escaped)
                escaped = false;
            else if (*ch == current_quote)
                current_quote = '\0';
            else if (*ch == '\\')
                escaped = true;
        } else if ((ch == query.cbegin() or *(ch - 1) == '\n') and *ch == '#') {
            ++ch; // Skip over the hash mark.
            std::string directive;
            while (ch != query.cend() and *ch != '\n')
                directive += *ch++;
            if (ch != query.cend())
                ++ch; // Skip over the newline.
            StringUtil::RightTrim(&directive);

            if (directive == "do_not_split_on_semicolons") {
                if (do_not_split_on_semicolons)
                    LOG_ERROR("found a #do_not_split_on_semicolons after an earlier #do_not_split_on_semicolons directive!");
                do_not_split_on_semicolons = true;
            } else if (directive == "end_do_not_split_on_semicolons") {
                if (not do_not_split_on_semicolons)
                    LOG_ERROR("found an #end_do_not_split_on_semicolons w/o an earlier #do_not_split_on_semicolons directive!");
                StringUtil::TrimWhite(&statement);
                if (not statement.empty())
                    statements.emplace_back(statement);
                statement.clear();
                do_not_split_on_semicolons = false;
                continue;
            } else
                LOG_ERROR("unknown directive #" + directive + "!");
        } else if (unlikely(not do_not_split_on_semicolons and *ch == ';')) {
            StringUtil::TrimWhite(&statement);
            if (not statement.empty())
                statements.emplace_back(statement);
            statement.clear();
            continue;
        } else if (unlikely(*ch == '#'))
            comment_flavour = END_OF_LINE_COMMENT;
        else if (*ch == '/' and ch + 1 < query.cend() and *(ch + 1) == '*') { // Check for comments starting with "/*".
            statement += *ch;
            ++ch;
            comment_flavour = C_STYLE_COMMENT;
        } else if (*ch == '-' and ch + 2 < query.cend() and *(ch + 1) == '-' and *(ch + 2) == ' ') { // Check for comments starting with "-- ".
            statement += *ch;
            ++ch;
            statement += *ch;
            ++ch;
            comment_flavour = END_OF_LINE_COMMENT;
        } else {
            if (*ch == '\'' or *ch == '"')
                current_quote = *ch;
        }

        statement += *ch;
    }

    if (unlikely(do_not_split_on_semicolons))
        LOG_ERROR("found #do_not_split_on_semicolons w/o #end_do_not_split_on_semicolons!");

    StringUtil::TrimWhite(&statement);
    if (not statement.empty())
        statements.emplace_back(statement);

    return statements;
}


std::vector<std::string> DbConnection::SplitPostgresStatements(const std::string &query) {
    // This may have to be refined in the future to account for differences in the syntax between MySQL and Psql:
    return SplitMySQLStatements(query);
}


const std::string DbConnection::DEFAULT_CONFIG_FILE_PATH(UBTools::GetTuelibPath() + "ub_tools.conf");


bool DbConnection::query(const std::string &query_statement) {
    if (MiscUtil::SafeGetEnv("UTIL_LOG_DEBUG") == "true")
        FileUtil::AppendString(UBTools::GetTueFindLogPath() + "sql_debug.log",
                               std::string(::program_invocation_name) + ": " +  query_statement + '\n');

    if (type_ == T_MYSQL) {
        const auto statements(SplitMySQLStatements(query_statement));
        for (const auto &statement : statements) {
            if (::mysql_query(&mysql_, statement.c_str()) != 0) {
                LOG_WARNING("Could not successfully execute statement \"" + statement + "\": SQL error code:"
                            + std::to_string(::mysql_errno(&mysql_)));
                return false;
            }
        }
	return true;
    } else if (type_ == T_SQLITE) {
        if (stmt_handle_ != nullptr) {
            const int result_code(::sqlite3_finalize(stmt_handle_));
            if (result_code != SQLITE_OK) {
                LOG_WARNING("failed to finalise an Sqlite3 statement! (" + getLastErrorMessage() + ", code was "
                            + std::to_string(result_code) + ")");
                return false;
            }
        }

        const char *rest;
        if (::sqlite3_prepare_v2(sqlite3_, query_statement.c_str(), query_statement.length(), &stmt_handle_, &rest)
            != SQLITE_OK)
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
    } else { // Postgres
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
}


void DbConnection::queryOrDie(const std::string &query_statement) {
    if (not query(query_statement))
        LOG_ERROR("in DbConnection::queryOrDie: \"" + query_statement + "\" failed: " + getLastErrorMessage());
}


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


bool DbConnection::queryFile(const std::string &filename) {
    std::string statements;
    if (not FileUtil::ReadString(filename, &statements))
        return false;

    if (type_ == T_MYSQL) {
        const auto individual_statements(SplitMySQLStatements(statements));
        for (const auto &statement : individual_statements) {
            if (::mysql_query(&mysql_, statement.c_str()) != 0) {
                LOG_WARNING("Could not successfully execute statement \"" + statement + "\": SQL error code:"
                            + std::to_string(::mysql_errno(&mysql_)));
                return false;
            }
        }
        return true;
    } else if (type_ == T_SQLITE) {
        std::vector<std::string> individual_statements;
        SplitSqliteStatements(statements, &individual_statements);
        for (const auto &statement : individual_statements) {
            if (not query(statement))
                return false;
        }

        return true;
    } else { // Postgres
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
}


void DbConnection::queryFileOrDie(const std::string &filename) {
    if (not queryFile(filename))
        LOG_ERROR("failed to execute statements from \"" + filename + "\"!");
}


bool DbConnection::backup(const std::string &output_filename, std::string * const err_msg) {
    if (type_ != T_SQLITE) {
        *err_msg = "only Sqlite is supported at this time!";
        return false;
    }

    sqlite3 *sqlite3_backup_file;
    int return_code;
    if ((return_code = ::sqlite3_open(output_filename.c_str(), &sqlite3_backup_file)) != SQLITE_OK) {
        *err_msg = "failed to create backup to \"" + output_filename + "\": " + std::string(::sqlite3_errmsg(sqlite3_backup_file));
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


void DbConnection::backupOrDie(const std::string &output_filename) {
    std::string err_msg;
    if (not backup(output_filename, &err_msg))
        LOG_ERROR(err_msg);
}


void DbConnection::insertIntoTableOrDie(const std::string &table_name,
                                        const std::map<std::string, std::string> &column_names_to_values_map,
                                        const DuplicateKeyBehaviour duplicate_key_behaviour,
                                        const std::string &where_clause)
{
    if (not where_clause.empty() and duplicate_key_behaviour != DKB_REPLACE)
        LOG_ERROR("\"where_clause\" is only valid when using the DKB_REPLACE mode!");

    std::string insert_stmt(duplicate_key_behaviour == DKB_REPLACE ? "REPLACE" : "INSERT");
    insert_stmt += " INTO " + table_name + " (";

    const char column_name_quote(type_ == T_MYSQL ? '`' : '"');

    bool first(true);
    for (const auto &column_name_and_value : column_names_to_values_map) {
        if (not first)
            insert_stmt += ',';
        else
            first = false;

        insert_stmt += column_name_quote;
        insert_stmt += column_name_and_value.first;
        insert_stmt += column_name_quote;
    }

    insert_stmt += ") VALUES (";

    first = true;
    for (const auto &column_name_and_value : column_names_to_values_map) {
        if (not first)
            insert_stmt += ',';
        else
            first = false;

        insert_stmt += escapeAndQuoteString(column_name_and_value.second);
    }

    insert_stmt += ')';

    if (not where_clause.empty())
        insert_stmt += " WHERE " + where_clause;

    queryOrDie(insert_stmt);
}


void DbConnection::insertIntoTableOrDie(const std::string &table_name, const std::vector<std::string> &column_names,
                                        const std::vector<std::vector<std::optional<std::string>>> &values,
                                        const DuplicateKeyBehaviour duplicate_key_behaviour, const std::string &where_clause)
{
    if (column_names.empty())
        LOG_ERROR("at least one column name must be provided!");

    if (not where_clause.empty() and duplicate_key_behaviour != DKB_REPLACE)
        LOG_ERROR("\"where_clause\" is only valid when using the DKB_REPLACE mode!");

    std::string insert_stmt(duplicate_key_behaviour == DKB_REPLACE ? "REPLACE" : "INSERT");
    insert_stmt += " INTO " + table_name + " (";

    const char column_name_quote(type_ == T_MYSQL ? '`' : '"');

    bool first(true);
    for (const auto &column_name : column_names) {
        if (not first)
            insert_stmt += ',';
        else
            first = false;

        insert_stmt += column_name_quote;
        insert_stmt += column_name;
        insert_stmt += column_name_quote;
    }

    insert_stmt += ") VALUES ";

    for (auto column_values(values.cbegin()); column_values != values.cend(); ++column_values) {
        insert_stmt += '(';
        first = true;
        for (const auto &column_value : *column_values) {
            if (not first)
                insert_stmt += ',';
            else
                first = false;

            insert_stmt += escapeAndQuoteNonEmptyStringOrReturnNull(*column_value);
        }
        insert_stmt += ')';

        if (column_values != values.cend() - 1)
            insert_stmt += ',';
    }

    if (not where_clause.empty())
        insert_stmt += " WHERE " + where_clause;

    queryOrDie(insert_stmt);
}


DbResultSet DbConnection::getLastResultSet() {
    if (type_ == T_MYSQL) {
        MYSQL_RES * const result_set(::mysql_store_result(&mysql_));
        if (result_set == nullptr)
            LOG_ERROR("::mysql_store_result() failed! (" + getLastErrorMessage() + ")");

        return DbResultSet(result_set);
    } else if (type_ == T_SQLITE) {
        const auto temp_handle(stmt_handle_);
        stmt_handle_ = nullptr;
        return DbResultSet(temp_handle);
    } else { // Postgres
        const auto temp_pg_result = pg_result_;
        pg_result_ = nullptr;
        return DbResultSet(temp_pg_result);
    }
}


std::string DbConnection::escapeString(const std::string &unescaped_string, const bool add_quotes,
                                       const bool return_null_on_empty_string)
{
    if (unescaped_string.empty() and return_null_on_empty_string)
        return "NULL";

    char * const buffer(reinterpret_cast<char *>(std::malloc(unescaped_string.size() * 2 + 1)));
    size_t escaped_length;

    if (sqlite3_ == nullptr)
        escaped_length = ::mysql_real_escape_string(&mysql_, buffer, unescaped_string.data(), unescaped_string.size());
    else {
        char *cp(buffer);
        for (char ch : unescaped_string) {
            if (ch == '\'')
                *cp++ = '\'';
            *cp++ = ch;
        }

        escaped_length = cp - buffer;
    }

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


std::string DbConnection::sqliteEscapeBlobData(const std::string &blob_data) {
    return "x'" + StringUtil::ToHexString(blob_data) + "'";
}


void DbConnection::setTimeZone(const TimeZone time_zone) {
    if (unlikely(type_ != T_MYSQL))
        LOG_ERROR("this only works for MySQL!");

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


void DbConnection::init(const std::string &database_name, const std::string &user, const std::string &passwd,
                        const std::string &host, const unsigned port, const Charset charset, const TimeZone time_zone)
{
    initialised_ = false;
    pg_conn_ = nullptr;
    pg_result_ = nullptr;
    sqlite3_ = nullptr;
    type_ = T_MYSQL;

    if (::mysql_init(&mysql_) == nullptr)
        LOG_ERROR("mysql_init() failed! (1)");

    if (::mysql_real_connect(&mysql_, host.c_str(), user.c_str(), passwd.c_str(), database_name.c_str(), port,
                             /* unix_socket = */nullptr, /* client_flag = */CLIENT_MULTI_STATEMENTS) == nullptr)

        // password is intentionally omitted here!
        LOG_ERROR("mysql_real_connect() failed! (" + getLastErrorMessage()
                  + ", host=\"" + host + "\", user=\"" + user + "\", passwd=\"" + passwd + "\", database_name=\""
                  + database_name + "\", port=" + std::to_string(port) + ")");
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


void DbConnection::init(const std::string &user, const std::string &passwd, const std::string &host, const unsigned port,
                        const Charset charset, const TimeZone time_zone)
{
    initialised_ = false;
    pg_conn_ = nullptr;
    pg_result_ = nullptr;
    sqlite3_ = nullptr;
    type_ = T_MYSQL;

    if (::mysql_init(&mysql_) == nullptr)
        LOG_ERROR("mysql_init() failed! (2)");

    if (::mysql_real_connect(&mysql_, host.c_str(), user.c_str(), passwd.c_str(), nullptr, port,
                             /* unix_socket = */nullptr, /* client_flag = */CLIENT_MULTI_STATEMENTS) == nullptr)
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


std::string DbConnection::CharsetToString(const Charset charset) {
    switch (charset) {
    case UTF8MB3:
        return "utf8";
    case UTF8MB4:
        return "utf8mb4";
    }

    __builtin_unreachable();
}


std::string DbConnection::CollationToString(const Collation collation) {
    switch (collation) {
    case UTF8MB3_BIN:
        return "utf8_bin";
    case UTF8MB4_BIN:
        return "utf8mb4_bin";
    }

    __builtin_unreachable();
}


bool DbConnection::mySQLDatabaseExists(const std::string &database_name) {
    std::vector<std::string> databases(mySQLGetDatabaseList());
    return (std::find(databases.begin(), databases.end(), database_name) != databases.end());
}


bool DbConnection::tableExists(const std::string &database_name, const std::string &table_name) {
    if (type_ == T_MYSQL) {
        queryOrDie("SELECT EXISTS(SELECT * FROM information_schema.tables WHERE table_schema = '" + escapeString(database_name)
                   + "' AND table_name = '" + escapeString(table_name) + "')");
        DbResultSet result_set(getLastResultSet());
        return result_set.getNextRow()[0] == "1";
    } else if (type_ == T_SQLITE) {
        DbConnection connection(database_name, READONLY);
        connection.queryOrDie("SELECT name FROM sqlite_master WHERE type='table' AND name='"
                              + connection.escapeString(table_name) + "'");
        return not connection.getLastResultSet().empty();
    } else // Postgres
        LOG_ERROR("not yet implemented for Postgres!");
}


bool DbConnection::mySQLDropDatabase(const std::string &database_name) {
    if (not mySQLDatabaseExists(database_name))
        return false;
    queryOrDie("DROP DATABASE " + database_name + ";");
    return (not mySQLDatabaseExists(database_name));
}


std::vector<std::string> DbConnection::mySQLGetDatabaseList() {
    queryOrDie("SHOW DATABASES;");

    std::vector<std::string> databases;
    DbResultSet result_set(getLastResultSet());
    while (const DbRow result_row = result_set.getNextRow())
        databases.emplace_back(result_row[0]);

    return databases;
}


std::vector<std::string> DbConnection::mySQLGetTableList() {
    queryOrDie("SHOW TABLES;");

    std::vector<std::string> tables;
    DbResultSet result_set(getLastResultSet());
    while (const DbRow result_row = result_set.getNextRow())
        tables.emplace_back(result_row[0]);

    return tables;
}


void DbConnection::MySQLImportFile(const std::string &sql_file, const std::string &database_name, const std::string &user,
                                   const std::string &passwd, const std::string &host, const unsigned port, const Charset charset)
{
    DbConnection db_connection(database_name, user, passwd, host, port, charset);
    db_connection.queryFileOrDie(sql_file);
}


std::string DbConnection::MySQLPrivilegeToString(const DbConnection::MYSQL_PRIVILEGE mysql_privilege) {
    switch (mysql_privilege) {
    case P_SELECT:
        return "SELECT";
    case P_INSERT:
        return "INSERT";
    case P_UPDATE:
        return "UPDATE";
    case P_DELETE:
        return "DELETE";
    case P_CREATE:
        return "CREATE";
    case P_DROP:
        return "DROP";
    case P_REFERENCES:
        return "REFERENCES";
    case P_INDEX:
        return "INDEX";
    case P_ALTER:
        return "ALTER";
    case P_CREATE_TEMPORARY_TABLES:
        return "CREATE_TEMPORARY_TABLES";
    case P_LOCK_TABLES:
        return "LOCK_TABLES";
    case P_EXECUTE:
        return "EXECUTE";
    case P_CREATE_VIEW:
        return "CREATE_VIEW";
    case P_SHOW_VIEW:
        return "SHOW_VIEW";
    case P_CREATE_ROUTINE:
        return "CREATE_ROUTINE";
    case P_ALTER_ROUTINE:
        return "ALTER_ROUTINE";
    case P_EVENT:
        return "EVENT";
    case P_TRIGGER:
        return "TRIGGER";
    }
}


bool DbConnection::mySQLUserExists(const std::string &user, const std::string &host) {
    queryOrDie("SELECT COUNT(*) as user_count FROM mysql.user WHERE User='" + user + "' AND Host='" + host + "';");
    DbResultSet result_set(getLastResultSet());
    const DbRow result_row(result_set.getNextRow());
    return result_row["user_count"] != "0";
}


DbConnection *DbConnection::PostgresFactory(std::string * const error_message, const std::string &database_name,
                                            const std::string &user_name, const std::string &password,
                                            const std::string &hostname, const unsigned port,
                                            const std::string &options)
{
    const auto port_as_string(StringUtil::ToString(port));
    const auto pg_conn(::PQsetdbLogin(hostname.c_str(), port_as_string.c_str(), options.c_str(), /* pgtty = */options.c_str(),
                                      database_name.c_str(), user_name.c_str(), password.c_str()));

    if (PQstatus(pg_conn) == CONNECTION_OK) {
        error_message->clear();
        if (::PQsetClientEncoding(pg_conn, "UTF8") != 0) {
            *error_message = "failed to set the client encoding to UTF8!";
            ::PQfinish(pg_conn);
            return nullptr;
        }
        return new DbConnection(pg_conn, user_name, password, hostname, port);
    } else {
        *error_message = ::PQerrorMessage(pg_conn);
        ::PQfinish(pg_conn);
        return nullptr;
    }
}


const std::unordered_set<DbConnection::MYSQL_PRIVILEGE> DbConnection::MYSQL_ALL_PRIVILEGES {
    P_SELECT,
    P_INSERT,
    P_UPDATE,
    P_DELETE,
    P_CREATE,
    P_DROP,
    P_REFERENCES,
    P_INDEX,
    P_ALTER,
    P_CREATE_TEMPORARY_TABLES,
    P_LOCK_TABLES,
    P_EXECUTE,
    P_CREATE_VIEW,
    P_SHOW_VIEW,
    P_CREATE_ROUTINE,
    P_ALTER_ROUTINE,
    P_EVENT,
    P_TRIGGER
};


static const std::unordered_map<std::string, DbConnection::MYSQL_PRIVILEGE> string_to_privilege_map {
    { "SELECT", DbConnection::P_SELECT },
    { "INSERT", DbConnection::P_INSERT },
    { "UPDATE", DbConnection::P_UPDATE },
    { "DELETE", DbConnection::P_DELETE },
    { "CREATE", DbConnection::P_CREATE },
    { "DROP", DbConnection::P_DROP },
    { "REFERENCES", DbConnection::P_REFERENCES },
    { "INDEX", DbConnection::P_INDEX },
    { "ALTER", DbConnection::P_ALTER },
    { "CREATE TEMPORARY TABLES", DbConnection::P_CREATE_TEMPORARY_TABLES},
    { "LOCK TABLES", DbConnection::P_LOCK_TABLES },
    { "EXECUTE", DbConnection::P_EXECUTE },
    { "CREATE VIEW", DbConnection::P_CREATE_VIEW },
    { "SHOW VIEW", DbConnection::P_SHOW_VIEW },
    { "CREATE ROUTINE", DbConnection::P_CREATE_ROUTINE },
    { "ALTER ROUTINE", DbConnection::P_ALTER_ROUTINE },
    { "EVENT", DbConnection::P_EVENT},
    { "TRIGGER", DbConnection::P_TRIGGER },
};


DbConnection::MYSQL_PRIVILEGE MySQLPrivilegeStringToEnum(const std::string &candidate) {
    const auto string_and_privilege(string_to_privilege_map.find(candidate));
    if (unlikely(string_and_privilege == string_to_privilege_map.end()))
        LOG_ERROR(candidate + " is not in our map!");
    return string_and_privilege->second;
}


std::string MySQLPrivilegeEnumToString(const DbConnection::MYSQL_PRIVILEGE privilege) {
    for (const auto &string_and_privilege : string_to_privilege_map) {
        if (string_and_privilege.second == privilege)
            return string_and_privilege.first;
    }

    LOG_ERROR("Privilege " + std::to_string(privilege) + " is not in our map!");
}


std::unordered_set<DbConnection::MYSQL_PRIVILEGE> DbConnection::mySQLGetUserPrivileges(const std::string &user,
                                                                                       const std::string &database_name,
                                                                                       const std::string &host)
{
    const std::string QUERY("SHOW GRANTS FOR " + user + "@" + host + ";");
    if (not query(QUERY)) {
        // catch "No such privileges defined" error and return empty set
        if (getLastErrorCode() == 1141)
            return {};
        LOG_ERROR(QUERY + " failed: " + getLastErrorMessage());
    }

    DbResultSet result_set(getLastResultSet());
    while (const auto row = result_set.getNextRow()) {
        static RegexMatcher * const mysql_privileges_matcher(
            RegexMatcher::RegexMatcherFactory("GRANT (.+) ON [`']?" + database_name + "[`']?.* TO ['`]?" + user
                                              + "['`]?@['`]?" + host + "['`]?"));

        if (not mysql_privileges_matcher->matched(row[0]))
            LOG_WARNING("unexpectedly, no privileges were extracted from " + row[0]);
        else {
            const std::string matched_privileges((*mysql_privileges_matcher)[1]);
            if (matched_privileges == "ALL PRIVILEGES")
                return MYSQL_ALL_PRIVILEGES;

            std::unordered_set<std::string> privileges_strings;
            StringUtil::SplitThenTrimWhite(matched_privileges, ',', &privileges_strings);

            std::unordered_set<DbConnection::MYSQL_PRIVILEGE> privileges;
            for (const auto &privilege_string : privileges_strings)
                privileges.emplace(MySQLPrivilegeStringToEnum(privilege_string));

            return privileges;
        }
    }

    return {};
}


unsigned DbTransaction::active_count_;


DbTransaction::DbTransaction(DbConnection * const db_connection, const bool rollback_when_exceptions_are_in_flight)
    : db_connection_(*db_connection), rollback_when_exceptions_are_in_flight_(rollback_when_exceptions_are_in_flight),
      explicit_commit_or_rollback_has_happened_(false)
{
    if (active_count_ > 0)
        LOG_ERROR("no nested transactions are allowed!");
    ++active_count_;

    if (db_connection_.getType() == DbConnection::T_SQLITE)
        db_connection_.queryOrDie("BEGIN TRANSACTION");
    else {
        db_connection_.queryOrDie("SHOW VARIABLES LIKE 'autocommit'");
        DbResultSet result_set(db_connection_.getLastResultSet());
        if (unlikely(result_set.empty()))
            LOG_ERROR("this should never happen!");

        const std::string autocommit_status(result_set.getNextRow()["Value"]);
        if (autocommit_status == "ON") {
            autocommit_was_on_ = true;
            db_connection_.queryOrDie("SET autocommit=OFF");
        } else if (autocommit_status == "OFF")
            autocommit_was_on_ = false;
        else
            LOG_ERROR("unknown autocommit status \"" + autocommit_status + "\"!");
        db_connection_.queryOrDie("START TRANSACTION");
    }
}


DbTransaction::~DbTransaction() {
    if (not explicit_commit_or_rollback_has_happened_) {
        if (std::uncaught_exceptions() == 0)
            db_connection_.queryOrDie("COMMIT");
        else if (rollback_when_exceptions_are_in_flight_)
            db_connection_.queryOrDie("ROLLBACK");
    }

    if (db_connection_.getType() == DbConnection::T_MYSQL and autocommit_was_on_)
        db_connection_.queryOrDie("SET autocommit=ON");
    --active_count_;
}


void DbTransaction::commit() {
    db_connection_.queryOrDie("COMMIT");
    explicit_commit_or_rollback_has_happened_ = true;
}


void DbTransaction::rollback() {
    db_connection_.queryOrDie("ROLLBACK");
    explicit_commit_or_rollback_has_happened_ = true;
}
