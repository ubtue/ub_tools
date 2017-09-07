/** \file    TimeLimit.h
 *  \brief   Implements class TimeLimit.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Anthony Moralez
 */

/*
 *  \copyright 2005-2009 Project iVia.
 *  \copyright 2005-2009 The Regents of The University of California.
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

#ifndef TIME_LIMIT_H
#define TIME_LIMIT_H


#include <sys/time.h>
#include <time.h>


/** \class  TimeLimit
 *  \brief  Represents a time limit placed upon some operation.
 *
 *  The TimeLimit class is used to keep track of a time limit placed on some opertion. A time limit is specified (in
 *  milliseconds) when the object is created.  From that point on, the limitExceeded() function can be used to test
 *  whether the time limit has been reached, and the getRemainingTime() function to measure the time left before the
 *  limit has been reached.
 */
class TimeLimit {
    timeval expire_time_;
public:
 /** \brief  Construct a TimeLimit by specifying the limit.
  *  \param  time_limit  The time until expiration, in milliseconds.
  *  \note   This constructor is deliberately not explicit, so that unsigned values can be used in place of
  *          TimeLimit objects in function calls.
  */
    TimeLimit(const unsigned time_limit);

    /** TimeLimit Copy Constructor. */
    TimeLimit(const TimeLimit &rhs): expire_time_(rhs.expire_time_) { }

    /** TimeLimit assignment operator. */
    const TimeLimit operator=(const TimeLimit &rhs) { expire_time_ = rhs.expire_time_; return *this; }

    /** TimeLimit assignment operator. */
    const TimeLimit operator=(const unsigned new_time_limit);

    /** \brief   Test whether the time limit has been exceeded.
     *  \return  True if the time limit has been exceeded, otherwise false.
     */
    bool limitExceeded() const;

    /** \brief   Get the time remaining before the limit is exceeded.
     *  \return  The time remaining until the limit is exceeded (in milliseconds) or 0 if the limit is already
     *           exceeded.
     */
    unsigned getRemainingTime() const;

    /** Equality and inequality operators. */
    bool operator==(const TimeLimit &rhs);
    bool operator!=(const TimeLimit &rhs);
};


#endif // ifndef TIME_LIMIT_H
