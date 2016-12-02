/** \file    TimerUtil.h
 *  \brief   Timer related utility functions and classes.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2003-2008 Project iVia.
 *  Copyright 2003-2008 The Regents of The University of California.
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

#ifndef TIMER_UTIL_H
#define TIMER_UTIL_H


#include <string>
#include <climits>
#include <inttypes.h>
#include <signal.h>
#include <sys/time.h>


// operator< -- See e.g. man gettimeofday(2) for documentation of "struct timeval".
//
inline bool operator<(const struct timeval &tv1, const struct timeval &tv2) {
    if (tv1.tv_sec < tv2.tv_sec)
        return true;
    if (tv1.tv_sec > tv2.tv_sec)
        return false;

    // At this point tv1.tv_sec == tv2.tv_sec
    return tv1.tv_usec < tv2.tv_usec;
}


// operator> -- See e.g. man gettimeofday(2) for documentation of "struct timeval".
//
inline bool operator>(const struct timeval &tv1, const struct timeval &tv2) {
    if (tv1.tv_sec > tv2.tv_sec)
        return true;
    if (tv1.tv_sec < tv2.tv_sec)
        return false;

    // At this point tv1.tv_sec == tv2.tv_sec
    return tv1.tv_usec > tv2.tv_usec;
}


/** Returns the equivalent time in milliseconds for "tv", rounded to the nearest millisecond. */
inline long TimeValToMilliseconds(const struct timeval &tv) {
    return 1000 * tv.tv_sec + (tv.tv_usec + 500) / 1000;
}


/** Returns the equivalent time in microseconds for "tv". */
inline long TimeValToMicroseconds(const struct timeval &tv) {
    return 1000000 * tv.tv_sec + tv.tv_usec;
}


/** Converts milliseconds "ms" to a struct timeval "tv". */
inline void MillisecondsToTimeVal(const int ms, struct timeval * const tv)
{
    tv->tv_sec  = ms / 1000;
    tv->tv_usec = (ms % 1000) * 1000;
}


/** Converts milliseconds "ms" to a struct timespec "ts". */
inline void MillisecondsToTimeSpec(const int ms, struct timespec * const ts) {
    ts->tv_sec  = ms / 1000;
    ts->tv_nsec = (ms % 1000) * 1000000L;
}


inline double TimevalToDouble(const struct timeval &tv) {
    return static_cast<double>(tv.tv_sec) + static_cast<double>(tv.tv_usec) * 1.0e-6;
}


inline double TimespecToDouble(const struct timespec &ts) {
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1.0e-9;
}


inline timeval &operator+=(timeval &lhs, const unsigned milliseconds) {
    // Are we adding less than 1 second?
    if (milliseconds < 1000) // Yes!
        lhs.tv_usec += milliseconds * static_cast<suseconds_t>(1000);
    else { // No, more than 1 second.
        const time_t additional_seconds(milliseconds / 1000);
        lhs.tv_sec += additional_seconds;
        lhs.tv_usec += (milliseconds - additional_seconds * 1000)  * static_cast<suseconds_t>(1000);
    }

    // Make sure "lhs" is in normalised form:
    if (lhs.tv_usec > 1000000) {
        lhs.tv_usec -= 1000000;
        ++lhs.tv_sec;
    }

    return lhs;
}


/** Subtract one "struct timeval" from another.  Returns the difference in microseconds.  Does not check for overflow! */
inline long operator-(const timeval &lhs, const timeval &rhs) {
    const long lhs_usec(lhs.tv_sec * 1000000 + lhs.tv_usec);
    const long rhs_usec(rhs.tv_sec * 1000000 + rhs.tv_usec);

    return lhs_usec - rhs_usec;
}


/** Subtract a "struct timeval" from a "long" representing microseconds.  Returns the difference in milliseconds.
    Does not check for overflow! */
inline long operator-(const long &lhs_usec, const timeval &rhs) {
    const long rhs_usec(rhs.tv_sec * 1000000 + rhs.tv_usec);
    return lhs_usec - rhs_usec;
}


/** Subtract a "long" representing microseconds from a "struct timeval".  Returns the difference in microseconds.
    Does not check for overflow! */
