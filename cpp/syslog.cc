/** \brief A tool for logging to syslogd in a shell script.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
// clang-format off
// if the following order gets changed, #undef might not have the desired effect
#include <cstdlib>
#include <cstring>
#include "StringUtil.h"
#include "SysLog.h"
#undef LOG_WARNING
#undef LOG_INFO
#undef LOG_DEBUG
#include "util.h"
// clang-format on


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "[--facility=facility] [--message-prefix=prefix [--also-log-to-stderr] log_level log_message\n"
        "where \"facility\" must be one of AUTH, AUTHPRIV, DAEMON, LOCAL0..LOCAL7 or USER. (LOCAL0 is the default.) \n"
        "and \"log_level\" must be be one of EMERG, ALERT, CRIT, ERR, WARNING, NOTICE, INFO or DEBUG.\n");
}


int StringToFacility(const std::string &facility_as_string) {
    if (facility_as_string == "AUTH")
        return LOG_AUTH;
    if (facility_as_string == "AUTHPRIV")
        return LOG_AUTHPRIV;
    if (facility_as_string == "DAEMON")
        return LOG_DAEMON;
    if (facility_as_string == "LOCAL0")
        return LOG_LOCAL0;
    if (facility_as_string == "LOCAL1")
        return LOG_LOCAL1;
    if (facility_as_string == "LOCAL2")
        return LOG_LOCAL2;
    if (facility_as_string == "LOCAL3")
        return LOG_LOCAL3;
    if (facility_as_string == "LOCAL4")
        return LOG_LOCAL4;
    if (facility_as_string == "LOCAL5")
        return LOG_LOCAL5;
    if (facility_as_string == "LOCAL6")
        return LOG_LOCAL6;
    if (facility_as_string == "LOCAL7")
        return LOG_LOCAL7;
    if (facility_as_string == "USER")
        return LOG_USER;

    LOG_ERROR("\"" + facility_as_string + "\" is not a valid facility!");
}


SysLog::LogLevel StringToLogLevel(const std::string &level_as_string) {
    if (level_as_string == "EMERG")
        return SysLog::LEVEL_EMERG;
    if (level_as_string == "ALERT")
        return SysLog::LEVEL_ALERT;
    if (level_as_string == "CRIT")
        return SysLog::LEVEL_CRIT;
    if (level_as_string == "ERR")
        return SysLog::LEVEL_ERR;
    if (level_as_string == "WARNING")
        return SysLog::LEVEL_WARNING;
    if (level_as_string == "NOTICE")
        return SysLog::LEVEL_NOTICE;
    if (level_as_string == "INFO")
        return SysLog::LEVEL_INFO;
    if (level_as_string == "DEBUG")
        return SysLog::LEVEL_DEBUG;

    LOG_ERROR("\"" + level_as_string + "\" is not a valid log level!");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3 or argc > 6)
        Usage();

    int facility(LOG_LOCAL0);
    if (StringUtil::StartsWith(argv[1], "--facility=")) {
        facility = StringToFacility(argv[1] + __builtin_strlen("--facility="));
        --argc, ++argv;
    }

    std::string message_prefix;
    if (StringUtil::StartsWith(argv[1], "--message-prefix=")) {
        message_prefix = argv[1] + __builtin_strlen("--message-prefix=");
        --argc, ++argv;
    }

    int option(LOG_ODELAY);
    if (std::strcmp(argv[1], "--also-log-to-stderr") == 0) {
        option |= LOG_PERROR;
        --argc, ++argv;
    }

    if (argc != 3)
        Usage();

    SysLog sys_logger(message_prefix, option, LOG_USER);
    sys_logger.log(StringToLogLevel(argv[1]), argv[2]);

    return EXIT_SUCCESS;
}
