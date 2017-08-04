/** \file    SystemAndUserTimer.h
 *  \brief   Declaration of class SystemAndUserTimer.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  \copyright 2002-2005 Project iVia.
 *  \copyright 2002-2005 The Regents of The University of California.
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

#ifndef SYSTEM_AND_USER_TIMER_H
#define SYSTEM_AND_USER_TIMER_H


#ifndef STRING
#   include <string>
#   define STRING
#endif
#ifndef CMATH
#   include <cmath>
#   define CMATH
#endif
#ifndef TIMER_UTIL_H
#   include "TimerUtil.h"
#endif


/** \class  SystemAndUserTimer
 *  \brief  Measure execution time within a program.
 *
 *  The SystemAndUserTimer class is used to measure execution time within a
 *  program.  To use it, create a SystemAndUserTimer object, call start() to begin
 *  timing, perform the action(s) you wish to time, then call stop().
 *  The getUserTime(), getSystemTime() and getTotalTime() member
 *  functions will return the time elapsed between the calls to
 *  start() and stop().
 */
class SystemAndUserTimer {
    bool is_running_;
    struct timeval user_time_start_, system_time_start_;
    struct timeval user_time_stop_, system_time_stop_;
    double user_time_, system_time_;
    std::string name_;
public:
    enum SystemAndUserTimerType {
        CUMULATIVE,  			///< Time spent between multiple start/stop pairs gets accumulated.
        NON_CUMULATIVE, 		///< Each call to start() resets the timer to zero.
        CUMULATIVE_WITH_AUTO_STOP, 	///< Like "CUMULATIVE" and destructor automatically calls stop() if neccessary.
        NON_CUMULATIVE_WITH_AUTO_STOP 	///< Like "NON_CUMULATIVE" and destructor automatically calls stop() if neccessary.
    };
private:
    SystemAndUserTimerType timer_type_;
public:
    explicit SystemAndUserTimer(const SystemAndUserTimerType timer_type = NON_CUMULATIVE,
                                const std::string &name = "");
    ~SystemAndUserTimer();
    void start();
    void stop();

    /** \brief   Returns either the cumulative user time between all pairs of calls to
     *           to start() and stop() or else just the last pair.
     *  \return  The elapsed user time in seconds with a microsecond resolution.
     *  \note    The timer must be stopped to return a meaninful time.
     */
    double getUserTime() const;

    /** \brief   Returns either the cumulative user time between all pairs of calls to
     *           to start() and stop() or else just the last pair.
     *  \return  The elapsed user time in rounded milliseconds.
     *  \note    The timer must be stopped to return a meaninful time.
     */
    unsigned long getUserTimeInMilliseconds() const { return ::lround((getUserTime()) * 1000.0); }

    /** \brief   Returns either the cumulative system time between all pairs of calls to
     *           to start() and stop() or else just the last pair.
     *  \return  The elapsed system time in seconds with a microsecond resolution.
     *  \note    The timer must be stopped to return a meaninful time.
     */
    double getSystemTime() const;

    /** \brief   Returns either the cumulative system time between all pairs of calls to
     *           to start() and stop() or else just the last pair.
     *  \return  The elapsed system time in rounded milliseconds.
     *  \note    The timer must be stopped to return a meaninful time.
     */
    unsigned long getSystemTimeInMilliseconds() const { return ::lround((getSystemTime()) * 1000.0); }

    /** \brief   Returns either the cumulative user+system time between all pairs of calls to
     *           to start() and stop() or else just the last pair.
     *  \return  The elapsed user+system time in seconds with a microsecond resolution.
     *  \note    The Timer must be stopped to return a meaninful time.
     */
    double getTotalTime() const { return getUserTime() + getSystemTime(); }

    /** \brief   Returns either the cumulative user+system time between all pairs of calls to
     *           to start() and stop() or else just the last pair.
     *  \return  The elapsed wall clock time in rounded milliseconds.
     *  \note    The Timer must be stopped to return a meaninful time.
     */
    unsigned long getTotalTimeInMilliseconds() const { return ::lround((getUserTime() + getSystemTime()) * 1000.0); }

    void reset() { user_time_ = system_time_ = 0.0; }

    bool isRunning() const { return is_running_; }
private:
    SystemAndUserTimer(const SystemAndUserTimer &rhs);                  // Intentionally unimplemented!
    const SystemAndUserTimer &operator=(const SystemAndUserTimer &rhs); // Intentionally unimplemented!
};


// For convenience:
typedef TimerStartStopper<SystemAndUserTimer> SystemAndUserTimerStartStopper;


#endif // ifndef SYSTEM_AND_USER_TIMER_H
