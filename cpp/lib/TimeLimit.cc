/** \file    TimeLimit.h
 *  \brief   Implementation class TimeLimit.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2005 Project iVia.
 *  Copyright 2005 The Regents of The University of California.
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

#include "TimeLimit.h"
#ifndef TIMER_UTIL_H
#       include "TimerUtil.h"
#endif


TimeLimit::TimeLimit(const unsigned time_limit) {
    ::gettimeofday(&expire_time_, NULL);
    expire_time_ += time_limit;
}


unsigned TimeLimit::getRemainingTime() const {
    timeval now;
    ::gettimeofday(&now, NULL);

    timeval diff_time;
    timersub(&expire_time_, &now, &diff_time);
    if (diff_time.tv_sec < 0 or diff_time.tv_usec < 0)
        return 0;

    return TimeValToMilliseconds(diff_time);
}


bool TimeLimit::limitExceeded() const {
    timeval now;
    ::gettimeofday(&now, NULL);

    timeval diff_time;
    timersub(&expire_time_, &now, &diff_time);

    return diff_time.tv_sec < 0 or diff_time.tv_usec < 0;
}


bool TimeLimit::operator!=(const TimeLimit &rhs) {
    return TimeValToMilliseconds(expire_time_) != TimeValToMilliseconds(rhs.expire_time_);
}
