/** \file    TimeUtil.cc
 *  \brief   Declarations of time-related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 *  \author  Wagner Truppel
 */

/*
 *  Copyright 2003-2009 Project iVia.
 *  Copyright 2003-2009 The Regents of The University of California.
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

#include "TimeUtil.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/time.h>
#include "Compiler.h"
#include "Locale.h"
#include "PerlCompatRegExp.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "WebUtil.h"
#include "util.h"


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
std::string GetCurrentDateAndTime(const std::string &format, const TimeZone time_zone, const std::string &time_locale) {
    time_t now;
    std::time(&now);
    return TimeTToString(now, format, time_zone, time_locale);
}


void GetCurrentDate(unsigned * const year, unsigned * const month, unsigned * const day, const TimeZone time_zone) {
    const time_t now(::time(nullptr));
    struct tm tm;
    if (unlikely((time_zone == LOCAL ? ::localtime_r(&now, &tm) : ::gmtime_r(&now, &tm)) == nullptr))
        LOG_ERROR("time conversion error!");

    *year  = tm.tm_year + 1900;
    *month = tm.tm_mon + 1;
    *day   = tm.tm_mday;
}


// TimeTToLocalTimeString -- Convert a time from a time_t to a string.
//
std::string TimeTToString(const time_t &the_time, const std::string &format, const TimeZone time_zone,
                          const std::string &time_locale)
{
    Locale locale(time_locale, LC_TIME);

    struct tm tm;
    if (unlikely((time_zone == LOCAL ? ::localtime_r(&the_time, &tm) : ::gmtime_r(&the_time, &tm)) == nullptr))
        LOG_ERROR("time conversion error! (time_locale = " + time_locale + ")");
    char time_buf[50 + 1];
    errno = 0;
    if (unlikely(std::strftime(time_buf, sizeof(time_buf), format.c_str(), &tm) == 0 or errno != 0))
        LOG_ERROR("strftime(3) failed! (format: " + format + ")");
    return time_buf;
}


bool StringToTimeT(const std::string &time_str, time_t * const unix_time) {
    char *endptr;
    errno = 0;
    *unix_time = std::strtol(time_str.c_str(), &endptr, 10);
    if (errno != 0 or *endptr != '\0' or *unix_time < 0) {
        *unix_time = BAD_TIME_T;
        return false;
    }

    return true;
}


time_t StringToTimeT(const std::string &time_str) {
    time_t unix_time;
    if (StringToTimeT(time_str, &unix_time))
        return unix_time;
    LOG_ERROR("\"" + time_str + "\" cannot be converted to a valid time_t!");
}


time_t TimeGm(const struct tm &tm) {
    const char * const saved_time_zone(::getenv("TZ"));

    // Set the time zone to UTC:
    ::setenv("TZ", "UTC", /* overwrite = */true);
    ::tzset();

    struct tm temp_tm(tm);
    errno = 0;
    time_t ret_val(::mktime(&temp_tm));
    if (unlikely(errno != 0))
        ret_val = BAD_TIME_T;

    // Restore the original time zone:
    if (saved_time_zone != nullptr)
        ::setenv("TZ", saved_time_zone, 1);
    else
        ::unsetenv("TZ");
    ::tzset();

    return ret_val;
}


static const ThreadSafeRegexMatcher ISO8601_MATCHER(
    "[0-9]{4}-[0-9]{2}-[0-9]{2}([[:space:]]|T){1}[0-9]{2}:[0-9]{2}:[0-9]{2}(\\+|\\-|\\s){1}[0-9]{2}(:)[0-9]{2}");


// Strips out the colon in the date string to preserve compatibility with CentOS.
static void NormalizeTimeZoneOffset(std::string * const date_str) {
    if (ISO8601_MATCHER.match(*date_str))
        date_str->erase(date_str->length() - 3, 1);
}


