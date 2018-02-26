/** \file    SystemAndUserTimer.cc
 *  \brief   Implementation of class SystemAndUserTimer.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2005 Project iVia.
 *  Copyright 2002-2005 The Regents of The University of California.
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

#include <SystemAndUserTimer.h>
#ifndef STDEXCEPT
#   include <stdexcept>
#   define STDEXCEPT
#endif
#ifndef CERRNO
#   include <cerrno>
#   define CERRNO
#endif
#ifndef CSTRING
#   include <cstring>
#   define CSTRING
#endif
#ifndef SYS_RESOURCE_H
#   include <sys/resource.h>
#   define SYS_RESOURCE_H
#endif
#ifndef COMPILER_H
#   include "Compiler.h"
#endif
#include "util.h"


SystemAndUserTimer::SystemAndUserTimer(const SystemAndUserTimerType timer_type, const std::string &name)
    : is_running_(false), user_time_(0.0), system_time_(0.0), name_(name), timer_type_(timer_type)
{
    timerclear(&user_time_start_);
    timerclear(&system_time_start_);
}


SystemAndUserTimer::~SystemAndUserTimer() {
    // Ignore all potential error conditions if we have an uncaught exception:
    if (std::uncaught_exception())
        return;

    if ((timer_type_ == CUMULATIVE_WITH_AUTO_STOP or timer_type_ == NON_CUMULATIVE_WITH_AUTO_STOP) and isRunning())
        stop();
    if (unlikely(is_running_)) {
        if (not name_.empty())
            logger->error("in SystemAndUserTimer::~SystemAndUserTimer: timer \"" + name_ + "\" is running!");
        else
            logger->error("in SystemAndUserTimer::~SystemAndUserTimer: timer is running!");
    }
}


void SystemAndUserTimer::start() {
    if (unlikely(is_running_)) {
        if (not name_.empty())
            throw std::runtime_error("in SystemAndUserTimer::start: \"" + name_
                                     + "\" is running!");
        else
            throw std::runtime_error("in SystemAndUserTimer::start: timer is running!");
    }

    struct rusage ru;
    if (::getrusage(RUSAGE_SELF, &ru) == -1)
        throw std::runtime_error("in SystemAndUserTimer::start: getrusage(2) failed ("
                                 + std::string(std::strerror(errno)) + ")!");

    user_time_start_   = ru.ru_utime;
    system_time_start_ = ru.ru_stime;
    is_running_ = true;
}


void SystemAndUserTimer::stop() {
    if (unlikely(not is_running_)) {
        if (not name_.empty())
            throw std::runtime_error("in SystemAndUserTimer::stop: timer \"" + name_ + "\" is not running!");
        else
            throw std::runtime_error("in SystemAndUserTimer::stop: timer is not running!");
    }

    struct rusage ru;
    if (::getrusage(RUSAGE_SELF, &ru) == -1)
        throw std::runtime_error("in SystemAndUserTimer::stop: getrusage(2) failed ("
                                 + std::string(std::strerror(errno)) + ")!");
    user_time_stop_   = ru.ru_utime;
    system_time_stop_ = ru.ru_stime;

    if (timer_type_ == NON_CUMULATIVE) {
        user_time_   = TimevalToDouble(user_time_stop_)   - TimevalToDouble(user_time_start_);
        system_time_ = TimevalToDouble(system_time_stop_) - TimevalToDouble(system_time_start_);
    } else { // Assume cumulative timing.
        user_time_   += TimevalToDouble(user_time_stop_)   - TimevalToDouble(user_time_start_);
        system_time_ += TimevalToDouble(system_time_stop_) - TimevalToDouble(system_time_start_);
    }

    is_running_ = false;
}


double SystemAndUserTimer::getUserTime() const {
    if (unlikely(is_running_)) {
        if (not name_.empty())
            throw std::runtime_error("in SystemAndUserTimer::getUserTime: \"" + name_ + "\" is running!");
        else
            throw std::runtime_error("in SystemAndUserTimer::getUserTime: timer is running!");
    }

    return user_time_;
}


double SystemAndUserTimer::getSystemTime() const {
    if (unlikely(is_running_)) {
        if (not name_.empty())
            throw std::runtime_error("in SystemAndUserTimer::getSystemTime: \"" + name_ + "\" is running!");
        else
            throw std::runtime_error("in SystemAndUserTimer::getSystemTime: timer is running!");
    }

    return system_time_;
}
