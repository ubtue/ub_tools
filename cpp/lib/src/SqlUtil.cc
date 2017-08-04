/** \file    SqlUtil.cc
 *  \brief   Implementations of SQL-related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Artur Kedzierski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  \copyright 2002-2009 Project iVia.
 *  \copyright 2002-2009 The Regents of The University of California.
 *  \copyright 2016,2017 Universitätsbibliothek Tübingen.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "SqlUtil.h"
#include <algorithm>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "util.h"


namespace SqlUtil {

// reference count of open transactions
std::map<DbConnection *, TransactionGuard::Status> TransactionGuard::connection_status_;

    
TransactionGuard::TransactionGuard(DbConnection * const db_connection, const IsolationLevel level)
    : db_connection_(db_connection)
{
    std::string begin("START TRANSACTION");

    if (connection_status_.find(db_connection) == connection_status_.end()) {
        if (level == READ_COMMITTED)
            begin += " WITH CONSISTENT SNAPSHOT";
        db_connection_->queryOrDie(begin);
        Status status(level);
        connection_status_[db_connection] = status;
    } else {
        connection_status_[db_connection].reference_count_++;
        if (connection_status_[db_connection].level_ != level)
            Warning("in TransactionGuard::TransactionGuard: Inconsistent isolation level requested!");
    }
}


void TransactionGuard::rollback() { 
    connection_status_[db_connection_].rolled_back_ = true;
}


TransactionGuard::~TransactionGuard() {
    if (connection_status_[db_connection_].reference_count_ == 1) {
        if (connection_status_[db_connection_].rolled_back_)
            db_connection_->queryOrDie("ROLLBACK");
        else
            db_connection_->queryOrDie("COMMIT");
    } else
        --connection_status_[db_connection_].reference_count_;
}


// EscapeBlob -- Escape charcters in a binary string so it can be used as a MySQL BLOB.
//
std::string &EscapeBlob(std::string * const s) {
    std::string encoded_string;
    encoded_string.reserve(s->size() * 2);

    for (const char ch : *s) {
        if (unlikely(ch == '\'')) {
            encoded_string += '\\';
            encoded_string += '\'';
        }
        else if (unlikely(ch == '"')) {
            encoded_string += '\\';
            encoded_string += '"';
        }
        else if (unlikely(ch == '\\')) {
            encoded_string += '\\';
            encoded_string += '\\';
        }
        else if (unlikely(ch == '\0')) {
            encoded_string += '\\';
            encoded_string += '\0';
        }
        else
            encoded_string += ch;
    }

    std::swap(*s, encoded_string);
    return *s;
}


std::string Unescape(std::string * const s) {
    std::string decoded_string;
    decoded_string.reserve(s->size());
    for (auto ch(s->cbegin()); ch != s->cend(); ++ch) {
        if (likely(*ch != '\\'))
            decoded_string += *ch;
        else {
            ++ch;
            if (unlikely(ch == s->cend()))
                throw std::runtime_error("SqlUtil::Unescape: unexpected end of encoded string!");

            switch (*ch) {
            case '\'':
                decoded_string += '\'';
                break;
            case '"':
                decoded_string += '"';
                break;
            case 'b':
                decoded_string += '\b';
                break;
            case 'n':
                decoded_string += '\n';
                break;
            case 'r':
                decoded_string += '\r';
                break;
            case 't':
                decoded_string += '\t';
                break;
            case '\\':
                decoded_string += '\\';
                break;
            default:
                throw std::runtime_error("SqlUtil::Unescape: improperly encoded string!");
            }
        }
    }

    return (*s = decoded_string);
}


tm DatetimeToTm(const std::string &datetime) {
    if (not IsValidDatetime(datetime))
        throw std::runtime_error("in SqlUtil::DateToTm: invalid \"datetime\" argument \"" + datetime + "\"!");

    // current time format is YYYY-MM-DD hh:mm:ss
    tm time_struct;
    time_struct.tm_year  = std::atol(datetime.substr(0, 4).c_str()) - 1900;
    time_struct.tm_mon   = std::atol(datetime.substr(5, 2).c_str()) - 1;
    time_struct.tm_mday  = std::atol(datetime.substr(8, 2).c_str());
    time_struct.tm_hour  = std::atol(datetime.substr(11, 2).c_str());
    time_struct.tm_min   = std::atol(datetime.substr(14, 2).c_str());
    time_struct.tm_sec   = std::atol(datetime.substr(17, 2).c_str());
    time_struct.tm_isdst = -1; // Don't *ever* change this!!

    return time_struct;
}


time_t DatetimeToTimeT(const std::string &datetime) {
    tm time_struct(DatetimeToTm(datetime));
    return mktime(&time_struct);
}


std::string TmToDatetime(const struct tm &time_struct) {
    char sql_datetime[30 + 1];
    std::sprintf(sql_datetime, "%04u-%02u-%02u %02u:%02u:%02u", time_struct.tm_year + 1900,
                 time_struct.tm_mon + 1, time_struct.tm_mday, time_struct.tm_hour,
                 time_struct.tm_min, time_struct.tm_sec);

    return sql_datetime;
}


std::string TimeTToDatetime(const time_t time) {
    return TmToDatetime(*::gmtime(&time));
}


bool IsValidDatetime(const std::string &datetime) {
    if (datetime.length() == 10) {
        // Presumably "YYYY-MM-DD".

        unsigned year, month, day;
        if (std::sscanf(datetime.c_str(), "%4u-%2u-%2u", &year, &month, &day) != 3)
            return false;

        return (year >= 1000 and year <= 9999) and (month >= 1 and month <= 12) and
            (day >= 1 and day <= 365);
    } else if (datetime.length() == 19) {
        // Presumably "YYYY-MM-DD hh:mm:ss".

        unsigned year, month, day, hour, minute, second;
        if (std::sscanf(datetime.c_str(), "%4u-%2u-%2u %2u:%2u:%2u", &year, &month, &day, &hour,
                        &minute, &second) != 6)
            return false;

        return (year >= 1000 and year <= 9999) and (month >= 1 and month <= 12) and
            (day >= 1 and day <= 365) and hour <= 23 and minute <= 59 and second <= 61;
    } else
        return false;
}


std::string GetDatetime(const long offset) {
    time_t now(std::time(NULL) + offset);
    char buf[20 + 1];
    std::strftime(buf, sizeof(buf), "%F %T", std::localtime(&now));

    return buf;
}


std::set<std::string> GetColumnNames(DbConnection * const connection, const std::string &table_name) {
    const std::string SHOW_COUMNS_STMT("SHOW COLUMNS FROM `" + table_name + "`;");
    if (not connection->query(SHOW_COUMNS_STMT))
        throw std::runtime_error("in SqlUtil::GetColumnNames: Show columns failed: " + SHOW_COUMNS_STMT
                                 + " (" + connection->getLastErrorMessage() + ")");
    DbResultSet result_set(connection->getLastResultSet());
    std::set<std::string> column_names;
    while (const DbRow row = result_set.getNextRow())
        column_names.insert(row["Field"]);

    return column_names;
}


} // namespace SqlUtil
