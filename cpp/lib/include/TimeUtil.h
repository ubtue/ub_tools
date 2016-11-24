/** \file    TimeUtil.h
 *  \brief   Declarations of time-related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 *  \author  Wagner Truppel
 */

/*
 *  Copyright 2003-2008 Project iVia.
 *  Copyright 2003-2008 The Regents of The University of California.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef TIME_UTIL_H
#define TIME_UTIL_H


#include <string>
#include <cstdint>
#include <ctime>
#include "PerlCompatRegExp.h"


/** \namespace  TimeUtil
 *  \brief      Utility funstions for manipulating dates and times.
 */
namespace TimeUtil {


const time_t BAD_TIME_T = static_cast<time_t>(-1);


const std::string ISO_8601_FORMAT("%Y-%m-%d %T");


/** The default strftime(3) format string for representing dates and times. */
const std::string DEFAULT_FORMAT("%Y-%m-%d %T");


/** The strftime(3) format string for representing the UTC "Zulu" representation. */
const std::string ZULU_FORMAT("%Y-%m-%dT%TZ");


/** \enum   TimeZone
 *  \brief  Differentiate between UTC and the local timezone.
 */
enum TimeZone { UTC, LOCAL };


inline void MillisecondsToTimeSpec(const unsigned milliseconds, timespec * const time_spec) {
    time_spec->tv_sec  = milliseconds / 1000u;
    time_spec->tv_nsec = (milliseconds % 1000u) * 1000000u;
}


/** \brief  Returns a string representation of a time interval in milliseconds, in the form exemplified by
 *          3d 17h 29m 7s 376.2ms. Zero values are ommitted: example: 2h 8s. The separator between fields
 *          defaults to " " but can be set to any string if you prefer formats like 4d-5h-10s or 4d5h10s.
 */
std::string FormatTime(const double time_in_millisecs, const std::string &separator = " ");


/** \brief   Get the current date time as a string
 *  \return  A string representing the current date and time.
 */
std::string GetCurrentDateAndTime(const std::string &format = DEFAULT_FORMAT, const TimeZone time_zone = LOCAL);


/** \brief   Get the current time as a string.
 *  \return  A string representing the current time.
 */
inline std::string GetCurrentTime(const TimeZone time_zone = LOCAL) { return GetCurrentDateAndTime("%T", time_zone); }


/** \brief  Convert a time from a time_t to a string.
 *  \param  the_time   The time to convert.
 *  \param  format     The format of the result, in strftime(3) format.
 *  \param  time_zone  Whether to use local time (the default) or UTC.
 *  \return The converted time.
 */
std::string TimeTToString(const time_t &the_time, const std::string &format = DEFAULT_FORMAT,
                          const TimeZone time_zone = LOCAL);


/** \brief  Inverse of gmtime(3).
 *  \param  tm  Broken-down time, expressed in Coordinated Universal Time (UTC).
 *  \return The calendar time or TimeUtil::BAD_TIME_T on error.
 */
time_t TimeGm(const struct tm &tm);


/** \brief   Convert a time from a time_t to a string, using local time.
 *  \param   the_time  The time to convert.
 *  \param   format    The format of the result, in strftime(3) format.
 *  \return  The converted time.
 */
inline std::string TimeTToLocalTimeString(const time_t &the_time, const std::string &format = DEFAULT_FORMAT)
    { return TimeTToString(the_time, format, LOCAL); }


/** \brief   Convert a time from a time_t to a string, using UTC.
 *  \param   the_time  The time to convert.
 *  \param   format    The format of the result, in strftime(3) format.
 *  \return  The converted time.
 */
inline std::string TimeTToUtcString(const time_t &the_time, const std::string &format = DEFAULT_FORMAT)
    { return TimeTToString(the_time, format, UTC); }


/** \brief   Convert a time from a time_t to a string using Zulu representation.
 *  \param   the_time  The time to convert.
 *  \return  The converted time.
 */
inline std::string TimeTToZuluString(const time_t &the_time)
    { return TimeTToUtcString(the_time, ZULU_FORMAT); }


/** \brief  Parses a date/time string into its individual components.
 *  \note   We support three formats: "YYYY-MM-DD hh:mm:ss", "YYYY-MM-DDThh:mm:ssZ", and "YYYY-MM-DD".
 *  \param  possible_date            The string that we suspect to be a date in one of the 3 formats that we support.
 *  \param  year                     The year component of the date.
 *  \param  month                    The month component of the date.
 *  \param  day                      The day component of the date.
 *  \param  hour                     The hour component of the date.
 *  \param  minute                   The minute component of the date.
 *  \param  second                   The second component of the date.
 *  \param  is_definitely_zulu_time  Will be set to true if the date/time format was in Zulu time for sure.
 *  \return The number of components that we successfully identified, 3 or 6, or 0 if we didn't have an exact match with
 *          one of our three supported formats.  When we return three, hour, minute, and second will be set to zero.
 */
unsigned StringToBrokenDownTime(const std::string &possible_date, unsigned * const year, unsigned * const month,
                                unsigned * const day, unsigned * const hour, unsigned * const minute,
                                unsigned * const second, bool * const is_definitely_zulu_time);


/** \brief   Convert a time from (a subset of) ISO 8601 format to a time_t.
 *  \param   iso_time   The time to convert in ISO 8601 format.
 *  \param   time_zone  Whether to use local time (the default) or UTC.
 *  \return  The converted time.
 *
 *  The time is expected to be in the following formats:
 *  "YYYY-MM-DD hh:mm:ss" or "YYYY-MM-DDThh:mm:ssZ" (Zulu time) or "YYYY-MM-DD".
 *
 *  \note  If "iso_time" is in Zulu format, then the time_zone
 *         parameter must be set to UTC.
 */
time_t Iso8601StringToTimeT(const std::string &iso_time, const TimeZone time_zone = LOCAL);


/** \brief   Convert a local time into a UTC time in Zulu represenation.
 *  \param   local_time  The local time in a format understood by Iso8601StringToTimeT.
 *  \return  The UTC time in Zulu representation.
 */
inline std::string LocalTimeToZuluTime(const std::string &local_time)
    { return TimeTToZuluString(Iso8601StringToTimeT(local_time, LOCAL)); }


/** \brief   Convert a UTC time (including Zulu representation) into a local time.
 *  \param   utc  The UTC time in a format understood by Iso8601StringToTimeT.
 *  \param   format The format of the UTC time passed into the function.
 *  \return  The local time in ISO8601 representation.
 */
inline std::string UtcToLocalTime(const std::string &utc, const std::string &format = DEFAULT_FORMAT)
    { return TimeTToLocalTimeString(Iso8601StringToTimeT(utc, UTC), format); }


/** \brief  Calculates the Julian Day Number from a Gregorian date.
 *  \param  year   A four-digit year.
 *  \param  month  The number of a month [1-12].
 *  \param  day    The day of a month [1-31].
 *  \return The Julian Day Number for the beginning of the date in question at 0 hours, Greenwich time.
 *  \note   This always gives you a half day extra. That is because the Julian Day begins at noon, Greenwich time.
 *          This is convenient for astronomers (who until recently only observed at night), but it is confusing.
 */
double GetJulianDayNumber(const unsigned year, const unsigned month, const unsigned day);


/** \brief  Calculates the Gregorian year, month, and day corresponding to a Julian Day Number.
 *  \param  julian_day_number  The Julian Day Number to convert from.
 *  \param  year               Output: A four-digit year.
 *  \param  month              Output: The number of a month [1-12].
 *  \param  day                Output: The day of a month [1-31].
 */
void JulianDayNumberToYearMonthAndDay(const double julian_day_number, unsigned * const year, unsigned * const month,
                                      unsigned * const day);


/** \brief  Add an integral number of days to a "time_t".
 *  \param  start_time  The initial time.
 *  \param  days        The number of days to add to the start time (can be negative).
 *  \return The time after the addition or TimeUtil::BAD_TIME_T if an error occurred.
 */
time_t AddDays(const time_t &start_time, const int days);


/** \brief  Try to figure out the time_t value if passed a "Human" style date and time representation
 *  \param  human_date  A date as humans write it, such as "Fri Jun 30 3:30PM."
 *  \return Returns a time_t representation of the human style date passed in or BAD_TIME_T if the format was not recognized.
 */
time_t ConvertHumanDateTimeToTimeT(const std::string &human_date);


/** Returns elapsed time since the Unix epoch rounded to the nearest millisecond. */
uint64_t GetCurrentTimeInMilliseconds();


/** Returns elapsed time since the Unix epoch rounded to the nearest microsecond. */
uint64_t GetCurrentTimeInMicroseconds();


/** Attempts to sleep at least "sleep_interval" milliseconds.  The function can return earlier if a signal has been
    delivered to the process.  In that case errno would be set to EINTR. */
void Millisleep(const unsigned sleep_interval);


std::string GetCurrentYear(const TimeZone time_zone = LOCAL);


} // namespace TimeUtil


#endif // ifndef TIME_UTIL_H
