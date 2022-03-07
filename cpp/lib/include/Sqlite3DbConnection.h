/** \file   Sqlite3DbConnection.h
 *  \brief  Interface for the Sqlite3DbConnection class.
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
#include <sqlite3.h>
#include "DbConnection.h"
#include "DbResultSet.h"


class Sqlite3DbConnection final : public DbConnection {
    friend class DbConnection;
    sqlite3 *sqlite3_;
    sqlite3_stmt *stmt_handle_;
    std::string database_path_;

protected:
    Sqlite3DbConnection(const std::string &database_path, const OpenMode open_mode);
    inline virtual ~Sqlite3DbConnection();

    inline virtual DbConnection::Type getType() const override { return DbConnection::T_SQLITE; }
    inline virtual unsigned getNoOfAffectedRows() const override { return ::sqlite3_changes(sqlite3_); }
    inline virtual std::string getLastErrorMessage() const override { return ::sqlite3_errmsg(sqlite3_); }
    inline virtual int getLastErrorCode() const override { return ::sqlite3_errcode(sqlite3_); }
    inline virtual bool query(const std::string &query_statement) override;
    virtual bool queryFile(const std::string &filename) override;
    virtual DbResultSet getLastResultSet() override;
    virtual std::string escapeString(const std::string &unescaped_string, const bool add_quotes = false,
                                     const bool return_null_on_empty_string = false) override;
    virtual bool tableExists(const std::string &database_name, const std::string &table_name) override;
    inline const std::string &getDatabasePath() const { return database_path_; }
    bool backup(const std::string &output_filename, std::string * const err_msg);
};
