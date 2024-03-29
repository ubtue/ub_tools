/** \file    SqlUtil.h
 *  \brief   Declarations of SQL-related utility functions.
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
#pragma once


#include <map>
#include <set>
#include <string>
#include <ctime>


// Forward declaration:
class DbConnection;


namespace SqlUtil {


// As supported by MySQL
extern const std::string DATETIME_RANGE_MIN;
extern const std::string DATETIME_RANGE_MAX;
// As supported by MariaDB on CentOS
constexpr size_t VARCHAR_UTF8_MAX_INDEX_LENGTH(768);


/** \class TransactionGuard
 *  \brief Creates a BEGIN/COMMIT guard between the time the first instance of this class is created and the same
 *         instance is destroyed.
 *
 *         This is based on the DbConnection used and is reference counted.
 */
class TransactionGuard {
public:
    enum IsolationLevel { READ_COMMITTED, SERIALIZABLE };

private:
    struct Status {
        IsolationLevel level_;
        bool rolled_back_;
        unsigned reference_count_;
        Status(const IsolationLevel &level = READ_COMMITTED): level_(level), rolled_back_(false), reference_count_(1) { }
    };
    static std::map<DbConnection *, Status> connection_status_;

    DbConnection * const db_connection_;

public:
    explicit TransactionGuard(DbConnection * const db_connection, const IsolationLevel level = READ_COMMITTED);
    void rollback();
    ~TransactionGuard();
};


/* Returns the string truncated to the maximum number of UTF-8 characters supported by our codebase */
std::string TruncateToVarCharMaxIndexLength(const std::string &s);


/** \brief   Escape special characters in a MySQL BLOB
 *  \param   s  The BLOB to escape
 *  \return  A pointer to the new, escaped s.
 *
 *  EscapeBlob escapes binary strings that represent MySQL BLOBs.
 *  BLOBs require less escaping than strings: only null characters,
 *  backslashes, quotes and double-quotes need be escaped.
 *
 * \note Use the standard Unescape to unescape a BLOB.
 */
std::string &EscapeBlob(std::string * const s);


/** \brief  Unescape characters in a MySQL string or BLOB.
 *  \param  s  The string to unescape
 *  \return The new, unescaped s.
 */
std::string Unescape(std::string * const s);


/** Converts an SQL datetime or timestamp to a struct tm type. */
tm DatetimeToTm(const std::string &datetime);


/** Converts an SQL datetime or timestamp to a time_t type (number of seconds since epoch. */
time_t DatetimeToTimeT(const std::string &datetime);


/** Changes a time_t type (number of seconds since epoch) to an SQL datetime. */
std::string TimeTToDatetime(const time_t time);


/** Changes a struct tm (broken down time) to an SQL datetime. */
std::string TmToDatetime(const struct tm &time_struct);


/** Checks if "datetime" is in format that an SQL database can use ("YYYY-MM-DD" or "YYYY-MM-DD hh:mm:ss"). */
bool IsValidDatetime(const std::string &datetime);


/** \brief   Get the current date and time in SQL datetime format.
 *  \param   offset  An offset in seconds to be added the current date/time.
 *  \return  Returns the current date/time in the "YYYY-MM-DD hh:mm:ss" format.
 */
std::string GetDatetime(const long offset = 0);


/** \return A set that contains the names of the columns in "table_name". */
std::set<std::string> GetColumnNames(DbConnection * const connection, const std::string &table_name);


/** \return The number of rows in "table_name". */
unsigned GetTableSize(DbConnection * const connection, const std::string &table_name);


// Ensures that thread-specific variables are initialized for the correct functioning
// of the MySQL connector. Must be initialized at the very beginning of the invoking thread.
struct ThreadSafetyGuard {
    enum ThreadType { MAIN_THREAD, WORKER_THREAD };

private:
    ThreadType invoker_thread_;

public:
    explicit ThreadSafetyGuard(const ThreadType invoker_thread);
    ~ThreadSafetyGuard();
};


} // namespace SqlUtil