// If "format_string" ends with "%Z", we remove it and extract the corresponding time zone name into "*time_zone_name".
static void ExtractOptionalTimeZoneName(std::string * const date_str, std::string * const format_string, std::string * const time_zone_name) {
    if (StringUtil::EndsWith(*format_string, "%Z")) {
        *format_string = StringUtil::RightTrim(format_string->substr(0, format_string->length() - 2));
        auto ch(date_str->rbegin());
        while (ch != date_str->rend() and
               (StringUtil::IsAsciiLetter(*ch)
                or *ch == ' ' and (ch + 1) != date_str->rend() and StringUtil::IsAsciiLetter(*(ch + 1))))
        {
            *time_zone_name += *ch;
            ++ch;
        }

        date_str->resize(date_str->length() - time_zone_name->length());
        StringUtil::RightTrim(date_str);
        std::reverse(time_zone_name->begin(), time_zone_name->end());
    }
}


// If time_offset refers to a valid RFC 3339 time offset, we adjust *date_time for it and return true.
// O/w we set *date_time to BAD_TIME_T and return false.
static bool AdjustForTimeOffset(time_t * const date_time, const char * const time_offset) {
    if (std::strlen(time_offset) != 6 /* sign + mm:ss */ or time_offset[3] != ':' or not StringUtil::IsDigit(time_offset[1])
        or not StringUtil::IsDigit(time_offset[2]) or not StringUtil::IsDigit(time_offset[4])
        or not StringUtil::IsDigit(time_offset[5]))
    {
        *date_time = BAD_TIME_T;
        return false;
    }

    int offset(0);
    offset += (time_offset[1] - '0') * 36000; // tens of hours
    offset += (time_offset[2] - '0') *  3600; // hours
    offset += (time_offset[4] - '0') *   600; // tens of minutes
    offset += (time_offset[5] - '0') *    60; // minutes

    if (time_offset[0] == '+')
        *date_time -= offset;
    else
        *date_time += offset;

    return true;
}


const std::unordered_map<std::string, std::string> time_zone_names_to_offsets_map{
    { "GMT",                    "+00:00" },
    { "Hora de verano romance", "+01:00" },
    { "PDT",                    "-07:00" },
    { "PST",                    "-08:00" },
    { "UTC",                    "+00:00" },
};


static void CorrectForSymbolicTimeZone(struct tm * const tm, const std::string &time_zone_name) {
    errno = 0;
    time_t converted_tm(::mktime(tm));
    if (unlikely(errno != 0))
        LOG_ERROR("bad time conversion!");

    const auto time_zone_name_and_offset(time_zone_names_to_offsets_map.find(time_zone_name));
    if (time_zone_name_and_offset == time_zone_names_to_offsets_map.cend())
        LOG_ERROR("Unhandled timezone symbolic name '" + time_zone_name + "'");

    if (not AdjustForTimeOffset(&converted_tm, time_zone_name_and_offset->second.c_str()))
        LOG_ERROR("couldn't adjust time_t " + TimeTToString(converted_tm) + " with offset " + time_zone_name_and_offset->second);
}


