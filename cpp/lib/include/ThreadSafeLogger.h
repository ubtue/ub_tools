/** \file    ThreadSafeLogger.h
 *  \brief   Declaration of class ThreadSafeLogger.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2006-2007 Project iVia.
 *  Copyright 2006-2007 The Regents of The University of California.
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

#ifndef THREAD_SAFE_LOGGER_H
#define THREAD_SAFE_LOGGER_H


#include "Logger.h"
#include <mutex>


/** \class ThreadSafeLogger
 *  \brief A class for logging timestamped messages that is thread-safe.
 */
class ThreadSafeLogger: public Logger {
    std::mutex mutex_;
public:
    /** \brief  Creates a new ThreadSafeLogger object that writes to a log file.
     *  \param  log_filename  The log file name.
     *  \param  open_mode     Whether to clear the log file upon opening it or not.
     */
    explicit ThreadSafeLogger(const std::string &log_filename,
                              const Logger::VerbosityLevel default_verbosity = Logger::VL_NORMAL,
                              const Logger::OpenMode open_mode = Logger::DO_NOT_CLEAR)
        : Logger(log_filename, default_verbosity, open_mode) { }

    /** \brief  Creates a new ThreadSafeLogger object that writes to a log file.
     *  \param  log_filename  The log file name.
     *  \param  open_mode     Whether to clear the log file upon opening it or not.
     */
    explicit ThreadSafeLogger(const char * const log_filename, const Logger::VerbosityLevel default_verbosity = Logger::VL_NORMAL,
                              const Logger::OpenMode open_mode = Logger::DO_NOT_CLEAR)
        : Logger(log_filename, default_verbosity, open_mode) { }

    /** \brief  Creates a new ThreadSafeLogger object that writes to a File.
     *  \param  log_file   The File to write to.
     *  \param  open_mode  Whether to clear the log file upon opening it or not.
     */
    explicit ThreadSafeLogger(File * const log_file, const Logger::VerbosityLevel default_verbosity = Logger::VL_NORMAL,
                              const Logger::OpenMode open_mode = Logger::DO_NOT_CLEAR)
        : Logger(log_file, default_verbosity, open_mode) { }
protected:
    virtual void writeLog(const std::string &message, const unsigned log_mask);
};


#endif // ifndef THREAD_SAFE_LOGGER_H
