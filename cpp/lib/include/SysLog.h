/** \file    SysLog.h
 *  \brief   Declaration of class SysLog.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2020 University Library of Tübingen
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
#pragma once


#include <string>
#include <syslog.h>


class SysLog {
public:
    enum LogLevel {
        LEVEL_EMERG = LOG_EMERG,
        LEVEL_ALERT = LOG_ALERT,
        LEVEL_CRIT = LOG_CRIT,
        LEVEL_ERR = LOG_ERR,
        LEVEL_WARNING = LOG_WARNING,
        LEVEL_NOTICE = LOG_NOTICE,
        LEVEL_INFO = LOG_INFO,
        LEVEL_DEBUG = LOG_DEBUG
    };

public:
    /** \param  message_prefix A string that will be prepended to all logged messages.
     *  \param  option         An or'ed together set of values described in syslog(3). (Our default is usually fine but
     *                         you may want to add LOG_PERROR (additional logging to stderr) in certain situtaions.
     *  \param  facility       One of LOG_AUTH, LOG_AUTHPRIV, LOG_CRON, LOG_DAEMON, LOG_FTP, LOG_LOCAL0 through LOG_LOCAL7
     *                         LOG_LPR, LOG_MAIL, LOG_NEWS, LOG_USER, LOG_UUCP, see syslog(3) for details.
     *                         LOG_USER or LOG_LOCAL0 through LOG_LOCAL7 are typically the only sensible values.
     *  \notice Initially all levels, exept for LEVEL_DEBUG will be logged.
     */
    explicit SysLog(const std::string &message_prefix = "", const int option = LOG_ODELAY, const int facility = LOG_USER);
    ~SysLog();

    // \brief Write a log entry.
    void log(const LogLevel level, const std::string &message);

    // \return An or'rd together set of the current log levels.
    int getLogLevels() const { return ::setlogmask(0); }

    // \brief  Sets a new set of levels that will be logged unless "new_levels" is 0.
    // \param  new_levels An or'rd together set of the levels that wile be logged after the call.
    // \return An or'rd together set of the current log levels..
    int setLogLevels(const int new_levels) { return ::setlogmask(new_levels); }
};
