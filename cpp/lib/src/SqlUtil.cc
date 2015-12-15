/** \file    SqlUtil.cc
 *  \brief   Implementations of SQL-related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Artur Kedzierski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
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
#include <stdexcept>
#include <cstdio>


namespace SqlUtil {


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


} // namespace SqlUtil
