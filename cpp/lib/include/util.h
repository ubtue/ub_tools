/** \file   util.h
 *  \brief  Various utility functions that did not seem to logically fit anywhere else.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014,2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#ifndef UTIL_H
#define UTIL_H


#include <mutex>
#include <string>
#include <vector>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "Compiler.h"


// Macros to create strings describing where and why an error occurred. Must be macros to access __FILE__ and __LINE__.
// This gobble-dee-goop necessary to turn __LINE__ into a string. See doctor dobs: http://www.ddj.com/dept/cpp/184403864
//
#define Stringize(S) ReallyStringize(S)
#define ReallyStringize(S) #S


// A thread-safe logger class.
class Logger {
    friend Logger *LoggerInstantiator();
    std::mutex mutex_;
    int fd_;
private:
    Logger(): fd_(STDERR_FILENO) { }
public:
    void redirectOutput(const int new_fd) { fd_ = new_fd; }

    /** Emits "msg" and then calls exit(3). */
    void error(const std::string &msg) __attribute__((noreturn));
    inline void error(const std::string &function_name, const std::string &msg) { error("in " + function_name + ": " + msg); }

    void warning(const std::string &msg);
    inline void warning(const std::string &function_name, const std::string &msg) { warning("in " + function_name + ": " + msg); }

    void info(const std::string &msg);
    inline void info(const std::string &function_name, const std::string &msg) { info("in " + function_name + ": " + msg); }

    /** \note Only writes actual log messages if the environment variable "UTIL_LOG_DEBUG" exists and is set
     *  to "true"!
     */
    void debug(const std::string &msg);
    inline void debug(const std::string &function_name, const std::string &msg) { debug("in " + function_name + ": " + msg); }
};
extern Logger *logger;


#define ERROR(message)   logger->error(__func__, message)
#define WARNING(message) logger->warning(__func__, message)
#define INFO(message)    logger->info(__func__, message)
#define DEBUG(message)   logger->debug(__func__, message)


// TestAndThrowOrReturn -- tests condition "cond" and, if it evaluates to "true", throws an exception unless another
//                         exception is already in progress.  In the latter case, TestAndThrowOrReturn() simply
//                         returns.
//
#define TestAndThrowOrReturn(cond, err_text)                                                                       \
    do {                                                                                                           \
        if (unlikely(cond)) {                                                                                      \
            if (unlikely(std::uncaught_exception()))                                                               \
                return;                                                                                            \
            else                                                                                                   \
                throw std::runtime_error(std::string("in ") + __PRETTY_FUNCTION__ + "(" __FILE__ ":"               \
                                         Stringize(__LINE__) "): " + std::string(err_text)                         \
                                         + std::string(errno != 0 ? " (" + std::string(std::strerror(errno)) + ")" \
                                                                         : std::string("")));                      \
            }                                                                                                      \
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
    explicit DSVReader(const std::string &filename, const char field_separator=',', const char field_delimiter='"');
    ~DSVReader();
    bool readLine(std::vector<std::string> * const values);
};


#endif // ifndef UTIL_H
