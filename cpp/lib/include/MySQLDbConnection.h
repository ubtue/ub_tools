/** \file   MySQLDbConnection.h
 *  \brief  Interface for the MySQLDbConnection class.
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
#pragma once


#include <string>
#include <mysql/mysql.h>
#ifdef MARIADB_PORT
#       define MYSQL_PORT MARIADB_PORT
#endif
#include "DbConnection.h"
#include "DbResultSet.h"


// Forward declarations:
class IniFile;


class MySQLDbConnection final : public DbConnection {
    friend class DbConnection;
    mutable MYSQL mysql_;
    std::string database_name_;
    std::string user_;
    std::string passwd_;
    std::string host_;
    unsigned port_;
    DbConnection::Charset charset_;
    TimeZone time_zone_;
protected:
    explicit MySQLDbConnection(const TimeZone time_zone); // Uses the ub_tools database.
    MySQLDbConnection(const std::string &database_name, const std::string &user, const std::string &passwd,
                      const std::string &host, const unsigned port, const DbConnection::Charset charset,
                      const DbConnection::TimeZone time_zone)
        { init(database_name, user, passwd, host, port, charset, time_zone); }
    MySQLDbConnection(const std::string &mysql_url, const DbConnection::Charset charset,
                      const DbConnection::TimeZone time_zone);
    MySQLDbConnection(const IniFile &ini_file, const std::string &ini_file_section, const TimeZone time_zone);

    /** \note This constructor is for operations which do not require any existing database.
     *        It should only be used in static functions.
     */
    MySQLDbConnection(const std::string &user, const std::string &passwd, const std::string &host, const unsigned port,
                      const Charset charset)
        { init(user, passwd, host, port, charset, TZ_SYSTEM); }
    virtual ~MySQLDbConnection();

    inline virtual DbConnection::Type getType() const override { return DbConnection::T_MYSQL; }
    inline virtual std::string getLastErrorMessage() const override { return ::mysql_error(&mysql_); }
    inline DbConnection::Charset getCharset() const { return charset_; }
    inline DbConnection::TimeZone getTimeZone() const { return time_zone_; }
    void setTimeZone(const TimeZone time_zone);
    inline virtual unsigned getNoOfAffectedRows() const override { return ::mysql_affected_rows(&mysql_); }
    inline virtual int getLastErrorCode() const override { return static_cast<int>(::mysql_errno(&mysql_)); }
    inline virtual bool query(const std::string &query_statement) override;
    virtual bool queryFile(const std::string &filename) override;
    virtual DbResultSet getLastResultSet() override;
    virtual std::string escapeString(const std::string &unescaped_string, const bool add_quotes = false,
                                     const bool return_null_on_empty_string = false) override;
    virtual bool tableExists(const std::string &database_name, const std::string &table_name) override;
private:
    void init(const std::string &database_name, const std::string &user, const std::string &passwd,
              const std::string &host, const unsigned port, const Charset charset, const TimeZone time_zone);

    void init(const std::string &user, const std::string &passwd, const std::string &host, const unsigned port,
              const Charset charset, const TimeZone time_zone);
    inline std::string getDbName() const { return database_name_; }
    inline std::string getUser() const { return user_; }
    inline std::string getPasswd() const { return passwd_; }
    inline std::string getHost() const { return host_; }
    inline unsigned getPort() const { return port_; }
};