// Strptime()/CentOS idiosyncrasies:
//      - Strptime() supports "%Z" to denote time zone names, but it doesn't perform the time conversion.
//        Furthermore, the above format specifier is apparently broken on CentOS 7.
//        So, we need to override the behaviour on our end.
//      - "%z" does not support a colon in the time zone offset on CentOS.
//        So, we need to strip it out before we pass it to the function.
static bool StringToStructTmHelper(std::string date_str, std::string optional_strptime_format, struct tm * const tm) {
    if (optional_strptime_format.empty()) {
        const auto unix_time(WebUtil::ParseWebDateAndTime(date_str));
        if (unix_time == TimeUtil::BAD_TIME_T)
            return false;
        return ::gmtime_r(&unix_time, tm) != nullptr;
    }

    std::vector<std::string> locales;

    // Optional locale specification(s)?
    if (optional_strptime_format[0] == '(') {
        const size_t closing_paren_pos(optional_strptime_format.find(')', 1));
        if (unlikely(closing_paren_pos == std::string::npos or closing_paren_pos == 1))
            LOG_ERROR("bad locale specification \"" + optional_strptime_format + "\"!");
        const std::string locale_specifications(optional_strptime_format.substr(1, closing_paren_pos - 1));
        StringUtil::Split(locale_specifications, '|', &locales, /* suppress_empty_components = */true);
        if (unlikely(locales.empty()))
            LOG_ERROR("no locales found between the paretheses \"" + locale_specifications + "\"!");
        optional_strptime_format = optional_strptime_format.substr(closing_paren_pos + 1);
    } else
        locales.emplace_back("");

    NormalizeTimeZoneOffset(&date_str);

    std::unordered_set<std::string> format_string_splits;
    // try available format strings until a matching one is found
    if (StringUtil::SplitThenTrimWhite(optional_strptime_format, '|', &format_string_splits) == 0)
        LOG_ERROR("missing format string!");

    for (auto format_string : format_string_splits) {
        std::string time_zone_name;
        ExtractOptionalTimeZoneName(&date_str, &format_string, &time_zone_name);

        for (const auto &locale : locales) {
            std::unique_ptr<Locale> locale_ptr;
            if (not locale.empty())
                locale_ptr.reset(new Locale(locale, LC_ALL));

            std::memset(tm, 0, sizeof(*tm));
            const char * const last_char(::strptime(date_str.c_str(), format_string.c_str(), tm));
            if (last_char != nullptr and *last_char == '\0') {
                if (not time_zone_name.empty())
                    CorrectForSymbolicTimeZone(tm, time_zone_name);

                if (tm->tm_mday == 0)
                    tm->tm_mday = 1;
                return true;
            }
        }
    }

    return false;
}


bool ConvertFormat(const std::string &from_format, const std::string &to_format, std::string * const datetime, const TimeZone time_zone) {
    struct tm tm;
    if (not StringToStructTmHelper(*datetime, from_format, &tm))
        return false;

    const time_t time(TimeGm(tm));
    if (unlikely(time == BAD_TIME_T))
        return false;

    *datetime = TimeTToString(time, to_format, time_zone);

    return true;
}


