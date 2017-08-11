/** \file    WallClockTimer.h
 *  \brief   Declaration of class WallClockTimer.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  \copyright 2003-2008 Project iVia.
 *  \copyright 2003-2008 The Regents of The University of California.
 *  \copyright 2017 Universitätsbibliothek Tübingen.
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

#ifndef WALL_CLOCK_TIMER_H
#define WALL_CLOCK_TIMER_H


#include <string>
#include <cmath>
#include "TimerUtil.h"


#define USE_CLOCK_GETTIME 1 // Set this to 0 to use gettimeofday(2).


/** \class  WallClockTimer
 *  \brief  Measure wall clock time.
 *
 *  To use a WallClockTimer, create it, call start() to begin timing, perform the action(s) you wish to time,
 *  then call stop().  The getTime() member function will return the time elapsed between the calls to start() and
 *  stop().
 */
class WallClockTimer {
    bool is_running_;
#if USE_CLOCK_GETTIME
    struct timespec time_start_;
#else
    struct timeval time_start_;
#endif
    double time_;
    std::string name_;

    static const unsigned char CUMULATIVE_FLAG = 1u << 0;
    static const unsigned char AUTO_START_FLAG = 1u << 1;
    static const unsigned char AUTO_STOP_FLAG  = 1u << 2;
public:
    enum WallClockTimerType {
        /** Time spent between multiple start/stop pairs gets accumulated. */
        CUMULATIVE                                   = CUMULATIVE_FLAG,
        /** Each call to start() resets the timer to zero. */
        NON_CUMULATIVE                               = 0,
        /** Like "CUMULATIVE" and constructor automatically calls start(). */
        CUMULATIVE_WITH_AUTO_START                   = CUMULATIVE_FLAG | AUTO_START_FLAG,
        /** Like "NON_CUMULATIVE" and constructor automatically calls start(). */
        NON_CUMULATIVE_WITH_AUTO_START               = AUTO_START_FLAG,
        /** Like "CUMULATIVE" and destructor automatically calls stop() if neccessary. */
        CUMULATIVE_WITH_AUTO_STOP                    = CUMULATIVE_FLAG | AUTO_STOP_FLAG,
        /** Like "NON_CUMULATIVE" and destructor automatically calls stop() if neccessary. */
        NON_CUMULATIVE_WITH_AUTO_STOP                = AUTO_STOP_FLAG,
        /** Like "CUMULATIVE", constructor automatically calls start() and destructor automatically calls stop()
            if neccessary. */
        CUMULATIVE_WITH_AUTO_START_AND_AUTO_STOP     = CUMULATIVE_FLAG | AUTO_START_FLAG | AUTO_STOP_FLAG,
        /** Like "NON_CUMULATIVE", constructor automatically calls start() and destructor automatically calls stop()
            if neccessary. */
        NON_CUMULATIVE_WITH_AUTO_START_AND_AUTO_STOP = AUTO_START_FLAG | AUTO_STOP_FLAG,
    };
private:
    WallClockTimerType timer_type_;
public:
    /** \brief  Constructs and initialises an object of type WallClockTimer.
     *  \param  timer_type  Specifies the desired behaviour of the timer.
     *  \param  name        Allows assignment of an optional name to a timer.  This is particularly useful for
     *                      debugging.  The name, if provided, will also be used in error reporting,
     */
    explicit WallClockTimer(const WallClockTimerType timer_type = NON_CUMULATIVE, const std::string &name = "");

    ~WallClockTimer();

    void start();
    void stop();

    /** \brief   Returns either the cumulative wall clock time between all pairs of calls to to start() and stop()
     *           or else just the last pair.
     *  \return  The elapsed wall clock time in seconds with a microsecond resolution.
     *  \note    The Timer must be stopped to return a meaningful time.
     */
    double getTime() const;

    /** \brief   Returns either the cumulative wall clock time between all pairs of calls to to start() and stop()
     *           or else just the last pair.
     *  \return  The elapsed wall clock time in rounded milliseconds.
     *  \note    The Timer must be stopped to return a meaningful time.
     */
    unsigned long getTimeInMilliseconds() const { return static_cast<unsigned long>(getTime() * 1000.0); }

    void reset() { time_ = 0.0; }

    bool isRunning() const { return is_running_; }

    std::string getName() const { return name_; }
private:
    WallClockTimer(const WallClockTimer &rhs) = delete;                  // Intentionally unimplemented!
    const WallClockTimer &operator=(const WallClockTimer &rhs) = delete; // Intentionally unimplemented!
};


// For convenience:
typedef TimerStartStopper<WallClockTimer> WallClockTimerStartStopper;


#endif // ifndef WALL_CLOCK_TIMER_H
