/** \file   DbConnection.cc
 *  \brief  Implementation of the DbConnection class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "IniFile.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "UBTools.h"
#include "util.h"


DbConnection::DbConnection(const std::string &mysql_url, const Charset charset, const TimeZone time_zone)
    : sqlite3_(nullptr), stmt_handle_(nullptr)
{
    static RegexMatcher * const mysql_url_matcher(
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


DbConnection::DbConnection(const std::string &database_path, const OpenMode open_mode): type_(T_SQLITE), stmt_handle_(nullptr) {
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
    stmt_handle_ = nullptr;
    initialised_ = true;
}


DbConnection::~DbConnection() {
    if (initialised_) {
        if (type_ == T_MYSQL)
            ::mysql_close(&mysql_);
        else {
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
}


int DbConnection::getLastErrorCode() const {
    if (type_ == T_MYSQL)
        return static_cast<int>(::mysql_errno(&mysql_));
    else
        return ::sqlite3_errcode(sqlite3_);
}


const std::string DbConnection::DEFAULT_CONFIG_FILE_PATH(UBTools::GetTuelibPath() + "ub_tools.conf");


bool DbConnection::query(const std::string &query_statement) {
    if (MiscUtil::SafeGetEnv("UTIL_LOG_DEBUG") == "true")
        FileUtil::AppendString("/usr/local/var/log/tuefind/sql_debug.log",
                               std::string(::progname) + ": " +  query_statement + '\n');

    if (type_ == T_MYSQL) {
        if (::mysql_query(&mysql_, query_statement.c_str()) != 0) {
            LOG_WARNING("Could not successfully execute statement \"" + query_statement + "\": SQL error code:"
                        + std::to_string(::mysql_errno(&mysql_)));
            return false;
        }
	return true;
    } else {
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
        ::mysql_set_server_option(&mysql_, MYSQL_OPTION_MULTI_STATEMENTS_ON);
        const bool query_result(query(StringUtil::TrimWhite(&statements)));
        if (query_result)
            mySQLSyncMultipleResults();
        ::mysql_set_server_option(&mysql_, MYSQL_OPTION_MULTI_STATEMENTS_OFF);
        return query_result;
    } else {
        std::vector<std::string> individual_statements;
        SplitSqliteStatements(statements, &individual_statements);
        for (const auto &statement : individual_statements) {
            if (not query(statement))
                return false;
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
                                        const DuplicateKeyBehaviour duplicate_key_behaviour)
{
    std::string insert_stmt(duplicate_key_behaviour == DKB_REPLACE ? "REPLACE" : "INSERT");
    if (duplicate_key_behaviour == DKB_IGNORE)
        insert_stmt += (type_ == T_MYSQL) ? " IGNORE" : " OR IGNORE";
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

    queryOrDie(insert_stmt);
}


DbResultSet DbConnection::getLastResultSet() {
    if (sqlite3_ == nullptr) {
        MYSQL_RES * const result_set(::mysql_store_result(&mysql_));
        if (result_set == nullptr)
            throw std::runtime_error("in DbConnection::getLastResultSet: mysql_store_result() failed! ("
                                     + getLastErrorMessage() + ")");

        return DbResultSet(result_set);
    } else {
        const auto temp_handle(stmt_handle_);
        stmt_handle_ = nullptr;
        return DbResultSet(temp_handle);
    }
}


std::string DbConnection::escapeString(const std::string &unescaped_string, const bool add_quotes) {
    char * const buffer(reinterpret_cast<char * const>(std::malloc(unescaped_string.size() * 2 + 1)));
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


void DbConnection::setTimeZone(const TimeZone time_zone) {
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
    sqlite3_ = nullptr;
    type_ = T_MYSQL;

    if (::mysql_init(&mysql_) == nullptr)
        throw std::runtime_error("in DbConnection::init: mysql_init() failed!");

    if (::mysql_real_connect(&mysql_, host.c_str(), user.c_str(), passwd.c_str(), database_name.c_str(), port,
                             /* unix_socket = */nullptr, /* client_flag = */CLIENT_MULTI_STATEMENTS) == nullptr)

        // password is intentionally omitted here!
        throw std::runtime_error("in DbConnection::init: mysql_real_connect() failed! (" + getLastErrorMessage()
                                 + ", host=\"" + host + "\", user=\"" + user + "\", passwd=\"********\", database_name=\""
                                 + database_name + "\", port=" + std::to_string(port) + ")");
    if (::mysql_set_character_set(&mysql_, (charset == UTF8MB4) ? "utf8mb4" : "utf8") != 0)
        throw std::runtime_error("in DbConnection::init: mysql_set_character_set() failed! (" + getLastErrorMessage() + ")");
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
    sqlite3_ = nullptr;
    type_ = T_MYSQL;

    if (::mysql_init(&mysql_) == nullptr)
        throw std::runtime_error("in DbConnection::init: mysql_init() failed!");

    if (::mysql_real_connect(&mysql_, host.c_str(), user.c_str(), passwd.c_str(), nullptr, port,
                             /* unix_socket = */nullptr, /* client_flag = */CLIENT_MULTI_STATEMENTS) == nullptr)
        throw std::runtime_error("in DbConnection::init: mysql_real_connect() failed! (" + getLastErrorMessage() + ")");
    if (::mysql_set_character_set(&mysql_, CharsetToString(charset).c_str()) != 0)
        throw std::runtime_error("in DbConnection::init: mysql_set_character_set() failed! (" + getLastErrorMessage() + ")");

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
    } else { // Sqlite
        DbConnection connection(database_name, READONLY);
        connection.queryOrDie("SELECT name FROM sqlite_master WHERE type='table' AND name='" + connection.escapeString(table_name) + "'");
        return not connection.getLastResultSet().empty();
    }
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


