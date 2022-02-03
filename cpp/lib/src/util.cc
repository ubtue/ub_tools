/** \file    util.cc
 *  \brief   Implementation of various utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015-2020 Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "util.h"
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <execinfo.h>
#include <signal.h>
#include "Compiler.h"
#include "FileLocker.h"
#include "FileUtil.h"
#include "MiscUtil.h"
#include "SignalUtil.h"
#include "StringUtil.h"
#include "TimeUtil.h"


static std::string log_path;
static int log_fd;


void HUPHandler(int /* signal_number*/) {
    ::close(log_fd);
    const int new_fd(::open(log_path.c_str(), O_WRONLY | O_APPEND));
    if (new_fd == -1) { // We're screwed!
        const std::string error_message("in HUPHandler(util.cc): can't open(2) \"" + log_path + "\"!\n");
        ::write(STDERR_FILENO, error_message.c_str(), error_message.size());
        ::exit(EXIT_FAILURE);
    }

    // Make sure we're using the same file descriptor as before!
    if (new_fd != log_fd) {
        ::dup2(new_fd, log_fd);
        ::close(new_fd);
    }
}


// Macro to determine the number of entries in a one-dimensional array:
#define DIM(array) (sizeof(array) / sizeof(array[0]))


char *progname; // Must be set in main() with "progname = argv[0];";


const std::string Logger::FUNCTION_NAME_SEPARATOR(" --> ");


Logger::Logger(): log_process_pids_(false), log_no_decorations_(false), log_strip_call_site_(false), min_log_level_(LL_INFO) {
    log_fd = STDERR_FILENO;
    log_path = FileUtil::GetPathFromFileDescriptor(log_fd);
    SignalUtil::InstallHandler(SIGHUP, HUPHandler);

    const char * const min_log_level(::getenv("MIN_LOG_LEVEL"));
    if (min_log_level != nullptr)
        min_log_level_ = Logger::StringToLogLevel(min_log_level);
    const char * const logger_format(::getenv("LOGGER_FORMAT"));
    if (logger_format != nullptr) {
        if (std::strstr(logger_format, "process_pids") != nullptr)
            log_process_pids_ = true;
        if (std::strstr(logger_format, "no_decorations") != nullptr)
            log_no_decorations_ = true;
        if (std::strstr(logger_format, "strip_call_site") != nullptr)
            log_strip_call_site_ = true;
    }
}


void Logger::redirectOutput(const int new_fd) {
    log_fd = new_fd;
    log_path = FileUtil::GetPathFromFileDescriptor(new_fd);
}


void Logger::error(const std::string &msg) {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    std::string error_message_string;
    if (errno != 0)
        error_message_string = " (last errno error code: " + std::string(std::strerror(errno)) + ")";

    writeString("SEVERE", msg + error_message_string);
    if (::getenv("BACKTRACE") != nullptr) {
        const bool saved_log_no_decorations(log_no_decorations_);
        log_no_decorations_ = true;
        writeString("", "Backtrace:");
        for (const auto &stack_entry : MiscUtil::GetCallStack())
            writeString("", "  " + stack_entry);
        log_no_decorations_ = saved_log_no_decorations;
    }

    std::exit(EXIT_FAILURE);
}


void Logger::warning(const std::string &msg) {
    if (min_log_level_ < LL_WARNING)
        return;

    std::lock_guard<std::mutex> mutex_locker(mutex_);
    writeString("WARN", msg);
}


void Logger::info(const std::string &msg) {
    if (min_log_level_ < LL_INFO)
        return;

    std::lock_guard<std::mutex> mutex_locker(mutex_);
    writeString("INFO", msg);
}


void Logger::debug(const std::string &msg) {
    if ((min_log_level_ < LL_DEBUG) and (MiscUtil::SafeGetEnv("UTIL_LOG_DEBUG") != "true"))
        return;

    std::lock_guard<std::mutex> mutex_locker(mutex_);
    writeString("DEBUG", msg);
}


inline Logger *LoggerInstantiator() {
    return new Logger();
}


