/** \file    OaiPmhServerUtil.cc
 *  \brief   Implementation of various utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
 *
 *  This file is part of the libiViaOaiPmh package.
 *
 *  The libiViaOaiPmh package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaOaiPmh is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaOaiPmh; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "OaiPmhUtil.h"
#include "PerlCompatRegExp.h"


namespace OaiPmh {
namespace Util {


// IsValidOaiIdentifier --  is this a valid OAI-PMH identifier?
//
bool IsValidOaiIdentifier(const std::string &identifier) {
    // Note: the following pattern was taken "oai-identifier.xsd" as found
    // at http://www.openarchives.org/OAI/2.0/oai-identifier.xsd.
    const std::string PATTERN = "^oai:[a-zA-Z][a-zA-Z0-9\\-]*(\\.[a-zA-Z][a-zA-Z0-9\\-]+)+:"
        "[a-zA-Z0-9\\-_\\.!~\\*'\\(\\);/\\?:@&=\\+\\$,%]+$";

    PerlCompatRegExp perl_compat_reg_exp(PATTERN);
    size_t start_pos, length;
    return perl_compat_reg_exp.match(identifier, 0 /* start offset */, &start_pos, &length);
}


// IsValidUtcDateTime -- Verify that the given string is a valid UTC timestamp.
//
bool IsValidUtcDateTime(const std::string &utc_date_time) {
    bool success = true;

    try {
        TimeUtil::UtcToLocalTime(utc_date_time);
    } catch (const std::exception &x) {
        success = false;
    }

    return success;
}


// UtcDateTimeToLocalSqlDateTime -- Attempts to convert Zulu time to a local SQL datetime.
//                                  Will also accept other UTC time formats.
//                                  Returns true on success and false on failure.
//
bool UtcDateTimeToLocalSqlDateTime(const std::string &utc_date_time,
				   std::string * const local_sql_date_time)
{
    bool success = true;
    local_sql_date_time->clear();

    try {
        *local_sql_date_time = TimeUtil::UtcToLocalTime(utc_date_time);
    }
    catch (const std::exception &x) {
        success = false;
        local_sql_date_time->clear();
    }

    return success;
}


} // namespace Util
} // namespace OaiPmh