void DbConnection::mySQLSyncMultipleResults() {
    int next_result_exists;
    do {
        MYSQL_RES * const result_set(::mysql_store_result(&mysql_));
        if (result_set != nullptr)
            ::mysql_free_result(result_set);
        next_result_exists = ::mysql_next_result(&mysql_);
    } while (next_result_exists == 0);
}


void DbConnection::MySQLImportFile(const std::string &sql_file, const std::string &database_name, const std::string &user,
                                   const std::string &passwd, const std::string &host, const unsigned port, const Charset charset)
{
    DbConnection db_connection(database_name, user, passwd, host, port, charset);
    db_connection.queryFileOrDie(sql_file);
}


bool DbConnection::mySQLUserExists(const std::string &user, const std::string &host) {
    queryOrDie("SELECT COUNT(*) as user_count FROM mysql.user WHERE User='" + user + "' AND Host='" + host + "';");
    DbResultSet result_set(getLastResultSet());
    const DbRow result_row(result_set.getNextRow());
    return result_row["user_count"] != "0";
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


std::unordered_set<DbConnection::MYSQL_PRIVILEGE> DbConnection::mySQLGetUserPrivileges(const std::string &user, const std::string &database_name,
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
        if (row[0] == "GRANT ALL PRIVILEGES ON `" + database_name + "`.* TO '" + user + "'@'" + host + "'")
            return MYSQL_ALL_PRIVILEGES;
        static RegexMatcher * const mysql_privileges_matcher(
            RegexMatcher::RegexMatcherFactory("GRANT (.+) ON `" + database_name + "`.* TO '" + user + "'@'" + host + "'"));

        if (mysql_privileges_matcher->matched(row[0])) {
            const std::string matched_privileges((*mysql_privileges_matcher)[1]);
            if (matched_privileges == "ALL PRIVILEGES")
                return MYSQL_ALL_PRIVILEGES;

            std::unordered_set<std::string> privileges_strings;
            StringUtil::SplitThenTrimWhite(matched_privileges, ",", &privileges_strings);

            std::unordered_set<DbConnection::MYSQL_PRIVILEGE> privileges;
            for (const auto &privilege_string : privileges_strings)
                privileges.emplace(MySQLPrivilegeStringToEnum(privilege_string));

            return privileges;
        }
    }

    return {};
}
