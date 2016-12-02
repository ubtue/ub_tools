/** \file    Logger.cc
 *  \brief   Implementation of class Logger.
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

#include "Logger.h"
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include "File.h"
#include "FileUtil.h"
#include "TimeUtil.h"
#include "util.h"


const size_t MAX_BUF_SIZE(2048);


Logger::Logger(const std::string &log_filename, const VerbosityLevel default_verbosity, const OpenMode open_mode)
    : destroy_file_(true), already_dead_(false), log_file_(nullptr), log_filename_(log_filename), verbosity_(default_verbosity)
{
    // Make sure the logging directory exists:
    std::string dirname, basename;
    FileUtil::DirnameAndBasename(log_filename, &dirname, &basename);
    if (not dirname.empty() and unlikely(not FileUtil::MakeDirectory(dirname, /* recursive = */ true)))
        throw std::runtime_error("in Logger::Logger: can't create directory \"" + dirname + "\"!");

    log_file_ = new File(log_filename_, (open_mode == CLEAR ? "w" : "a"));
    if (log_file_->fail())
        Error("in Logger::Logger: can't open \"" + log_filename_ + "\" for logging!");
}


Logger::Logger(const char * const log_filename, const VerbosityLevel default_verbosity, const OpenMode open_mode)
        : destroy_file_(true), already_dead_(false), log_file_(nullptr), log_filename_(log_filename), verbosity_(default_verbosity)
{
    // Make sure the logging directory exists:
    std::string dirname, basename;
    FileUtil::DirnameAndBasename(log_filename, &dirname, &basename);
    if (not dirname.empty() and unlikely(not FileUtil::MakeDirectory(dirname, /* recursive = */ true)))
        throw std::runtime_error("in Logger::Logger: can't create directory \"" + dirname + "\"!");

    log_file_ = new File(log_filename_, (open_mode == CLEAR ? "w" : "a"));
    if (log_file_->fail())
        Error("in Logger::Logger: can't open \"" + log_filename_ + "\" for logging!");
}


Logger::Logger(File * const log_file, const VerbosityLevel default_verbosity, const OpenMode open_mode)
        : destroy_file_(false), already_dead_(false), log_file_(log_file), log_filename_(""), verbosity_(default_verbosity)
{
    if (log_file_->fail())
        Error("in Logger::Logger: logger file not working!");

    if (open_mode == CLEAR)
        clear();
}


void Logger::clear() {
    log_file_->truncate();
}


Logger::~Logger() { if (destroy_file_) delete log_file_; }


void Logger::reopen(const std::string &log_filename) {
    if (destroy_file_)
        delete log_file_;

    if (log_filename.empty() and log_filename_.empty())
        Error("in Loger::reopen: no log filename available!");

    if (not log_filename.empty())
        log_filename_ = log_filename;

    // Make sure the logging directory exists:
    std::string dirname, basename;
    FileUtil::DirnameAndBasename(log_filename, &dirname, &basename);
    if (not dirname.empty() and unlikely(not FileUtil::MakeDirectory(dirname, /* recursive = */ true)))
        throw std::runtime_error("in Logger::reopen: can't create directory \"" + dirname + "\"!");

    log_file_ = new File(log_filename_.c_str(), "a");
    if (log_file_->fail())
        throw std::runtime_error("can't open \"" + log_filename_ + "\" for logging!");
}


void Logger::log(const VerbosityLevel min_verbosity_level, const char *fmt, ...) {
    if (verbosity_ < min_verbosity_level)
        return;

    char msg_buffer[MAX_BUF_SIZE];

    va_list args;
    va_start(args, fmt);
    ::vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);

    log(std::string(msg_buffer));
}


void Logger::log(const char *fmt, ...) {
    char msg_buffer[MAX_BUF_SIZE];

    va_list args;
    va_start(args, fmt);
    ::vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);

    log(std::string(msg_buffer));
}


void Logger::log(const std::string &message) {
    writeLog(message, NON_EXITING);
    if (log_file_->fail())
        throw std::runtime_error("in Logger::log: failed to write to the log file \"" + log_filename_ + "\"!");
}


void Logger::sysLog(const char *fmt, ...) {
    char msg_buffer[MAX_BUF_SIZE];

    va_list args;
    va_start(args, fmt);
    ::vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);

    sysLog(std::string(msg_buffer));
}


void Logger::sysLog(const std::string &message) {
    writeLog(message, NON_EXITING | HANDLE_ERRNO);
    if (log_file_->fail())
        throw std::runtime_error("in Logger::syslog: failed to write to the log file \"" + log_filename_ + "\"!");
}


void Logger::logAndDie(const char *fmt, ...) {
    if (already_dead_)
        std::exit(EXIT_FAILURE);

    char msg_buffer[MAX_BUF_SIZE];

    va_list args;
    va_start(args, fmt);
    ::vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);

    logAndDie(std::string(msg_buffer));
}


void Logger::logAndThrow(const std::string &message) {
    if (already_dead_)
        std::exit(EXIT_FAILURE);

    writeLog(message, EXITING);
    if (log_file_->fail()) {
        already_dead_ = true;
        throw std::runtime_error("in Logger::logAndThrow: failed to write to the log file \"" + log_filename_ + "\"!");
    }

    throw std::runtime_error(message);
}


void Logger::logAndThrow(const char *fmt, ...)
{
        if (already_dead_)
                std::exit(EXIT_FAILURE);

        char msg_buffer[MAX_BUF_SIZE];

        va_list args;
        va_start(args, fmt);
        ::vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
        va_end(args);

        logAndThrow(std::string(msg_buffer));
}


void Logger::logAndDie(const std::string &message) {
    if (already_dead_)
        std::exit(EXIT_FAILURE);

    writeLog(message, EXITING);
    if (log_file_->fail()) {
        already_dead_ = true;
        throw std::runtime_error("in Logger::logAndDie: failed to write to the log file \"" + log_filename_ + "\"!");
    }

    std::exit(EXIT_FAILURE);
}


void Logger::sysLogAndDie(const char *fmt, ...) {
    if (already_dead_)
        std::exit(EXIT_FAILURE);

    char msg_buffer[MAX_BUF_SIZE];

    va_list args;
    va_start(args, fmt);
    ::vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);

    sysLogAndDie(std::string(msg_buffer));
}


void Logger::sysLogAndDie(const std::string &message) {
    if (already_dead_)
        std::exit(EXIT_FAILURE);

    writeLog(message, EXITING | HANDLE_ERRNO);
    if (log_file_->fail()) {
        already_dead_ = true;
        throw std::runtime_error("in Logger::sysLogAndDie: failed to write to the log file \"" + log_filename_ + "\"!");
    }

    std::exit(EXIT_FAILURE);
}


void Logger::writeLog(const std::string &message, const unsigned log_mask) {
    *log_file_ << TimeUtil::GetCurrentDateAndTime() << " [" << ::progname << ", PID:" << ::getpid();
    *log_file_ << "]: ";
    if (log_mask & EXITING)
        *log_file_ << "Exiting: ";
    *log_file_ << message_prefix_ << message;
    if (log_mask & HANDLE_ERRNO)
        *log_file_ << " (" << std::to_string(errno) << ')';
    *log_file_ << File::endl;
}
