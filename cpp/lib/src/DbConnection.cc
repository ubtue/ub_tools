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
#include "MySQLDbConnection.h"
#include "PostgresDbConnection.h"
#include "RegexMatcher.h"
#include "Sqlite3DbConnection.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "UBTools.h"
#include "util.h"
#include "VuFind.h"


DbConnection::DbConnection(DbConnection &&other) {
    if (unlikely(this == &other))
        return;

    delete db_connection_;
    db_connection_ = other.db_connection_;
    initialised_   = other.initialised_;
    other.db_connection_ = nullptr;
    other.initialised_ = false;
}


DbConnection DbConnection::UBToolsFactory(const TimeZone time_zone) {
    return DbConnection(new MySQLDbConnection(time_zone));
}


DbConnection DbConnection::MySQLFactory(const std::string &database_name, const std::string &user, const std::string &passwd,
                                        const std::string &host, const unsigned port, const Charset charset,
                                        const TimeZone time_zone)
{
    return DbConnection(new MySQLDbConnection(database_name, user, passwd, host, port, charset, time_zone));
}


DbConnection DbConnection::MySQLFactory(const IniFile &ini_file, const std::string &ini_file_section, const TimeZone time_zone) {
    return DbConnection(new MySQLDbConnection(ini_file, ini_file_section, time_zone));
}


DbConnection DbConnection::MySQLFactory(const std::string &mysql_url, const Charset charset, const TimeZone time_zone) {
    return DbConnection(new MySQLDbConnection(mysql_url, charset, time_zone));
}


DbConnection DbConnection::VuFindMySQLFactory() {
    return DbConnection(new MySQLDbConnection(VuFind::GetMysqlURLOrDie(), UTF8MB4, TZ_SYSTEM));
}


DbConnection DbConnection::Sqlite3Factory(const std::string &database_path, const OpenMode open_mode) {
    return DbConnection(new Sqlite3DbConnection(database_path, open_mode));
}


DbConnection DbConnection::PostgresFactory(std::string * const error_message, const std::string &database_name,
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
        return DbConnection(new PostgresDbConnection(pg_conn, user_name, password, hostname, port));
    } else {
        *error_message = ::PQerrorMessage(pg_conn);
        ::PQfinish(pg_conn);
        return nullptr;
    }
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


void DbConnection::queryOrDie(const std::string &query_statement) {
    if (not query(query_statement))
        LOG_ERROR("in DbConnection::queryOrDie: \"" + query_statement + "\" failed: " + getLastErrorMessage());
}


void DbConnection::queryFileOrDie(const std::string &filename) {
    if (not queryFile(filename))
        LOG_ERROR("failed to execute statements from \"" + filename + "\"!");
}


DbResultSet DbConnection::selectOrDie(const std::string &select_statement) {
    queryOrDie(select_statement);
    return getLastResultSet();
}