Logger *logger(LoggerInstantiator());


Logger::LogLevel Logger::StringToLogLevel(const std::string &level_candidate) {
    if (level_candidate == "ERROR")
        return Logger::LL_ERROR;
    if (level_candidate == "WARNING")
        return Logger::LL_WARNING;
    if (level_candidate == "INFO")
        return Logger::LL_INFO;
    if (level_candidate == "DEBUG")
        return Logger::LL_DEBUG;
    LOG_ERROR("not a valid minimum log level: \"" + level_candidate + "\"! (Use ERROR, WARNING, INFO or DEBUG)");
}


std::string Logger::LogLevelToString(const LogLevel log_level) {
    if (log_level == Logger::LL_ERROR)
        return "ERROR";
    if (log_level == Logger::LL_WARNING)
        return "WARNING";
    if (log_level == Logger::LL_INFO)
        return "INFO";
    if (log_level == Logger::LL_DEBUG)
        return "DEBUG";
    LOG_ERROR("unsupported log level, we should *never* get here!");
}


void Logger::formatMessage(const std::string &level, std::string * const msg) {
    if (not log_no_decorations_) {
        *msg = TimeUtil::GetCurrentDateAndTime(TimeUtil::ISO_8601_FORMAT) + " " + level + " " + std::string(::program_invocation_name)
               + ": " + *msg;
        if (log_process_pids_)
            *msg += " (PID: " + std::to_string(::getpid()) + ")";
    }

    if (log_strip_call_site_) {
        const auto END_OF_CALL_SITE_PREFIX(msg->find(FUNCTION_NAME_SEPARATOR));
        if (END_OF_CALL_SITE_PREFIX != std::string::npos)
            *msg = msg->substr(END_OF_CALL_SITE_PREFIX + FUNCTION_NAME_SEPARATOR.length());
    }

    *msg += '\n';
}


void Logger::writeString(const std::string &level, std::string msg, const bool format_message) {
    if (format_message)
        formatMessage(level, &msg);

    FileLocker file_locker(log_fd, FileLocker::READ_WRITE, 20 /* seconds */);
    SignalUtil::SignalBlocker signal_blocker(SIGHUP);
    if (unlikely(::write(log_fd, reinterpret_cast<const void *>(msg.data()), msg.size()) == -1)) {
        const std::string error_message("in Logger::writeString(util.cc): write to file descriptor " + std::to_string(log_fd)
                                        + " failed! (errno = " + std::to_string(errno) + ")");
#pragma GCC diagnostic ignored "-Wunused-result"
        ::write(STDERR_FILENO, error_message.data(), error_message.size());
#pragma GCC diagnostic warning "-Wunused-result"
        _exit(EXIT_FAILURE);
    }
}


int Logger::getFileDescriptor() const {
    return log_fd;
}


DSVReader::DSVReader(const std::string &filename, const char field_separator, const char field_delimiter)
    : field_separator_(field_separator), field_delimiter_(field_delimiter), line_no_(0), filename_(filename) {
    input_ = std::fopen(filename.c_str(), "rm");
    if (input_ == nullptr)
        throw std::runtime_error("in DSVReader::DSVReader: can't open \"" + filename + "\" for reading!");
}


DSVReader::~DSVReader() {
    if (input_ != nullptr)
        std::fclose(input_);
}


