/** \file   DbConnection.h
 *  \brief  Interface for the DbConnection class.
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
#pragma once


#include <string>
#include <vector>
#include <libpq-fe.h>
#include "DbResultSet.h"
#include "MiscUtil.h"
#include "util.h"


// Forward declarations:
class IniFile;
class MySQLDbConnection;
class Sqlite3DbConnection;
class PostgresDbConnection;


class DbConnection {
public:
    enum Charset { UTF8MB3, UTF8MB4 };
    enum Collation { UTF8MB3_BIN, UTF8MB4_BIN };
    enum DuplicateKeyBehaviour { DKB_FAIL, DKB_REPLACE };
    enum OpenMode { READONLY, READWRITE, CREATE /* = create if not exists */ };
    enum TimeZone { TZ_SYSTEM, TZ_UTC };
    enum Type { T_MYSQL, T_SQLITE, T_POSTGRES };
    enum MYSQL_PRIVILEGE {
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
    static const std::unordered_set<MYSQL_PRIVILEGE> MYSQL_ALL_PRIVILEGES;
    static const std::string DEFAULT_CONFIG_FILE_PATH;

private:
    DbConnection *db_connection_;

protected:
    bool initialised_;

public:
    DbConnection(DbConnection &&other);

    // \return An unusable DbConnection, in fact, all you can do on it is call isNullConnection().
    static DbConnection NullFactory() { return DbConnection(); }

    static DbConnection UBToolsFactory(const TimeZone time_zone = TZ_SYSTEM); // Uses the ub_tools database.

    static DbConnection MySQLFactory(const std::string &database_name, const std::string &user, const std::string &passwd = "",
                                     const std::string &host = "localhost", const unsigned port = MYSQL_PORT,
                                     const Charset charset = UTF8MB4, const TimeZone time_zone = TZ_SYSTEM);

    // Expects to find entries named "sql_database", "sql_username" and "sql_password".  Optionally there may also
    // be an entry named "sql_host".  If this entry is missing a default value of "localhost" will be assumed.
    // Another optional entry is "sql_port".  If that entry is missing the default value MYSQL_PORT will be used.
    static DbConnection MySQLFactory(const IniFile &ini_file, const std::string &ini_file_section = "Database",
                                     const TimeZone time_zone = TZ_SYSTEM);

    /** \brief Attemps to parse something like "mysql://ruschein:xfgYu8z@localhost:3345/vufind" */
    static DbConnection MySQLFactory(const std::string &mysql_url, const Charset charset = UTF8MB4, const TimeZone time_zone = TZ_SYSTEM);

    // \return A connection to the VuFind MySQL database.
    static DbConnection VuFindMySQLFactory();

    // Creates or opens an Sqlite3 database.
    static DbConnection Sqlite3Factory(const std::string &database_path, const OpenMode open_mode);

    /** \brief   Creates a Postgres database connection.
     *  \param   options  See https://www.postgresql.org/docs/9.4/runtime-config.html for a huge list.
     *  \returns NULL if no database connection could be established and sets error_message.
     */
    static DbConnection PostgresFactory(std::string * const error_message, const std::string &database_name = "",
                                        const std::string &user_name = "", const std::string &password = "",
                                        const std::string &hostname = "localhost", const unsigned port = 5432,
                                        const std::string &options = "");

    static DbConnection PostgresFactory(const IniFile &ini_file, const std::string &ini_file_section = "Database");

    inline virtual ~DbConnection() {
        delete db_connection_;
        db_connection_ = nullptr;
    }

    inline bool isNullConnection() const { return db_connection_ == nullptr; }
    inline virtual Type getType() const { return db_connection_->getType(); }

    /** \warning You must not call this for Postgres as it doesn't support the notion of a purely numeric error code! */
    virtual int getLastErrorCode() const { return db_connection_->getLastErrorCode(); }

    /** \note If the environment variable "UTIL_LOG_DEBUG" has been set "true", query statements will be
     *        logged to /usr/local/var/log/tuefind/sql_debug.log.
     */
    inline virtual bool query(const std::string &query_statement) { return db_connection_->query(query_statement); }

    /** \brief Executes an SQL statement and aborts printing an error message to stderr if an error occurred.
     *  \note If the environment variable "UTIL_LOG_DEBUG" has been set "true", query statements will be
     *        logged to /usr/local/var/log/tuefind/sql_debug.log.
     */
    void queryOrDie(const std::string &query_statement);

    /** \brief Reads SQL statements from "filename" and executes them.
     *  \note  Aborts if "filename" can't be read.
     *  \note  If the environment variable "UTIL_LOG_DEBUG" has been set "true", query statements will be
     *         logged to /usr/local/var/log/tuefind/sql_debug.log.
     */
    inline virtual bool queryFile(const std::string &filename) { return db_connection_->queryFile(filename); }

    /** \brief Reads SQL statements from "filename" and executes them.
     *  \note  Aborts printing an error message to stderr if an error occurred.
     *  \note  If the environment variable "UTIL_LOG_DEBUG" has been set "true", query statements will be
     *         logged to /usr/local/var/log/tuefind/sql_debug.log.
     */
    void queryFileOrDie(const std::string &filename);

    /** \brief Similar to queryOrDie, but returns a DbResultSet, typically used for SELECT statements. */
    DbResultSet selectOrDie(const std::string &select_statement);

    /** \brief Can be used as a short helper function to evaluate SELECT COUNT(*)... query.
     *  \param select_statement     The statement containing a 'COUNT (*) AS <variable>' expression
     *  \param count_variable_name  The name of the variable as defined after the AS statement.
     */
    unsigned countOrDie(const std::string &select_statement, const std::string &count_variable_name);

    /** \note Supports online backups of a running database.
     */
    bool sqlite3Backup(const std::string &output_filename, std::string * const err_msg);
    void sqlite3BackupOrDie(const std::string &output_filename);

    /* \param where_clause Do not include the WHERE keyword and only use this if duplicate_key_behaviour is DKB_REPLACE. */
    void insertIntoTableOrDie(const std::string &table_name, const std::map<std::string, std::string> &column_names_to_values_map,
                              const DuplicateKeyBehaviour duplicate_key_behaviour = DKB_FAIL, const std::string &where_clause = "");

    /* \param   where_clause  Do not include the WHERE keyword and only use this if duplicate_key_behaviour is DKB_REPLACE.
     * \param   column_names  The column names.
     * \param   values        The values.  Every entry must have a matching entry for each column name.
     * \warning All entries in "column_names_to_values_maps" must have the same keys, i.e. column names!
     */
    void insertIntoTableOrDie(const std::string &table_name, const std::vector<std::string> &column_names,
                              const std::vector<std::vector<std::optional<std::string>>> &values,
                              const DuplicateKeyBehaviour duplicate_key_behaviour = DKB_FAIL, const std::string &where_clause = "");

    inline virtual DbResultSet getLastResultSet() { return db_connection_->getLastResultSet(); }

    inline virtual std::string getLastErrorMessage() const { return db_connection_->getLastErrorMessage(); }

    /** \return The the number of rows changed, deleted, or inserted by the last statement if it was an UPDATE,
     *          DELETE, or INSERT.
     *  \note   Must be called immediately after calling "query()".
     */
    inline virtual unsigned getNoOfAffectedRows() const { return db_connection_->getNoOfAffectedRows(); }

    /** \note Converts the binary contents of "unescaped_string" into a form that can used as a string.
     *  \note This probably breaks for Sqlite if the string contains binary characters.
     */
    inline virtual std::string escapeString(const std::string &unescaped_string, const bool add_quotes = false,
                                            const bool return_null_on_empty_string = false) {
        return db_connection_->escapeString(unescaped_string, add_quotes, return_null_on_empty_string);
    }

    inline std::string escapeAndQuoteString(const std::string &unescaped_string) {
        return escapeString(unescaped_string, /* add_quotes = */ true);
    }

    inline std::string escapeAndQuoteNonEmptyStringOrReturnNull(const std::string &unescaped_string) {
        return escapeString(unescaped_string, /* add_quotes = */ true, /* return_null_on_empty_string = */ true);
    }

    /** \brief  Join a "list" of words to form a single string,
     *          typically used for SQL statements using the IN keyword.
     *  \note   Adds "," as delimiter.
     *  \param  source     The container of strings that are to be joined.
     */
    template <typename StringContainer>
    std::string joinAndEscapeAndQuoteStrings(const StringContainer &container) {
        std::string subquery;
        for (const auto &string : container) {
            if (not subquery.empty())
                subquery += ",";
            subquery += escapeAndQuoteString(string);
        }
        return subquery;
    }

    // Deletes and recreates the underlying Sqlite3 database.  If "script_path" is non-empty
    // it will be executed after recreation.
    void sqliteResetDatabase(const std::string &script_path = "");

    // Returns a string of the form x'A554E59F' etc.
    std::string sqliteEscapeBlobData(const std::string &blob_data);

    std::string mySQLGetDbName() const;
    std::string mySQLGetUser() const;
    std::string mySQLGetPasswd() const;
    std::string mySQLGetHost() const;
    unsigned mySQLGetPort() const;

    inline void mySQLCreateDatabase(const std::string &database_name, const Charset charset = UTF8MB4,
                                    const Collation collation = UTF8MB4_BIN) {
        queryOrDie("CREATE DATABASE " + database_name + " CHARACTER SET " + CharsetToString(charset) + " COLLATE "
                   + CollationToString(collation) + ";");
    }

    void mySQLCreateUser(const std::string &new_user, const std::string &new_passwd, const std::string &host = "localhost") {
        queryOrDie("CREATE USER " + new_user + "@" + host + " IDENTIFIED BY '" + new_passwd + "';");
    }

    bool mySQLDatabaseExists(const std::string &database_name);

    inline virtual bool tableExists(const std::string &database_name, const std::string &table_name) {
        return db_connection_->tableExists(database_name, table_name);
    }

    bool mySQLDropDatabase(const std::string &database_name);

    std::vector<std::string> mySQLGetDatabaseList();

    std::vector<std::string> mySQLGetTableList();

    void mySQLGrantAllPrivileges(const std::string &database_name, const std::string &database_user,
                                 const std::string &host = "localhost") {
        queryOrDie("GRANT ALL PRIVILEGES ON " + database_name + ".* TO '" + database_user + "'@'" + host + "';");
    }

    void mySQLGrantGrantOption(const std::string &database_name, const std::string &database_user, const std::string &host = "localhost") {
        queryOrDie("GRANT GRANT OPTION ON " + database_name + ".* TO '" + database_user + "'@'" + host + "';");
    }

    std::unordered_set<MYSQL_PRIVILEGE> mySQLGetUserPrivileges(const std::string &user, const std::string &database_name,
                                                               const std::string &host = "localhost");

    bool mySQLUserExists(const std::string &user, const std::string &host);

    void mySQLCreateUserIfNotExists(const std::string &new_user, const std::string &new_passwd, const std::string &host = "localhost") {
        if (not mySQLUserExists(new_user, host)) {
            LOG_INFO("Creating MySQL user '" + new_user + "'@'" + host + "'");
            mySQLCreateUser(new_user, new_passwd, host);
        } else
            LOG_INFO("MySQL user '" + new_user + "'@'" + host + "' already exists");
    }

    inline bool mySQLUserHasPrivileges(const std::string &database_name, const std::unordered_set<MYSQL_PRIVILEGE> &privileges,
                                       const std::string &user, const std::string &host = "localhost") {
        return MiscUtil::AbsoluteComplement(privileges, mySQLGetUserPrivileges(user, database_name, host)).empty();
    }

    inline bool mySQLUserHasPrivileges(const std::string &database_name, const std::unordered_set<MYSQL_PRIVILEGE> &privileges) {
        return mySQLUserHasPrivileges(database_name, privileges, mySQLGetUser(), mySQLGetHost());
    }

    std::string PostgresGetUser() const;
    std::string PostgresGetPasswd() const;
    std::string PostgresGetHost() const;
    unsigned PostgresGetPort() const;

    static std::string TypeToString(const Type type);

protected:
    DbConnection(): db_connection_(nullptr) { }
    DbConnection(DbConnection * const db_connection): db_connection_(db_connection) { }

public:
    /** \brief Splits "query" into individual statements.
     *
     * Splits "query" on semicolons unless we're in a section bounded by "#do_not_split_on_semicolons"
     * and "#end_do_not_split_on_semicolons".  These two directives have to start at the beginning of a line, i.e.
     * either at the start of "query" or immediately after a newline.
     */
    static std::vector<std::string> SplitMySQLStatements(const std::string &query);
    static std::vector<std::string> SplitPostgresStatements(const std::string &query);

    static std::string CharsetToString(const Charset charset);

    static std::string CollationToString(const Collation collation);

    static void MySQLCreateDatabase(const std::string &database_name, const std::string &admin_user, const std::string &admin_passwd,
                                    const std::string &host = "localhost", const unsigned port = MYSQL_PORT,
                                    const Charset charset = UTF8MB4, const Collation collation = UTF8MB4_BIN);

    static void MySQLCreateUser(const std::string &new_user, const std::string &new_passwd, const std::string &admin_user,
                                const std::string &admin_passwd, const std::string &host = "localhost", const unsigned port = MYSQL_PORT,
                                const Charset charset = UTF8MB4);

    static void MySQLCreateUserIfNotExists(const std::string &new_user, const std::string &new_passwd, const std::string &admin_user,
                                           const std::string &admin_passwd, const std::string &host = "localhost",
                                           const unsigned port = MYSQL_PORT, const Charset charset = UTF8MB4);

    static bool MySQLDatabaseExists(const std::string &database_name, const std::string &admin_user, const std::string &admin_passwd,
                                    const std::string &host = "localhost", const unsigned port = MYSQL_PORT,
                                    const Charset charset = UTF8MB4);

    static bool MySQLDropDatabase(const std::string &database_name, const std::string &admin_user, const std::string &admin_passwd,
                                  const std::string &host = "localhost", const unsigned port = MYSQL_PORT, const Charset charset = UTF8MB4);

    static std::vector<std::string> MySQLGetDatabaseList(const std::string &admin_user, const std::string &admin_passwd,
                                                         const std::string &host = "localhost", const unsigned port = MYSQL_PORT,
                                                         const Charset charset = UTF8MB4);

    static void MySQLGrantAllPrivileges(const std::string &database_name, const std::string &database_user, const std::string &admin_user,
                                        const std::string &admin_passwd, const std::string &host = "localhost",
                                        const unsigned port = MYSQL_PORT, const Charset charset = UTF8MB4);

    static bool MySQLUserExists(const std::string &database_user, const std::string &admin_user, const std::string &admin_passwd,
                                const std::string &host = "localhost", const unsigned port = MYSQL_PORT, const Charset charset = UTF8MB4);

    /** \note This function will enable "multiple statement execution support".
     *        To avoid problems with other operations, this function should always create a new connection which is not reusable.
     */
    static void MySQLImportFile(const std::string &sql_file, const std::string &database_name, const std::string &admin_user,
                                const std::string &admin_passwd, const std::string &host = "localhost", const unsigned port = MYSQL_PORT,
                                const Charset charset = UTF8MB4, const TimeZone time_zone = TZ_SYSTEM);

    std::string MySQLPrivilegeToString(const MYSQL_PRIVILEGE mysql_privilege);
};


/** \brief Represents a database transaction.
 *  \note  Restores the autocommit state after going out of scope.
 *  \note  Cannot be nested at this time.
 */
class DbTransaction final {
    static unsigned active_count_;
    DbConnection &db_connection_;
    bool autocommit_was_on_;
    bool rollback_when_exceptions_are_in_flight_;
    bool explicit_commit_or_rollback_has_happened_;

public:
    /** \param rollback_when_exceptions_are_in_flight  If true, the destructor issue a ROLLBACK instead of a commit if
     *         current thread has a live exception object derived from std::exception.
     */
    explicit DbTransaction(DbConnection * const db_connection, const bool rollback_when_exceptions_are_in_flight = true);

    /** \note unless a call to commit() or rollback() has happened the destructur issues a commit. */
    ~DbTransaction();

    void commit();
    void rollback();
};
