/** \file    Logger.h
 *  \brief   Declaration of class Logger.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
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

#ifndef LOGGER_H
#define LOGGER_H


#include <string>


class File;


/** \class Logger
 *  \brief A class for logging timestamped messages.
 *
 *  \note  The logging functions starting with "sys" additionally report system
 *         errors as indicated by a non-zero setting of the global "errno" variable.
 */
class Logger {
protected:
    const bool destroy_file_; // If true we delete "log_file_" in the destructor, otherwise we don't.
    bool already_dead_; // To prevent reentrancy problems in "logAndDie".
    File *log_file_;
    std::string log_filename_, message_prefix_;
public:
    enum OpenMode { DO_NOT_CLEAR, CLEAR };
    enum VerbosityLevel { VL_ERRORS_ONLY = 1, VL_WARNINGS_AND_ERRORS, VL_NORMAL, VL_CHATTY, VL_DEBUG };
private:
    VerbosityLevel verbosity_;
public:
    /** \brief  Creates a new Logger object that writes to a log file.
     *  \param  log_filename       The log file name.
     *  \param  default_verbosity  Min. required verbosity for logging to occur.
     *  \param  open_mode          Whether to clear the log file upon opening it or not.
     */
    explicit Logger(const std::string &log_filename, const VerbosityLevel default_verbosity = VL_NORMAL,
                    const OpenMode open_mode = DO_NOT_CLEAR);

    /** \brief  Creates a new Logger object that writes to a log file.
     *  \param  log_filename       The log file name.
     *  \param  default_verbosity  Min. required verbosity for logging to occur.
     *  \param  open_mode          Whether to clear the log file upon opening it or not.
     */
    explicit Logger(const char * const log_filename, const VerbosityLevel default_verbosity = VL_NORMAL,
                    const OpenMode open_mode = DO_NOT_CLEAR);

    /** \brief  Creates a new Logger object that writes to a File.
     *  \param  log_file           The File to write to.
     *  \param  default_verbosity  Min. required verbosity for logging to occur.
     *  \param  open_mode          Whether to clear the log file upon opening it or not.
     */
    explicit Logger(File * const log_file, const VerbosityLevel default_verbosity = VL_NORMAL,
                    const OpenMode open_mode = DO_NOT_CLEAR);

    virtual ~Logger();

    /** Remove all log file contents. */
    void clear();

    /** Get the log file name currently in use. */
    std::string getFileName() const { return log_filename_; }

    /** \brief  Reopens the output file used for logging.
     *  \param log_filename  File to write logging messages to.  If unspecified here and a filename was provided by
     *                       the constructor it will be reused.
     */
    void reopen(const std::string &log_filename = "");

    void setVerbosityLevel(const VerbosityLevel new_verbosity_level) { verbosity_ = new_verbosity_level; }
    VerbosityLevel getVerbosityLevel() const { return verbosity_; }

    void setMessagePrefix(const std::string &new_message_prefix) { message_prefix_ = new_message_prefix; }
    std::string getMessagePrefix() const { return message_prefix_; }

    void log(const VerbosityLevel min_verbosity_level, const char *fmt, ...) __attribute__((__format__(printf,3,4)));

    void log(const VerbosityLevel min_verbosity_level, const std::string &message) {
        if (verbosity_ >= min_verbosity_level)
            log(message);
    }

    /** Record a message in the log. */
    void log(const char *fmt, ...) __attribute__((__format__(printf,2,3)));

    virtual void log(const std::string &message);

    /** Record a message in the log, including any information from errno. */
    void sysLog(const char *fmt, ...) __attribute__((__format__(printf,2,3)));
    void sysLog(const std::string &message);

    /** Record a message in the log and then throw an exception. */
    void logAndThrow(const char *fmt, ...) __attribute__((__format__(printf,2,3)));
    void logAndThrow(const std::string &message);

    /** Record a message in the log and then exit the program. */
    void logAndDie(const char *fmt, ...) __attribute__((__format__(printf,2,3)));
    void logAndDie(const std::string &message);

    /** Record a message in the log, including any information from errno, then exit the program. */
    void sysLogAndDie(const char *fmt, ...) __attribute__((__format__(printf,2,3)));
    void sysLogAndDie(const std::string &message);

protected:
    explicit Logger(const bool destroy_file)
        : destroy_file_(destroy_file), already_dead_(true), log_file_(nullptr) { }
private:
    Logger() = delete;
    Logger(const Logger &rhs) = delete;
    const Logger &operator=(const Logger &rhs) = delete;
protected:
    enum LogType { EXITING = 1, NON_EXITING = 2, HANDLE_ERRNO = 4 };
    virtual void writeLog(const std::string &message, const unsigned log_mask);
};


#endif // ifndef LOGGER_H