unsigned DbConnection::countOrDie(const std::string &select_statement, const std::string &count_variable_name) {
    auto result_set(selectOrDie(select_statement));
    if (result_set.size() != 1)
        LOG_ERROR("Invalid result set size for query: " + select_statement);

    const auto row = result_set.getNextRow();
    return StringUtil::ToUnsigned(row[count_variable_name]);
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

    const char column_name_quote(getType() == T_MYSQL ? '`' : '"');

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

    const char column_name_quote(getType() == T_MYSQL ? '`' : '"');

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


void DbConnection::sqliteResetDatabase(const std::string &script_path) {
    Sqlite3DbConnection * const sqlite3_db_connection(dynamic_cast<Sqlite3DbConnection *>(db_connection_));
    if (unlikely(sqlite3_db_connection == nullptr))
        LOG_ERROR("you can only call this on a Sqlite3 DbConnection!");

    const std::string database_path(sqlite3_db_connection->getDatabasePath());
    delete db_connection_;
    ::unlink(database_path.c_str());
    db_connection_ = new Sqlite3DbConnection(database_path, DbConnection::CREATE);

    if (not script_path.empty())
        db_connection_->queryFileOrDie(script_path);
}


std::string DbConnection::sqliteEscapeBlobData(const std::string &blob_data) {
    if (unlikely(getType() != T_SQLITE))
        LOG_ERROR("you can only call this on a Sqlite3 DbConnection!");
    return "x'" + StringUtil::ToHexString(blob_data) + "'";
}


std::string DbConnection::mySQLGetDbName() const {
    const auto mysql_db_connection(dynamic_cast<MySQLDbConnection *>(db_connection_));
    if (unlikely(mysql_db_connection == nullptr))
        LOG_ERROR("we need a MySQLDbConnection here!");

    return mysql_db_connection->getDbName();
}


std::string DbConnection::mySQLGetUser() const {
    const auto mysql_db_connection(dynamic_cast<MySQLDbConnection *>(db_connection_));
    if (unlikely(mysql_db_connection == nullptr))
        LOG_ERROR("we need a MySQLDbConnection here!");

    return mysql_db_connection->getUser();
}


std::string DbConnection::mySQLGetPasswd() const {
    const auto mysql_db_connection(dynamic_cast<MySQLDbConnection *>(db_connection_));
    if (unlikely(mysql_db_connection == nullptr))
        LOG_ERROR("we need a MySQLDbConnection here!");

    return mysql_db_connection->getPasswd();
}


std::string DbConnection::mySQLGetHost() const {
    const auto mysql_db_connection(dynamic_cast<MySQLDbConnection *>(db_connection_));
    if (unlikely(mysql_db_connection == nullptr))
        LOG_ERROR("we need a MySQLDbConnection here!");

    return mysql_db_connection->getHost();
}


unsigned DbConnection::mySQLGetPort() const {
    const auto mysql_db_connection(dynamic_cast<MySQLDbConnection *>(db_connection_));
    if (unlikely(mysql_db_connection == nullptr))
        LOG_ERROR("we need a MySQLDbConnection here!");

    return mysql_db_connection->getPort();
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


void DbConnection::MySQLCreateDatabase(const std::string &database_name, const std::string &admin_user,
                                       const std::string &admin_passwd, const std::string &host,
                                       const unsigned port, const Charset charset, const Collation collation)
{
    MySQLDbConnection db_connection(admin_user, admin_passwd, host, port, charset);
    db_connection.mySQLCreateDatabase(database_name, charset, collation);
}


void DbConnection::MySQLCreateUser(const std::string &new_user, const std::string &new_passwd, const std::string &admin_user,
                                   const std::string &admin_passwd, const std::string &host, const unsigned port,
                                   const Charset charset)
{
        MySQLDbConnection db_connection(admin_user, admin_passwd, host, port, charset);
        db_connection.mySQLCreateUser(new_user, new_passwd, host);
}


void DbConnection::MySQLCreateUserIfNotExists(const std::string &new_user, const std::string &new_passwd,
                                              const std::string &admin_user, const std::string &admin_passwd,
                                              const std::string &host, const unsigned port, const Charset charset)
{
    MySQLDbConnection db_connection(admin_user, admin_passwd, host, port, charset);
    db_connection.mySQLCreateUserIfNotExists(new_user, new_passwd, host);
}


bool DbConnection::MySQLDatabaseExists(const std::string &database_name, const std::string &admin_user,
                                       const std::string &admin_passwd, const std::string &host, const unsigned port,
                                       const Charset charset)
{
    MySQLDbConnection db_connection(admin_user, admin_passwd, host, port, charset);
    return db_connection.mySQLDatabaseExists(database_name);
}


bool DbConnection::MySQLDropDatabase(const std::string &database_name, const std::string &admin_user,
                                     const std::string &admin_passwd, const std::string &host, const unsigned port,
                                     const Charset charset)
{
    MySQLDbConnection db_connection(admin_user, admin_passwd, host, port, charset);
    return db_connection.mySQLDropDatabase(database_name);
}


std::vector<std::string> DbConnection::MySQLGetDatabaseList(const std::string &admin_user, const std::string &admin_passwd,
                                                            const std::string &host, const unsigned port, const Charset charset)
{
    MySQLDbConnection db_connection(admin_user, admin_passwd, host, port, charset);
    return db_connection.mySQLGetDatabaseList();
}


void DbConnection::MySQLGrantAllPrivileges(const std::string &database_name, const std::string &database_user,
                                           const std::string &admin_user, const std::string &admin_passwd,
                                           const std::string &host, const unsigned port, const Charset charset)
{
    MySQLDbConnection db_connection(admin_user, admin_passwd, host, port, charset);
    return db_connection.mySQLGrantAllPrivileges(database_name, database_user, host);
}


bool DbConnection::MySQLUserExists(const std::string &database_user, const std::string &admin_user,
                                   const std::string &admin_passwd, const std::string &host, const unsigned port,
                                   const Charset charset)
{
    MySQLDbConnection db_connection(admin_user, admin_passwd, host, port, charset);
    return db_connection.mySQLUserExists(database_user, host);
}


void DbConnection::MySQLImportFile(const std::string &sql_file, const std::string &database_name, const std::string &user,
                                   const std::string &passwd, const std::string &host, const unsigned port,
                                   const Charset charset, const TimeZone time_zone)
{
    MySQLDbConnection db_connection(database_name, user, passwd, host, port, charset, time_zone);
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


std::string DbConnection::PostgresGetUser() const {
    const auto mysql_db_connection(dynamic_cast<PostgresDbConnection *>(db_connection_));
    if (unlikely(mysql_db_connection == nullptr))
        LOG_ERROR("we need a PostgresDbConnection here!");

    return mysql_db_connection->getUser();
}


std::string DbConnection::PostgresGetPasswd() const {
    const auto mysql_db_connection(dynamic_cast<PostgresDbConnection *>(db_connection_));
    if (unlikely(mysql_db_connection == nullptr))
        LOG_ERROR("we need a PostgresDbConnection here!");

    return mysql_db_connection->getPasswd();
}


unsigned DbConnection::PostgresGetPort() const {
    const auto mysql_db_connection(dynamic_cast<PostgresDbConnection *>(db_connection_));
    if (unlikely(mysql_db_connection == nullptr))
        LOG_ERROR("we need a PostgresDbConnection here!");

    return mysql_db_connection->getPort();
}


std::string DbConnection::TypeToString(const Type type) {
    switch (type) {
    case T_MYSQL:
        return "T_MYSQL";
    case T_SQLITE:
        return "T_SQLITE";
    case T_POSTGRES:
        return "T_POSTGRES";
    default:
        LOG_ERROR("unknown database type: " + std::to_string(type) + "!");
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
            RegexMatcher::RegexMatcherFactory("GRANT (.+) ON [`']?([^`'.]+)[`']?\\.\\* TO ['`]?([^`'@]+)['`]?@['`]?([^`' ]+)['`]?"));

        if (not mysql_privileges_matcher->matched(row[0]))
            LOG_WARNING("unexpectedly, no privileges were extracted from " + row[0]);
        else {
            const std::string matched_privileges((*mysql_privileges_matcher)[1]);
            const std::string matched_database((*mysql_privileges_matcher)[2]);
            const std::string matched_user((*mysql_privileges_matcher)[3]);
            const std::string matched_host((*mysql_privileges_matcher)[4]);

            if (matched_user != user or matched_database != database_name)
                continue;

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

    const auto connection_type(db_connection_.getType());
    if (connection_type == DbConnection::T_SQLITE)
        db_connection_.queryOrDie("BEGIN TRANSACTION");
    else if (connection_type == DbConnection::T_MYSQL) {
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
    } else if (connection_type == DbConnection::T_POSTGRES)
        db_connection_.queryOrDie("BEGIN");
    else
        LOG_ERROR("unknown connection type: " + DbConnection::TypeToString(connection_type) + "!");
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
