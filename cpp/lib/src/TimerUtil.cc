/** \file    TimerUtil.cc
 *  \brief   Implementations of timer-related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2004-2005 Project iVia.
 *  Copyright 2004-2005 The Regents of The University of California.
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

#include "TimerUtil.h"
#include <stdexcept>
#ifndef SYS_TIME_H
#       include <sys/time.h>
#       define SYS_TIME_H
#endif
#include "Compiler.h"
#include "util.h"


namespace TimerUtil {


SaveAndRestorePendingTimer::SaveAndRestorePendingTimer() {
    if (::gettimeofday(&start_time_, nullptr) == -1)
        throw std::runtime_error("in SaveAndRestorePendingTimer::SaveAndRestorePendingTimer: "
                                 "gettimeofday(2) failed!");
    if (::getitimer(ITIMER_REAL, &saved_itimerval_) == -1)
        throw std::runtime_error("in SaveAndRestorePendingTimer::SaveAndRestorePendingTimer: getitimer(2) failed!");
    if (::sigaction(SIGALRM, nullptr, &old_sigaction_) == -1)
        throw std::runtime_error("in SaveAndRestorePendingTimer::SaveAndRestorePendingTimer: sigaction(2) failed!");
}


SaveAndRestorePendingTimer::~SaveAndRestorePendingTimer() {
    if (::sigaction(SIGALRM, &old_sigaction_, nullptr) == -1)
        logger->error("in SaveAndRestorePendingTimer::~SaveAndRestorePendingTimer: sigaction(2) failed!");

    // If we have no saved timeout, bail out early:
    if (not timerisset(&saved_itimerval_.it_value))
        return;

    timeval end_time;
    if (::gettimeofday(&end_time, nullptr) == -1)
        logger->error("in SaveAndRestorePendingTimer::~SaveAndRestorePendingTimer: gettimeofday(2) failed!");

    // Subtract elapsed time from our saved itimer timeout value:
    timeval diff_time;
    timersub(&end_time, &start_time_, &diff_time);
    if (unlikely(diff_time.tv_sec < 0 or diff_time.tv_usec < 0))
        diff_time.tv_sec = diff_time.tv_usec = 0;
    timersub(&saved_itimerval_.it_value, &diff_time, &saved_itimerval_.it_value);
    if (unlikely(saved_itimerval_.it_value.tv_sec < 0 or saved_itimerval_.it_value.tv_usec < 0))
        saved_itimerval_.it_value.tv_sec = saved_itimerval_.it_value.tv_usec = 0;

    if (::setitimer(ITIMER_REAL, &saved_itimerval_, nullptr) == -1)
        logger->error("in SaveAndRestorePendingTimer::~SaveAndRestorePendingTimer: setitimer(2) failed!");
}


unsigned long SaveAndRestorePendingTimer::getRemainingTimeOnPendingTimer() const {
    const unsigned long remaining_time(saved_itimerval_.it_value.tv_sec * 1000000UL
                                       + saved_itimerval_.it_value.tv_usec);
    return remaining_time == 0 ? ULONG_MAX : remaining_time;
}


unsigned malarm(const unsigned milliseconds) {
    itimerval new_values, old_values;
    MillisecondsToTimeVal(milliseconds, &new_values.it_value);
    MillisecondsToTimeVal(0, &new_values.it_interval);
    if (::setitimer(ITIMER_REAL, &new_values, &old_values) != 0)
        return static_cast<unsigned>(-1);

    return TimeValToMilliseconds(old_values.it_value);
}


unsigned LeftOverTime(const struct timeval &start_time, const struct timeval &end_time) {
    struct timeval diff_time;
    timersub(&end_time, &start_time, &diff_time);
    if (diff_time.tv_sec < 0 or diff_time.tv_usec < 0)
        return 0;

    return TimeValToMilliseconds(diff_time);
}


void SubtractLeftOverTime(const struct timeval &start_time, const struct timeval &end_time,
                          unsigned * const timeout)
{
    const unsigned difference = LeftOverTime(start_time, end_time);
    if (difference < *timeout)
        *timeout -= difference;
    else
        *timeout = 0;
}


uint64_t MillisecondsSinceEpoch() {
    timeval time_val;
    ::gettimeofday(&time_val, nullptr);
    return 1000UL * time_val.tv_sec + (time_val.tv_usec + 500UL) / 1000UL;
}


} // namespace TimerUtil
