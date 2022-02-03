/** \file   util.h
 *  \brief  Various utility functions that did not seem to logically fit anywhere else.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014-2020 Universitätsbibliothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once


#include <mutex>
#include <string>
#include <vector>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "Compiler.h"


// Macros to create strings describing where and why an error occurred. Must be macros to access __FILE__ and __LINE__.
// This gobble-dee-goop is necessary to turn __LINE__ into a string. See https://gcc.gnu.org/onlinedocs/cpp/Stringizing.html
//
#define Stringize(S) ReallyStringize(S)
#define ReallyStringize(S) #S


/** A thread-safe logger class.
 * \note Set the environment variable LOGGER_FORMAT to control the output format of our logger.  So far we support
 *       "process_pids", "strip_call_site" and "no_decorations".  You may combine any of these, e.g. by separating them with
 *       commas.
 */
class Logger {
public:
    enum LogLevel { LL_ERROR = 1, LL_WARNING = 2, LL_INFO = 3, LL_DEBUG = 4 };
    friend Logger *LoggerInstantiator();

protected:
    static const std::string FUNCTION_NAME_SEPARATOR;

    std::mutex mutex_;
    bool log_process_pids_, log_no_decorations_, log_strip_call_site_;
    LogLevel min_log_level_;

    void formatMessage(const std::string &level, std::string * const msg);

public:
    Logger();
    virtual ~Logger() = default;

public:
    void redirectOutput(const int new_fd);

    bool getLogNoDecorations() { return log_no_decorations_; }
    void setLogNoDecorations(const bool log_no_decorations) { log_no_decorations_ = log_no_decorations; }

    bool getLogStripCallSite() { return log_strip_call_site_; }
    void setLogStripCallSite(const bool log_strip_call_site) { log_strip_call_site_ = log_strip_call_site; }

    void setMinimumLogLevel(const LogLevel min_log_level) { min_log_level_ = min_log_level; }
    LogLevel getMinimumLogLevel() const { return min_log_level_; }

    //* Emits "msg" and then calls exit(3), also generates a call stack trace if the environment variable BACKTRACE has been set.
    [[noreturn]] virtual void error(const std::string &msg) __attribute__((noreturn));
    [[noreturn]] virtual void error(const std::string &function_name, const std::string &msg) __attribute__((noreturn)) {
        error("in " + function_name + FUNCTION_NAME_SEPARATOR + msg);
        __builtin_unreachable();
    }

    virtual void warning(const std::string &msg);
    inline void warning(const std::string &function_name, const std::string &msg) {
        warning("in " + function_name + FUNCTION_NAME_SEPARATOR + msg);
    }

    virtual void info(const std::string &msg);
    inline void info(const std::string &function_name, const std::string &msg) {
        info("in " + function_name + FUNCTION_NAME_SEPARATOR + msg);
    }

    /** \note Only writes actual log messages if the environment variable "UTIL_LOG_DEBUG" exists and is set
     *  to "true"!
     */
    virtual void debug(const std::string &msg);
    inline void debug(const std::string &function_name, const std::string &msg) {
        debug("in " + function_name + FUNCTION_NAME_SEPARATOR + msg);
    }

    //* \note Aborts if ""level_candidate" is not one of "ERROR", "WARNING", "INFO" or "DEBUG".
    static LogLevel StringToLogLevel(const std::string &level_candidate);

    // \brief Returns a string representation of "log_level".
    static std::string LogLevelToString(const LogLevel log_level);

protected:
    virtual void writeString(const std::string &level, std::string msg, const bool format_message = true);
    int getFileDescriptor() const;
};
extern Logger *logger;


#define LOG_ERROR(message) logger->error(__PRETTY_FUNCTION__, message), __builtin_unreachable()
#define LOG_WARNING(message) logger->warning(__PRETTY_FUNCTION__, message)
#define LOG_INFO(message) logger->info(__PRETTY_FUNCTION__, message)
#define LOG_DEBUG(message) logger->debug(__PRETTY_FUNCTION__, message)


// TestAndThrowOrReturn -- tests condition "cond" and, if it evaluates to "true", throws an exception unless another
//                         exception is already in progress.  In the latter case, TestAndThrowOrReturn() simply
//                         returns.
//
#define TestAndThrowOrReturn(cond, err_text)                                                                                            \
    do {                                                                                                                                \
        if (unlikely(cond)) {                                                                                                           \
            if (unlikely(std::uncaught_exception()))                                                                                    \
                return;                                                                                                                 \
            else                                                                                                                        \
                throw std::runtime_error(std::string("in ") + __PRETTY_FUNCTION__                                                       \
                                         + "(" __FILE__ ":" Stringize(__LINE__) "): " + std::string(err_text)                           \
                                         + std::string(errno != 0 ? " (" + std::string(std::strerror(errno)) + ")" : std::string(""))); \
        }                                                                                                                               \
    } while (false)


/** Must be set to point to argv[0] in main(). */
extern char *progname;


/** \class DSVReader
 *  \brief A "reader" for delimiter-separated values.
 */
class DSVReader {
    char field_separator_;
    char field_delimiter_;
    unsigned line_no_;
    std::string filename_;
    FILE *input_;

public:
    explicit DSVReader(const std::string &filename, const char field_separator = ',', const char field_delimiter = '"');
    ~DSVReader();
    bool readLine(std::vector<std::string> * const values);
};


template <typename T>
std::string ArrayToString(T *array, size_t count) {
    std::string buffer("[");
    for (size_t i(0); i < count; ++i)
        buffer.append(std::to_string(array[i])).append(",");
    buffer.append("]");
    return buffer;
}


// \note A single newline will be appended to the message that is emitted on stderr.  Furthermore, "[--min-log-level] " will be prepended.
[[noreturn]] void Usage(const std::string &usage_message);
