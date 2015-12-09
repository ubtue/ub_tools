/** \file    ThreadSafeLogger.cc
 *  \brief   Implementation of class ThreadSafeLogger.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2006 Project iVia.
 *  Copyright 2006 The Regents of The University of California.
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

#include "ThreadSafeLogger.h"


void ThreadSafeLogger::writeLog(const std::string &message, const unsigned log_mask) {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    Logger::writeLog(message, log_mask);
}