inline long operator-(const timeval &lhs, const long &rhs_usec) {
    const long lhs_usec(lhs.tv_sec * 1000000 + lhs.tv_usec);
    return lhs_usec - rhs_usec;
}


/** \brief  Wrapper class that implements interval timers in an exception-safe manner.
 *  \note   The constructor of this class calls the timer's start() member function
 *          and the destructor calls the timer's stop() member function.  So timing
 *          will be limited to the scope of an object of this class.  Furthermore
 *          should any exceptions be thrown during the lifetime of an object of this
 *          class the timer's stop() member function will always be invoked (hurray)!
 *  \note   If you use this class you should preferentially use its "stop()" and "restart()"
 *          member functions rather than the managed timer class' "start()" and "stop()"
 *          member functions!
 */
template<class SomeTimer> class TimerStartStopper {
    SomeTimer &some_timer_;
    bool is_stopped_;
public:
    explicit TimerStartStopper(SomeTimer * const some_timer)
        : some_timer_(*some_timer), is_stopped_(false)
    { some_timer_.start(); }

    /** Use this in tandem with the "restart()" member function. */
    void stop() { some_timer_.stop(); is_stopped_ = true; }

    /** Use this in tandem with the "stop()" member function. */
    void restart() { some_timer_.start(); is_stopped_ = false; }

    ~TimerStartStopper()
    { if (not is_stopped_) some_timer_.stop(); }
private:
    TimerStartStopper(const TimerStartStopper &rhs) = delete;
    const TimerStartStopper &operator=(const TimerStartStopper &rhs) = delete;
};


namespace TimerUtil {


/** \brief  Helper class that saves (in the constructor) and restores (in the destructor)
 *          any pending real time itimer and signal handlers associated with SIGALRM.
 */
class SaveAndRestorePendingTimer {
    itimerval saved_itimerval_;      // Saved settings of a potentially active itimer.
    timeval start_time_;             // Systime at object construction.
    struct sigaction old_sigaction_; // The old options and signal handler for SIGALARM.
public:
    SaveAndRestorePendingTimer();
    ~SaveAndRestorePendingTimer();

    /** Returns the remaining time in microseconds on a pending timer if any or ULONG_MAX. */
    unsigned long getRemainingTimeOnPendingTimer() const;
private:
    SaveAndRestorePendingTimer(const SaveAndRestorePendingTimer &rhs)= delete;
    const SaveAndRestorePendingTimer operator=(const SaveAndRestorePendingTimer &rhs) = delete;
};


/** \brief  Subtracts "start_time" from "end_time" and returns the difference in milliseconds.
 *  \param  start_time  Some inital time.
 *  \param  end_time    Some final time.
 *  \note   If the difference would be negative, zero will be returned instead, i.e. the returned difference is *always*
 *          non-negative!
 *  \return The non-negative difference between "start_time" and "end_time".
 */
unsigned LeftOverTime(const struct timeval &start_time, const struct timeval &end_time);


/** \brief  Subtracts the difference between "start_time" and "end_time" form "*timeout".
 *  \param  start_time  Some inital time.
 *  \param  end_time    Some final time.
 *  \param  timeout     From this we'll substact "end_time" - "start_time" (but see the note).
 *  \note   If the difference between "start_time" and "end_time" would be negative, nothing will be subtracted from
 *          "*timeout"!
 */
void SubtractLeftOverTime(const struct timeval &start_time, const struct timeval &end_time,
                          unsigned * const timeout);


/** \brief  Millisecond resolution alarm function.
 *  \param  milliseconds  Timeout in milliseconds.
 *  \note   If "milliseconds" is 0, no new timer will be started and a potentially already running timer will be
 *          cancelled.  Caution: this function interacts and interferes with the standard Linux alarm(2) function
 *          in that both use the ITIMER_REAL itimer.
 *  \return The remaining time on an already existing timer in milliseconds, or -1 on error.
 */
unsigned malarm(const unsigned milliseconds);


uint64_t MillisecondsSinceEpoch();


} // namespace TimerUtil


#endif // ifndef TIMER_UTIL_H