namespace {


void SkipFieldPadding(FILE * const input) {
    int ch = std::fgetc(input);
    while (isblank(ch))
        ch = std::fgetc(input);
    std::ungetc(ch, input);
}


/** \brief Remove trailing spaces and tabs from "s". */
std::string TrimBlanks(std::string *s) {
    std::string::const_reverse_iterator it(s->crbegin());
    for (/* Empty! */; it != s->crend() and std::isblank(*it); ++it)
        /* Intentionally Empty! */;
    if (it != s->crbegin())
        *s = s->substr(0, std::distance(it, s->crend()));

    return *s;
}


std::string ReadQuotedValue(FILE * const input, const char field_delimiter, const char field_separator) {
    std::string value;
    bool delimiter_seen(false);
    for (;;) {
        const int ch(std::fgetc(input));
        if (ch == EOF)
            throw std::runtime_error("unexpected EOF while reading a quoted DSV value!");
        if (ch == field_delimiter) {
            if (delimiter_seen) {
                // ignore if it's not the outermost delimiter
                const int next(std::fgetc(input));
                std::ungetc(next, input);

                if (next == field_separator or next == '\n' or next == EOF)
                    return value;
            } else
                delimiter_seen = true;
        } else {
            if (ch == '\n' or ch == EOF) {
                std::ungetc(ch, input);
                return TrimBlanks(&value);
            }
            value += static_cast<char>(ch);
        }
    }
}


std::string ReadNonQuotedValue(FILE * const input, const char field_separator) {
    std::string value;
    for (;;) {
        const int ch(std::fgetc(input));
        if (ch == EOF or ch == '\n' or ch == field_separator) {
            std::ungetc(ch, input);
            return TrimBlanks(&value);
        }
        value += static_cast<char>(ch);
    }
}


void BacktraceSignalHandler(int signal_no) {
    void *stack_return_addresses[20];
    const size_t number_of_addresses(::backtrace(stack_return_addresses, DIM(stack_return_addresses)));
    char err_msg[1024] = "Caught signal ";
    char *cp = err_msg + std::strlen(err_msg);
    if (signal_no > 10)
        *cp++ = '0' + (signal_no / 10);
    *cp++ = '0' + (signal_no % 10);
    *cp++ = '.';
    *cp++ = '\n';
    *cp = '\0';
    ssize_t unused(::write(STDERR_FILENO, err_msg, std::strlen(err_msg)));
    (void)unused;
    ::backtrace_symbols_fd(stack_return_addresses, number_of_addresses, STDERR_FILENO);
    ::_exit(EXIT_FAILURE);
}


int InstallSegvSignalHandler(void handler(int)) {
    ::signal(SIGSEGV, handler);
    return 0;
}


volatile int dummy = InstallSegvSignalHandler(BacktraceSignalHandler);


} // unnamed namespace


bool DSVReader::readLine(std::vector<std::string> * const values) {
    values->clear();
    ++line_no_;

    int ch;
    for (;;) {
        if (not values->empty()) {
            SkipFieldPadding(input_);
            ch = std::fgetc(input_);
            if (ch == EOF)
                return not values->empty();
            if (ch == '\n')
                return true;
            if (ch != field_separator_)
                throw std::runtime_error("in DSVReader::readLine: on line " + std::to_string(line_no_)
                                         + ": field separator expected, found '" + std::string(1, static_cast<char>(ch)) + "' instead!");
        }

        SkipFieldPadding(input_);
        ch = std::fgetc(input_);
        if (ch == '\n')
            return true;
        if (ch == EOF)
            return not values->empty();
        if (ch == field_separator_) {
            std::ungetc(ch, input_);
            values->emplace_back("");
        } else if (ch == field_delimiter_) {
            std::ungetc(ch, input_);
            values->emplace_back(ReadQuotedValue(input_, field_delimiter_, field_separator_));
        } else {
            std::ungetc(ch, input_);
            values->emplace_back(ReadNonQuotedValue(input_, field_separator_));
        }
    }
}


[[noreturn]] void Usage(const std::string &usage_message) {
    std::vector<std::string> lines;
    StringUtil::SplitThenTrimWhite(usage_message, '\n', &lines, /* suppress_empty_words */ false);
    auto line(lines.begin());
    if (unlikely(line == lines.cend()))
        LOG_ERROR("missing usage message!");

    std::cerr << "Usage: " << ::program_invocation_name << " [--min-log-level=(ERROR|WARNING|INFO|DEBUG)] " << *line << '\n';
    const std::string padding(__builtin_strlen("Usage: ") + __builtin_strlen(::program_invocation_name) + 1, ' ');
    for (++line; line != lines.cend(); ++line)
        std::cerr << padding << *line << '\n';

    std::exit(EXIT_FAILURE);
}
