/** \file   PostgresDbConnection.h
 *  \brief  Interface for the PostgresDbConnection class.
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
#include <libpq-fe.h>
#include "DbConnection.h"
#include "DbResultSet.h"


class PostgresDbConnection final : public DbConnection {
    friend class DbConnection;
    PGconn *pg_conn_;
    PGresult *pg_result_;
    std::string user_;
    std::string passwd_;
    std::string host_;
    unsigned port_;
protected:
    PostgresDbConnection(PGconn * const pg_conn, const std::string &user, const std::string &passwd, const std::string &host,
                         const unsigned port)
        : DbConnection(this), pg_conn_(pg_conn), pg_result_(nullptr), user_(user), passwd_(passwd), host_(host), port_(port) { }
    inline virtual ~PostgresDbConnection();

    inline virtual DbConnection::Type getType() const override { return DbConnection::T_POSTGRES; }
    virtual inline std::string getLastErrorMessage() const override { return ::PQerrorMessage(pg_conn_); }
    virtual unsigned getNoOfAffectedRows() const override;
    virtual int getLastErrorCode() const override;
    inline virtual bool query(const std::string &query_statement) override;
    virtual bool queryFile(const std::string &filename) override;
    virtual DbResultSet getLastResultSet() override;
    virtual std::string escapeString(const std::string &unescaped_string, const bool add_quotes = false,
                                     const bool return_null_on_empty_string = false) override;
    virtual bool tableExists(const std::string &database_name, const std::string &table_name) override;
private:
    inline std::string getUser() const { return user_; }
    inline std::string getPasswd() const { return passwd_; }
    inline std::string getHost() const { return host_; }
    inline unsigned getPort() const { return port_; }
};
