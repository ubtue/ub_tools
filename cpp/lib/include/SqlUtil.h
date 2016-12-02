/** \file    SqlUtil.h
 *  \brief   Declarations of SQL-related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Artur Kedzierski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
 *  Copyright 2016 Universitätsbibliothek Tübingen.
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

#ifndef SQL_UTIL_H
#define SQL_UTIL_H


#include <set>
#include <string>
#include <ctime>


// Forward declaration:
class DbConnection;


namespace SqlUtil {


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


/** Converts an SQL datetime to a struct tm type. */
tm DatetimeToTm(const std::string &datetime);


/** Converts an SQL datetime to a time_t type (number of seconds since epoch. */
time_t DatetimeToTimeT(const std::string &datetime);


/** Changes a time_t type (number of seconds since epoch) to an SQL datetime. */
std::string TimeTToDatetime(const time_t time);


/** Changes a struct tm (broken down time) to an SQL datetime. */
std::string TmToDatetime(const struct tm &time_struct);


/** Checks if "datetime" is in format that an SQL database can use ("YYYY-MM-DD" or "YYYY-MM-DD hh:mm:ss"). */
bool IsValidDatetime(const std::string &datetime);


/** \return A set that contains the names of the columns in "table_name". */
std::set<std::string> GetColumnNames(DbConnection * const connection, const std::string &table_name);



} // namespace SqlUtil


#endif // ifndef SQL_UTIL_H