unsigned StringToBrokenDownTime(const std::string &possible_date, unsigned * const year, unsigned * const month,
                                unsigned * const day, unsigned * const hour, unsigned * const minute,
                                unsigned * const second, int * const hour_offset, int * const minute_offset,
                                bool * const is_definitely_zulu_time)
{
    *hour_offset = *minute_offset = 0;
    char plus_or_minus[1 + 1];
    char space_or_t[1 + 1];

    // First check for a simple time and date (can be local or UTC):
    if (possible_date.length() == 19
        and std::sscanf(possible_date.c_str(), "%4u-%2u-%2u%[Tt ]%2u:%2u:%2u",
                        year, month, day, space_or_t, hour, minute, second) == 7)
    {
        *is_definitely_zulu_time = false;
        return 7;
    }
    // Check for ISO 8601 w/ offset:
    else if (possible_date.length() == 25
             and std::sscanf(possible_date.c_str(), "%4u-%2u-%2u%*[Tt]%2u:%2u:%2u%[+-]%2d:%2d",
                             year, month, day, hour, minute, second, plus_or_minus, hour_offset, minute_offset) == 9)
    {
        if (plus_or_minus[0] == '-') {
            *hour_offset   = -*hour_offset;
            *minute_offset = -*minute_offset;
        }

        *is_definitely_zulu_time = true;
        return 9;
    }
    // Check for Zulu time format (must be UTC)
    else if (possible_date.length() == 20
             and std::sscanf(possible_date.c_str(), "%4u-%2u-%2u%*[Tt]%2u:%2u:%2u%*[Zz]",
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


bool StringToYear(const std::string &possible_date, unsigned * const year) {
    unsigned month, day, hour, minute, second;
    int hour_offset, minute_offset;
    bool is_definitely_zulu_time;
    return (StringToBrokenDownTime(possible_date, year, &month, &day, &hour, &minute, &second,
                                   &hour_offset, &minute_offset, &is_definitely_zulu_time) > 0);
}


bool Iso8601StringToTimeT(const std::string &iso_time, time_t * const converted_time, std::string * const err_msg,
                          const TimeZone time_zone)
{
    tm tm_struct;
    std::memset(&tm_struct, '\0', sizeof tm_struct);

    unsigned year, month, day, hour, minute, second;
    int hour_offset, minute_offset;
    bool is_definitely_zulu_time;
    const unsigned match_count = StringToBrokenDownTime(iso_time, &year, &month, &day, &hour, &minute, &second,
                                                        &hour_offset, &minute_offset, &is_definitely_zulu_time);
    // First check for Zulu time format w/ an offset
    if (match_count == 9) {
        if (time_zone == LOCAL) {
            *err_msg = "local time requested in Zulu time format!";
            return false;
        }

        tm_struct.tm_year  = year - 1900;
        tm_struct.tm_mon   = month - 1;
        tm_struct.tm_mday  = day;
        tm_struct.tm_hour  = hour - hour_offset;
        tm_struct.tm_min   = minute - minute_offset;
        tm_struct.tm_sec   = second;
        *converted_time = ::timegm(&tm_struct);
        if (*converted_time == static_cast<time_t>(-1)) {
            *err_msg = "cannot convert '" + iso_time + "' to a time_t!";
            return false;
        }
    }

    // Now check for Zulu time format w/o an offset (must be UTC)
    else if (match_count == 6 and is_definitely_zulu_time) {
        if (time_zone == LOCAL) {
            *err_msg = "local time requested in Zulu time format!";
            return false;
        }

        tm_struct.tm_year  = year - 1900;
        tm_struct.tm_mon   = month - 1;
        tm_struct.tm_mday  = day;
        tm_struct.tm_hour  = hour;
        tm_struct.tm_min   = minute;
        tm_struct.tm_sec   = second;
        *converted_time = ::timegm(&tm_struct);
        if (*converted_time == static_cast<time_t>(-1)) {
            *err_msg = "cannot convert '" + iso_time + "' to a time_t!";
            return false;
        }
    }

    // Check for a simple time and date (can be local or UTC):
    else if (match_count == 7) {
        tm_struct.tm_year  = year - 1900;
        tm_struct.tm_mon   = month - 1;
        tm_struct.tm_mday  = day;
        tm_struct.tm_hour  = hour;
        tm_struct.tm_min   = minute;
        tm_struct.tm_sec   = second;
        if (time_zone == LOCAL) {
            ::tzset();
            tm_struct.tm_isdst = -1;

            errno = 0;
            *converted_time = std::mktime(&tm_struct);
            if (unlikely(errno != 0))
                LOG_ERROR("bad time conversion! (2)");
        }
        else
            *converted_time = TimeGm(tm_struct);
        if (*converted_time == static_cast<time_t>(-1)) {
            *err_msg = "cannot convert '" + iso_time + "' to a time_t!";
            return false;
        }
    }

    // Next check a simple date
    else if (match_count == 3) {
        tm_struct.tm_year  = year - 1900;
        tm_struct.tm_mon   = month - 1;
        tm_struct.tm_mday  = day;
        if (time_zone == LOCAL) {
            ::tzset();
            tm_struct.tm_isdst = -1;

            errno = 0;
            *converted_time = std::mktime(&tm_struct);
            if (unlikely(errno != 0))
                LOG_ERROR("bad time conversion! (3)");
        } else
            *converted_time = ::timegm(&tm_struct);
        if (*converted_time == static_cast<time_t>(-1)) {
            *err_msg = "cannot convert '" + iso_time + "' to a time_t!";
            return false;
        }
    } else {
        *err_msg = "cannot convert '" + iso_time + "' to a time_t!";
        return false;
    }

    return true;
}


// Iso8601StringToTimeT -- Convert a time from (a special case of) ISO 8601 format to a time_t.
//
time_t Iso8601StringToTimeT(const std::string &iso_time, const TimeZone time_zone) {
    time_t converted_time;
    std::string err_msg;
    if (Iso8601StringToTimeT(iso_time, &converted_time, &err_msg, time_zone))
        return converted_time;

    throw std::runtime_error("in TimeUtil::Iso8601StringToTimeT: " + err_msg);
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


namespace {


//
// Taken from https://stackoverflow.com/questions/33964461/handling-julian-dates-in-c11-14
//


constexpr auto jdiff() {
    using namespace std::chrono_literals;
    return 58574100h;
}


struct jdate_clock {
    using rep        = double;
    using period     = std::ratio<86400>;
    using duration   = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<jdate_clock>;

    static time_point now() noexcept {
        using namespace std::chrono;
        return time_point{duration{system_clock::now().time_since_epoch()} + jdiff()};
    }
};


} // unnamed namespace


double GetJulianDayNumber() {
    return jdate_clock::now().time_since_epoch().count();
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

    errno = 0;
    const time_t ret_val(std::mktime(&time_elements));
    if (unlikely(errno != 0))
        return BAD_TIME_T;

    return ret_val;
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


time_t _mkgmtime(const struct tm &tm) {
    // Month-to-day offset for non-leap-years.
    static const int month_day[]{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

    // Most of the calculation is easy; leap years are the main difficulty.
    int month = tm.tm_mon % 12;
    int year = tm.tm_year + tm.tm_mon / 12;
    if (month < 0) {   // Negative values % 12 are still negative.
        month += 12;
        --year;
    }

    // This is the number of Februaries since 1900.
    const int year_for_leap = (month > 1) ? year + 1 : year;

    const time_t rt(tm.tm_sec                                  // Seconds
                    + 60 * (tm.tm_min                          // Minute = 60 seconds
                    + 60 * (tm.tm_hour                         // Hour = 60 minutes
                    + 24 * (month_day[month] + tm.tm_mday - 1  // Day = 24 hours
                    + 365 * (year - 70)                        // Year = 365 days
                    + (year_for_leap - 69) / 4                 // Every 4 years is     leap...
                    - (year_for_leap - 1) / 100                // Except centuries...
                    + (year_for_leap + 299) / 400))));         // Except 400s.
    return rt < 0 ? -1 : rt;
}


time_t UTCStructTmToTimeT(const struct tm &tm) {
    // Month-to-day offset for non-leap-years.
    static const int month_day[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

    // Most of the calculation is easy; leap years are the main difficulty.
    time_t month(tm.tm_mon % 12);
    time_t year(tm.tm_year + tm.tm_mon / 12);
    if (month < 0) { // Negative values % 12 are still negative.
        month += 12;
        --year;
    }

    // This is the number of Februaries since 1900.
    const time_t year_for_leap((month > 1) ? year + 1 : year);

    const time_t retval_candidate(tm.tm_sec        // Seconds
        + 60 * (tm.tm_min                          // Minute = 60 seconds
        + 60 * (tm.tm_hour                         // Hour = 60 minutes
        + 24 * (month_day[month] + tm.tm_mday - 1  // Day = 24 hours
        + 365 * (year - 70)                        // Year = 365 days
        + (year_for_leap - 69) / 4                 // Every 4 years is     leap...
        - (year_for_leap - 1) / 100                // Except centuries...
        + (year_for_leap + 299) / 400))));         // Except 400s.
    return retval_candidate < 0 ? BAD_TIME_T : retval_candidate;
}


// See https://www.rfc-editor.org/rfc/rfc822.txt section 5.1.
static bool ZoneAdjustment(const std::string &rfc822_zone, time_t * const adjustment) {
    if (rfc822_zone == "GMT" or rfc822_zone == "UT")
        *adjustment = 0;
    else if (rfc822_zone == "EST")
        *adjustment = +5 * 3600;
    else if (rfc822_zone == "EDT")
        *adjustment = +4 * 3600;
    else if (rfc822_zone == "CST")
        *adjustment = +6 * 3600;
    else if (rfc822_zone == "CDT")
        *adjustment = +5 * 3600;
    else if (rfc822_zone == "MST")
        *adjustment = +7 * 3600;
    else if (rfc822_zone == "MDT")
        *adjustment = +6 * 3600;
    else if (rfc822_zone == "PST")
        *adjustment = +8 * 3600;
    else if (rfc822_zone == "PDT")
        *adjustment = +7 * 3600;
    else if (rfc822_zone == "A")
        *adjustment = -1 * 3600;
    else if (rfc822_zone == "M")
        *adjustment = -12 * 3600;
    else if (rfc822_zone == "N")
        *adjustment = +1 * 3600;
    else if (rfc822_zone == "Y")
        *adjustment = +12 * 3600;
    else // Unrecognized tiem zone.
        return false;

    return true;
}


// In order to understand this insanity, have a look at section 5.1 of RFC822.  Please note that we also support 4-digit
// years as specified by RFC1123.
bool ParseRFC1123DateTime(const std::string &date_time_candidate, time_t * const date_time) {
    const auto first_comma_pos(date_time_candidate.find(','));
    const std::string::size_type start_pos(first_comma_pos == std::string::npos ? 0 : first_comma_pos + 1);
    std::string simplified_candidate(StringUtil::TrimWhite(date_time_candidate.substr(start_pos)));

    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(
        "^(\\d{1,2}) (...) (\\d{2}|\\d{4}) (\\d{2}:\\d{2}(:\\d{2})?)"));
    if (not matcher->matched(simplified_candidate)) {
        *date_time = BAD_TIME_T;
        return false;
    }

    const bool double_digit_year((*matcher)[3].length() == 2);
    const bool has_seconds((*matcher)[4].length() == 8);

    std::string format(double_digit_year ? "%d %b %y %H:%M" : "%d %b %Y %H:%M");
    if (has_seconds)
        format += ":%S";

    static RegexMatcher * const local_differential_matcher(RegexMatcher::RegexMatcherFactory("[+-]?\\d{4}$"));
    time_t local_differential_offset(0);
    if (local_differential_matcher->matched(simplified_candidate)) {
        std::string local_differential_time((*local_differential_matcher)[0]);

        // Trim the local differential time offset off the end of our date and time string:
        auto cp(simplified_candidate.begin() + simplified_candidate.length() - local_differential_time.length() - 1);
        while (*cp == ' ')
            --cp;
        simplified_candidate.resize(cp - simplified_candidate.begin() + 1);

        bool is_negative(false);
        if (local_differential_time[0] == '+')
            local_differential_time = local_differential_time.substr(1);
        else if (local_differential_time[0] == '-') {
            is_negative = true;
            local_differential_time = local_differential_time.substr(1);
        }

        local_differential_offset = (local_differential_time[0] - '0') * 600 + (local_differential_time[1] - '0') * 60
                                    + (local_differential_time[2] - '0') * 10 + (local_differential_time[3] - '0');
        local_differential_offset *= 60; // convert minutes to seconds

        if (is_negative)
            local_differential_offset = -local_differential_offset;
    } else {
        const auto last_space_pos(simplified_candidate.rfind(' '));
        if (unlikely(last_space_pos == std::string::npos
                     or not ZoneAdjustment(simplified_candidate.substr(last_space_pos + 1), &local_differential_offset)))
            return false;
        simplified_candidate.resize(last_space_pos);
    }

    struct tm tm;
    std::memset(&tm, 0, sizeof tm);
    const char * const first_not_processed(::strptime(simplified_candidate.c_str(), format.c_str(), &tm));
    if (first_not_processed == nullptr or *first_not_processed != '\0') {
        *date_time = BAD_TIME_T;
        return false;
    }

    tm.tm_gmtoff = 0;
    *date_time = TimeGm(tm) + local_differential_offset;

    return true;
}


bool ParseRFC3339DateTime(const std::string &date_time_candidate, time_t * const date_time) {
    // Convert possible lowercase t and z to uppercase:
    const std::string normalised_date_time_candidate(StringUtil::ASCIIToUpper(date_time_candidate));

    struct tm tm;
    std::memset(&tm, '\0', sizeof tm);
    time_t rounded_second_offset;
    const char *cp(::strptime(normalised_date_time_candidate.c_str(), "%Y-%m-%dT%H:%M:%S", &tm));
    if (cp == nullptr) {
        *date_time = BAD_TIME_T;
        return false;
    }
    if (*cp != '.')
        rounded_second_offset = 0;
    else { // Handle optional single-digit fractional second.
        ++cp;
        if (not StringUtil::IsDigit(*cp)) {
            *date_time = BAD_TIME_T;
            return false;
        }
        rounded_second_offset = (*cp >= '5') ? 1 : 0;
        while (StringUtil::IsDigit(*cp))
            ++cp;
    }

    // If the input format is correct cp now either points to the final Z or the sign of the optional time offset.
    if (*cp == 'Z') {
        *date_time = TimeGm(tm) + rounded_second_offset;
        return true;
    } else if (*cp == '+' or *cp == '-') {
        *date_time = TimeGm(tm) + rounded_second_offset;
        return AdjustForTimeOffset(date_time, cp);
    } else {
        *date_time = BAD_TIME_T;
        return false;
    }
}


std::string StructTmToString(const struct tm &tm) {
    std::string tm_as_string;
    tm_as_string += "tm_sec: " + std::to_string(tm.tm_sec);
    tm_as_string += ", tm_min: " + std::to_string(tm.tm_min);
    tm_as_string += ", tm_hour: " + std::to_string(tm.tm_hour);
    tm_as_string += ", tm_mday: " + std::to_string(tm.tm_mday);
    tm_as_string += ", tm_mon: " + std::to_string(tm.tm_mon);
    tm_as_string += ",tm_year: " + std::to_string(tm.tm_year);
    tm_as_string += ",tm_wday: " + std::to_string(tm.tm_wday);
    tm_as_string += ",tm_yday: " + std::to_string(tm.tm_yday);
    tm_as_string += ",tm_isdst: " + std::to_string(tm.tm_isdst);
    tm_as_string += ",tm_gmtoff: " + std::to_string(tm.tm_gmtoff);
    tm_as_string += ",tm_zone: ";
    tm_as_string += (tm.tm_zone == nullptr) ? "NULL" : tm.tm_zone;
    return tm_as_string;
}


struct tm StringToStructTm(std::string date_str, std::string optional_strptime_format) {
    struct tm tm;
    if (likely(StringToStructTmHelper(date_str, optional_strptime_format, &tm)))
        return tm;

    throw std::runtime_error("TimeUtil::StringToStructTm: gmtime(3) failed to convert a struct tm! (" + date_str + ")");
}


bool StringToStructTm(struct tm * const tm, std::string date_str, std::string optional_strptime_format) {
    return StringToStructTmHelper(date_str, optional_strptime_format, tm);
}


double DiffStructTm(struct tm end, struct tm beginning) {
    return ::difftime(::timegm(&end), ::timegm(&beginning));
}


struct tm GetCurrentTimeGMT() {
    time_t now(time(nullptr));
    return *std::gmtime(&now);
}


int IsDateInRange(time_t first, time_t last, time_t date) {
    const auto diff_first(::difftime(first, date));
    if (diff_first > 0.0)
        return -1;
    else if (diff_first == 0.0)
        return 0;

    const auto diff_last(::difftime(date, last));
    if (diff_last <= 0.0)
        return 0;
    else
        return 1;
}


bool AscTimeToStructTm(std::string asctime_output, struct tm * const tm) {
    // Example: "Fri Sep 16 13:29:51 1994"
    if (asctime_output.length() != 24)
        return false;

    // This is crazy but true: asctime(3) can generate a leading
    // space or zero for single digit days of the month => we need to normalise this!
    if (asctime_output[8] == '0')
        asctime_output[8] = ' ';

    return StringToStructTmHelper(asctime_output, "(en_US)%a %b %d %T %Y", tm);
}


bool IsLeapYear(const unsigned year) {
    if ((year % 4) != 0)
        return false;

    if ((year % 100) == 0)
        return (year % 400) == 0;

    return true;
}


bool IsLeapYear(const std::string &year_candidate) {
    unsigned year;
    if (StringUtil::ToUnsigned(year_candidate, &year))
        return IsLeapYear(year);
    return false;
}


} // namespace TimeUtil
