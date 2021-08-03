/** \file    TimeUtil.h
 *  \brief   Declarations of time-related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 *  \author  Wagner Truppel
 */

/*
 *  Copyright 2003-2008 Project iVia.
 *  Copyright 2003-2008 The Regents of The University of California.
 *  Copyright 2018-2021 Universitätsbibliothek Tübingen
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
#pragma once


#include <limits>
#include <string>
#include <cstdint>
#ifndef _BSD_SOURCE
#   define _BSD_SOURCE
#endif
#include <ctime>


//
// Support direct comparison of timespec values:
//
inline bool operator<(const timespec &lhs, const timespec &rhs) {
    if (lhs.tv_sec == rhs.tv_sec)
        return lhs.tv_nsec < rhs.tv_nsec;
    else
        return lhs.tv_sec < rhs.tv_sec;
}


inline bool operator>(const timespec &lhs, const timespec &rhs) {
    if (lhs.tv_sec == rhs.tv_sec)
        return lhs.tv_nsec > rhs.tv_nsec;
    else
        return lhs.tv_sec > rhs.tv_sec;
}


/** \namespace  TimeUtil
 *  \brief      Utility funstions for manipulating dates and times.
 */
namespace TimeUtil {


constexpr time_t BAD_TIME_T = std::numeric_limits<time_t>::min();
constexpr time_t MAX_TIME_T = std::numeric_limits<time_t>::max();


const std::string ISO_8601_FORMAT("%Y-%m-%dT%T"); // This is only one of several possible ISO 8601 date/time formats!


/** The default strftime(3) format string for representing dates and times. */
const std::string DEFAULT_FORMAT("%Y-%m-%d %T");


/** The strftime(3) format string for representing the UTC "Zulu" representation. */
const std::string ZULU_FORMAT("%Y-%m-%dT%TZ");


/** The strftime(3) format string for representing the UTC "Zulu" representation. */
const std::string RFC822_FORMAT("%a, %d %b %Y %H:%M:%S %z");


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
std::string GetCurrentDateAndTime(const std::string &format = DEFAULT_FORMAT, const TimeZone time_zone = LOCAL,
                                  const std::string &time_locale = UB_DEFAULT_LOCALE);


/** \brief   Get the current time as a string.
 *  \return  A string representing the current time.
 */
inline std::string GetCurrentTime(const TimeZone time_zone = LOCAL) { return GetCurrentDateAndTime("%T", time_zone); }


/** \brief   Get the current year as a string.
 *  \return  A string representing the current year.
 */
inline std::string GetCurrentYear(const TimeZone time_zone = LOCAL) { return GetCurrentDateAndTime("%Y", time_zone); }


void GetCurrentDate(unsigned * const year, unsigned * const month, unsigned * const day, const TimeZone time_zone = LOCAL);


/** \brief  Convert a time from a time_t to a string.
 *  \param  the_time   The time to convert.
 *  \param  format     The format of the result, in strftime(3) format.
 *  \param  time_zone  Whether to use local time (the default) or UTC.
 *  \return The converted time.
 */
std::string TimeTToString(const time_t &the_time, const std::string &format = DEFAULT_FORMAT, const TimeZone time_zone = LOCAL,
                          const std::string &time_locale = UB_DEFAULT_LOCALE);


/** \brief Converts a UNIX timestamp (in seconds) to a time_t. */
bool StringToTimeT(const std::string &time_str, time_t * const unix_time);


/** \brief Converts a UNIX timestamp (in seconds) to a time_t.
 *  \note This function will abort if "time_str" cannot be converted to a valid time_t!
 */
time_t StringToTimeT(const std::string &time_str);


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


/** \brief Converts "*datetime" from format "from_format" to format "to_format"
 *  \param  from_format  a strptime format
 *  \param  to_format    an strftime format
 *  \param  time_zone  Whether to use local time (the default) or UTC.
 *  \note The strptime format may be prefixed by a comma-separated list of locale names sourrounded by paretheses.  If more than one
 *        local has been provided, conversions will be attempted until one succeeds or the list has been exhausted.
 *  \return True if the input datetime matched "from_format", else false.
 */
bool ConvertFormat(const std::string &from_format, const std::string &to_format, std::string * const datetime,
                   const TimeZone time_zone = LOCAL);


/** \brief  Parses an ISO 8601 date/time string into its individual components (see https://en.wikipedia.org/wiki/ISO_8601).
 *  \note   We support three formats: "YYYY-MM-DD hh:mm:ss", "YYYY-MM-DDThh:mm:ssZ", and "YYYY-MM-DD".
 *  \param  possible_date            The string that we suspect to be a date in one of the 3 formats that we support.
 *  \param  year                     The year component of the date.
 *  \param  month                    The month component of the date.
 *  \param  day                      The day component of the date.
 *  \param  hour                     The hour component of the date.
 *  \param  minute                   The minute component of the date.
 *  \param  second                   The second component of the date.
 *  \param  hour_offset              Timezone hour offset
 *  \param  minute_offset            Timezone minute offset
 *  \param  is_definitely_zulu_time  Will be set to true if the date/time format was in Zulu time for sure.
 *  \return The number of components that we successfully identified, 3 or 6, or 0 if we didn't have an exact match with
 *          one of our three supported formats.  When we return three, hour, minute, and second will be set to zero.
 */
unsigned StringToBrokenDownTime(const std::string &possible_date, unsigned * const year, unsigned * const month,
                                unsigned * const day, unsigned * const hour, unsigned * const minute,
                                unsigned * const second, int * const hour_offset, int * const minute_offset,
                                bool * const is_definitely_zulu_time);


/** \brief  Parses a date/time string and gets the year out of it.
 *  \note   Possible string formats see StringToBrokenDownTime
 *  \param  Date/Time string in supported format.
 *  \year   The year component of the date.
 *  \return True if year could be determined, else false.
 */
bool StringToYear(const std::string &possible_date, unsigned * const year);



/** \brief   Convert a time from (a subset of) ISO 8601 format to a time_t.
 *  \param   iso_time        The time to convert in ISO 8601 format.
 *  \param   converted_time  Upon success, we stored the converted time here.
 *  \param   time_zone       Whether to use local time (the default) or UTC.
 *  \return  True if we successfully parsed the input, else false.
 *
 *  The time is expected to be in the following formats:
 *  "YYYY-MM-DD hh:mm:ss" or "YYYY-MM-DDThh:mm:ssZ" (Zulu time) or "YYYY-MM-DD".
 *
 *  \note  If "iso_time" is in Zulu format, then the time_zone
 *         parameter must be set to UTC.
 */
bool Iso8601StringToTimeT(const std::string &iso_time, time_t * const converted_time, std::string * const err_msg,
                          const TimeZone time_zone = LOCAL);


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


/** \return The Julian Day Number for the current moment.
 *  \note   This will typically not be a fractional value.
 */
double GetJulianDayNumber();


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


/** \brief Parses a date/time in RFC1123 date and time format.
 *  \note The returned time is UTC time.
 *  \note If an error occurred we return false and set "*date_time" to BAD_TIME_T.
 */
bool ParseRFC1123DateTime(const std::string &date_time_candidate, time_t * const date_time);


/** \brief Parses a date/time in RFC3339/ISO8601 date and time format.
 *  \note The returned time is UTC time.
 *  \note If an error occurred we return false and set "*date_time" to BAD_TIME_T.
 *  \note On success we set *date_time to the nearest second of the date/time.
 */
bool ParseRFC3339DateTime(const std::string &date_time_candidate, time_t * const date_time);


// Debugging aid.
std::string StructTmToString(const struct tm &tm);


/** \brief Attempts to convert "date_str" to a struct tm instance.
 *  \param optional_strptime_format  If empty, a few heuristics will be tried, o/w, you must specify an optional list of
 *         pipe-separated locales (please only pick ones that are supported by all operating systems that we use) in parentheses
 *         followed by a format as documented on the strptime(3) man page.
 *  \note The strptime format may be prefixed by a comma-separated list of locale names sourrounded by paretheses.  If more than one
 *        local has been provided, conversions will be attempted until one succeeds of the list has been exhausted.
 */
struct tm StringToStructTm(std::string date_str, std::string optional_strptime_format = DEFAULT_FORMAT);
bool StringToStructTm(struct tm * const tm, std::string date_str, std::string optional_strptime_format = DEFAULT_FORMAT);


/* Returns the difference in seconds between beginning and end. */
double DiffStructTm(struct tm end, struct tm beginning);


/* Returns the current time in the GMT/UTC timezone. */
struct tm GetCurrentTimeGMT();


/* Returns 0 if the date is in the range, 1 if date is greater than "last" or -1 if date is less than "first". */
int IsDateInRange(time_t first, time_t last, time_t date);


/** \brief  Converts output of the asctime(3) function to a struct tm
 *  \return True if the conversion succeeded o/w false.
 */
bool AscTimeToStructTm(std::string asctime_output, struct tm * const tm);


bool IsLeapYear(const unsigned year);
bool IsLeapYear(const std::string &year);


unsigned GetDaysInMonth(const unsigned year, const unsigned month);


} // namespace TimeUtil
