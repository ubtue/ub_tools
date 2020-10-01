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
#include "SysLog.h"


SysLog::SysLog(const int option, const int facility, const std::string &message_prefix) {
    ::openlog(message_prefix.c_str(), option, facility);
    ::setlogmask(LOG_EMERG | LOG_ALERT | LOG_CRIT | LOG_ERR | LOG_WARNING | LOG_NOTICE | LOG_INFO);
}


SysLog::~SysLog() {
    ::closelog();
}


void SysLog::log(const LogLevel level, const std::string &message) {
    ::syslog(level, "%s", message.c_str());
}
