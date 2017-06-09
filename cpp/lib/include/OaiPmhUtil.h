/** \file    OaiPmhUtil.h
 *  \brief   Declaration of various utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2005 Project iVia.
 *  Copyright 2002-2005 The Regents of The University of California.
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

#ifndef OAI_PMH_UTIL_H
#define OAI_PMH_UTIL_H


#ifndef STRING
#       include <string>
#       define STRING
#endif
#ifndef TIME_UTIL_H
#       include <TimeUtil.h>
#endif


namespace OaiPmh {
namespace Util {


/** \brief   Check whether an OAI-PMH identifier is in the standard format.
 *  \param  identifier  The identifier to test.
 *  \return  True if the identifier is conformant, otherwise false.
 *
 *  This function checks for conformance with
 *  http://www.openarchives.org/OAI/2.0/oai-identifier.xsd.
 */
bool IsValidOaiIdentifier(const std::string &identifier);


/** \brief Verify that the given string is a valid UTC timestamp.
 *  \param  utc_date_time  A tiemstamp that is supposedly in UTC format.
 *  \return  True if the datastamp is in UTC format.
 */
bool IsValidUtcDateTime(const std::string &utc_date_time);


/** \brief   Get the current date and time in Zulu format. */
inline std::string GetCurrentDateAndTime()
{
	return TimeUtil::GetCurrentDateAndTime(TimeUtil::ZULU_FORMAT, TimeUtil::UTC); 
}


/** \brief   Convert a local SQL DateTime to a UTC string. */
inline std::string LocalSqlDateTimeToUtcDateTime(const std::string &local_sql_date_time)
{ 
	return TimeUtil::LocalTimeToZuluTime(local_sql_date_time); 
}


/** \brief   Attempt to convert Zulu time to a local SQL datetime.
 *  \return  Returns true on success and false on failure.
 *  \note    Will also accept other UTC time formats.
 */
bool UtcDateTimeToLocalSqlDateTime(const std::string &utc_date_time, std::string * const sql_date_time);


} // namespace Util
} // namespace OaiPmh


#endif // ifndef OAI_PMH_UTIL_H
