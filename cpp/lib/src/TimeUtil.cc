/** \file    TimeUtil.cc
 *  \brief   Declarations of time-related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 *  \author  Wagner Truppel
 */

/*
 *  Copyright 2003-2009 Project iVia.
 *  Copyright 2003-2009 The Regents of The University of California.
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

#include "TimeUtil.h"
#include <map>
#include <stdexcept>
#include <string>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/time.h>
#include "Compiler.h"


namespace TimeUtil {


std::string FormatTime(const double time_in_millisecs, const std::string &separator) {
    if (time_in_millisecs < 0.0)
        throw std::runtime_error("in TimeUtil::FormatTime: 'time_in_millisecs' must be non-negative!");

    if (time_in_millisecs == 0.0)
        return "0ms";

    unsigned no_of_non_zero_values(0);

    unsigned secs = static_cast<unsigned>(time_in_millisecs / 1000.0);
    const double millis = time_in_millisecs - 1000.0 * secs;
    if (millis > 0)
        ++no_of_non_zero_values;

    unsigned mins = secs / 60;
    secs %= 60;
    if (secs > 0)
        ++no_of_non_zero_values;

    unsigned hours = mins / 60;
    mins %= 60;
    if (mins > 0)
        ++no_of_non_zero_values;

    unsigned days = hours / 24;
    hours %= 24;
    if (hours > 0)
        ++no_of_non_zero_values;

    if (days > 0)
        ++no_of_non_zero_values;

    bool show_millis_when_zero(true);
    std::string output;

    if (days != 0) {
        show_millis_when_zero = false;
        output += std::to_string(days) + 'd';
        if (no_of_non_zero_values > 1) {
            output += separator;
            --no_of_non_zero_values;
        }
    }

    if (hours != 0) {
        show_millis_when_zero = false;
        output += std::to_string(hours) + 'h';
        if (no_of_non_zero_values > 1) {
            output += separator;
            --no_of_non_zero_values;
        }
    }

    if (mins != 0) {
        show_millis_when_zero = false;
        output += std::to_string(mins) + 'm';
        if (no_of_non_zero_values > 1) {
            output += separator;
            --no_of_non_zero_values;
        }
    }

    if (secs != 0) {
        show_millis_when_zero = false;
        output += std::to_string(secs) + 's';
        if (no_of_non_zero_values > 1) {
            output += separator;
            --no_of_non_zero_values;
        }
    }

    if (show_millis_when_zero or millis != 0.0) {
        if (millis == static_cast<unsigned>(millis))
            output += std::to_string(static_cast<unsigned>(millis)) + "ms";
        else
            output += std::to_string(millis) + "ms";
    }

    return output;
}


// GetCurrentTime -- Get the current date and time as a string
//
std::string GetCurrentDateAndTime(const std::string &format, const TimeZone time_zone) {
    time_t now;
    std::time(&now);
    return TimeTToString(now, format, time_zone);
}


// TimeTToLocalTimeString -- Convert a time from a time_t to a string.
//
std::string TimeTToString(const time_t &the_time, const std::string &format, const TimeZone time_zone) {
    char time_buf[50 + 1];
    std::strftime(time_buf, sizeof(time_buf), format.c_str(),
                  (time_zone == LOCAL ? std::localtime(&the_time) : std::gmtime(&the_time)));
    return time_buf;
}


time_t TimeGm(const struct tm &tm) {
    const char * const saved_time_zone = ::getenv("TZ");

    // Set the time zone to UTC:
    ::setenv("TZ", "", 1);
    ::tzset();

    struct tm temp_tm(tm);
    const time_t ret_val = ::mktime(&temp_tm);

    // Restore the original time zone:
    if (saved_time_zone != nullptr)
        ::setenv("TZ", saved_time_zone, 1);
    else
        ::unsetenv("TZ");
    ::tzset();

    return ret_val;
}


struct tm StringToStructTm(const std::string &date_and_time, const std::string &format) {
    struct tm tm;
    std::memset(&tm, 0, sizeof tm);
    const char * const retval(::strptime(date_and_time.c_str(), format.c_str(), &tm));
    if (unlikely(retval == nullptr or *retval != '\0'))
        throw std::runtime_error("in TimeUtil::StringToStructTm: failed to convert \"" + date_and_time
                                 + "\" to a struct tm using the format \"" + format + "\"!");
    return tm;
}

    
unsigned StringToBrokenDownTime(const std::string &possible_date, unsigned * const year, unsigned * const month,
                                unsigned * const day, unsigned * const hour, unsigned * const minute,
                                unsigned * const second, bool * const is_definitely_zulu_time)
{
    // First check for a simple time and date (can be local or UTC):
    if (possible_date.length() == 19
        and std::sscanf(possible_date.c_str(), "%4u-%2u-%2u %2u:%2u:%2u",
                        year, month, day, hour, minute, second) == 6)
    {
        *is_definitely_zulu_time = false;
        return 6;
    }
    // Check for Zulu time format (must be UTC)
    else if (possible_date.length() == 20
             and std::sscanf(possible_date.c_str(), "%4u-%2u-%2uT%2u:%2u:%2uZ",
                             year, month, day, hour, minute, second) == 6)
    {
        *is_definitely_zulu_time = true;
        return 6;
    }
    // Next check a simple date
    else if (possible_date.length() == 10
             and std::sscanf(possible_date.c_str(), "%4u-%2u-%2u", year, month, day) == 3)
    {
        *hour = *minute = *second = 0;
        *is_definitely_zulu_time = false;
        return 3;
    } else
        return 0;
}


// Iso8601StringToTimeT -- Convert a time from (a special case of) ISO 8601 format to a time_t.
//
time_t Iso8601StringToTimeT(const std::string &iso_time, const TimeZone time_zone) {
    tm tm_struct;
    std::memset(&tm_struct, '\0', sizeof tm_struct);
    time_t the_time;

    unsigned year, month, day, hour, minute, second;
    bool is_definitely_zulu_time;
    const unsigned match_count = StringToBrokenDownTime(iso_time, &year, &month, &day, &hour, &minute, &second,
                                                        &is_definitely_zulu_time);

    // First check for Zulu time format (must be UTC)
    if (match_count == 6 and is_definitely_zulu_time) {
        if (time_zone == LOCAL)
            throw std::runtime_error("in TimeUtil::Iso8601StringToTimeT: local time requested in Zulu time format!");

        tm_struct.tm_year  = year - 1900;
        tm_struct.tm_mon   = month - 1;
        tm_struct.tm_mday  = day;
        tm_struct.tm_hour  = hour;
        tm_struct.tm_min   = minute;
        tm_struct.tm_sec   = second;
        the_time = ::timegm(&tm_struct);
        if (the_time == static_cast<time_t>(-1))
            throw std::runtime_error("in TimeUtil::Iso8601StringToTimeT: cannot convert '" + iso_time
                                     + "' to a time_t!");
    }

    // Check for a simple time and date (can be local or UTC):
    else if (match_count == 6) {
        tm_struct.tm_year  = year - 1900;
        tm_struct.tm_mon   = month - 1;
        tm_struct.tm_mday  = day;
        tm_struct.tm_hour  = hour;
        tm_struct.tm_min   = minute;
        tm_struct.tm_sec   = second;
        if (time_zone == LOCAL) {
            ::tzset();
            tm_struct.tm_isdst = -1;
            the_time = std::mktime(&tm_struct);
        }
        else
            the_time = TimeGm(tm_struct);
        if (the_time == static_cast<time_t>(-1))
            throw std::runtime_error("in TimeUtil::Iso8601StringToTimeT: cannot convert '" + iso_time
                                     + "' to a time_t!");
    }

    // Next check a simple date
    else if (match_count == 3) {
        tm_struct.tm_year  = year - 1900;
        tm_struct.tm_mon   = month - 1;
        tm_struct.tm_mday  = day;
        if (time_zone == LOCAL) {
            ::tzset();
            tm_struct.tm_isdst = -1;
            the_time = std::mktime(&tm_struct);
        }
        else
            the_time = ::timegm(&tm_struct);
        if (the_time == static_cast<time_t>(-1))
            throw std::runtime_error("in TimeUtil::Iso8601StringToTimeT: cannot convert '" + iso_time
                                     + "' to a time_t!");
    } else
        throw std::runtime_error("in TimeUtil::Iso8601StringToTimeT: cannot convert '" + iso_time + "' to a time_t!");

    return the_time;
}


// GetJulianDayNumber -- based on http://quasar.as.utexas.edu/BillInfo/JulianDatesG.html.
//
double GetJulianDayNumber(const unsigned year, const unsigned month, const unsigned day) {
    const unsigned A = year / 100;
    const unsigned B = A / 4;
    const unsigned C = 2 - A + B;
    const unsigned E = static_cast<unsigned>(365.25 * (year + 4716));
    const unsigned F = static_cast<unsigned>(30.6001 * (month + 1));
    return C + day + E + F - 1524.5;
}


// JulianDayNumberToYearMonthAndDay -- based on http://quasar.as.utexas.edu/BillInfo/JulianDatesG.html.
//
void JulianDayNumberToYearMonthAndDay(const double julian_day_number, unsigned * const year, unsigned * const month,
                                      unsigned * const day)
{
    const unsigned Z = static_cast<unsigned>(julian_day_number + 0.5);
    const unsigned W = static_cast<unsigned>((Z - 1867216.25) / 36524.25);
    const unsigned X = static_cast<unsigned>(W / 4);
    const unsigned A = Z + 1 + W - X;
    const unsigned B = A + 1524;
    const unsigned C = static_cast<unsigned>((B -122.1) / 365.25);
    const unsigned D = static_cast<unsigned>(365.25 * C);
    const unsigned E = static_cast<unsigned>((B - D) / 30.6001);
    const unsigned F = static_cast<unsigned>(30.6001 * E);

    *day = B - D - F;
    *month = E - 1;
    if (*month > 12)
        *month -= 12;
    if (*month == 1 or *month == 2)
        *year = C - 4715;
    else
        * year = C - 4716;


}


time_t AddDays(const time_t &start_time, const int days) {
    struct tm *start_tm(::gmtime(&start_time));
    double julian_day_number = GetJulianDayNumber(start_tm->tm_year + 1900, start_tm->tm_mon + 1, start_tm->tm_mday);
    julian_day_number += days;
    unsigned year, month, day;
    JulianDayNumberToYearMonthAndDay(julian_day_number, &year, &month, &day);
    struct tm end_tm(*start_tm);
    end_tm.tm_year = year - 1900;
    end_tm.tm_mon  = month - 1;
    end_tm.tm_mday = day;

    return TimeGm(end_tm);
}


time_t ConvertHumanDateTimeToTimeT(const std::string &human_date) {
    typedef std::multimap<std::string,PerlCompatRegExp> RegExpMap;
    // Using function static (initialised once per program) syntax, load up a std::map with time extraction patterns
    // (see standard function ::strptime) and regular expressions that correspond to various human date/time formats.
    // The human_date will be searched for a matching regular expression, and then the time will extracted with
    // the proper extraction pattern.
    static RegExpMap expressions;
    if (expressions.empty()) {
        expressions.insert(
            RegExpMap::value_type(
                "%Y %m %d %H %M %S",
                PerlCompatRegExp("[12][0-9]{3}[01][0-9][012][0-9][0-6][0-9][0-6][0-9][0-6][0-9]")));
        expressions.insert(
            RegExpMap::value_type(
                "%Y-%m-%d %T",
                PerlCompatRegExp("[12][0-9]{3}-[01][0-9]-[0123][0-9] [012][0-9]:[0-6][0-9]:[0-6][0-9]")));
        expressions.insert(
            RegExpMap::value_type(
                "%Y-%m-%dT%TZ",
                PerlCompatRegExp("[12][0-9]{3}-[01][0-9]-[0123][0-9]T[012][0-9]:[0-6][0-9]:[0-6][0-9]Z")));
        expressions.insert(
            RegExpMap::value_type(
                "%A %b %d, %Y %I:%M%p",
                PerlCompatRegExp("[[:alpha:]]+ [ 0123][0-9], [12][0-9]{3} [ 012][0-9]:[0-6][0-9][AP]M")));
        expressions.insert(
            RegExpMap::value_type(
                "%a %b %e %T %Y",
                PerlCompatRegExp("[[:alpha:]]+ [ 123][0-9] [012][0-9]:[0-6][0-9]:[0-6][0-9] [12][0-9]{3}")));
    }

    struct tm time_elements;
    bool found(false);
    for (RegExpMap::const_iterator expression = expressions.begin(); expression != expressions.end(); ++expression) {
        // Debugging statements
        //std::cout << std::string(expression->first) << "\n";
        //std::cout << std::string(expression->second.getPattern()) << std::endl;
        if (expression->second.match(human_date)) {
            found = true;
            if (::strptime(human_date.c_str(), expression->first.c_str(), &time_elements) == 0)
                return BAD_TIME_T;
            break;
        }
    }
    if (not found)
        return BAD_TIME_T;

    return std::mktime(&time_elements);
}


uint64_t GetCurrentTimeInMilliseconds() {
    timeval time_val;
    ::gettimeofday(&time_val, nullptr);
    return 1000ULL * time_val.tv_sec + (time_val.tv_usec + 500ULL) / 1000ULL;
}


uint64_t GetCurrentTimeInMicroseconds() {
    timeval time_val;
    ::gettimeofday(&time_val, nullptr);
    return 1000000ULL * time_val.tv_sec + time_val.tv_usec;
}


void Millisleep(const unsigned sleep_interval) {
    timespec time_spec;
    MillisecondsToTimeSpec(sleep_interval, &time_spec);
    ::nanosleep(&time_spec, nullptr);
}


std::string GetCurrentYear(const TimeZone time_zone) {
    time_t now;
    std::time(&now);
    return TimeTToString(now, "%Y", time_zone);
}

    
} // namespace TimeUtil
