/** \file    ThreadUtil.cc
 *  \brief   Implementation of thread utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2003-2009 Project iVia.
 *  Copyright 2003-2009 The Regents of The University of California.
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

#include "ThreadUtil.h"
#include <stdexcept>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <sys/syscall.h>
#include <unistd.h>
#include "File.h"
#include "TimeUtil.h"
#include "util.h"


namespace ThreadUtil {


Semaphore::Semaphore(const unsigned initial_count)
        : type_(SINGLE_PROCESS)
{
    semaphore_ = new sem_t;
    if (::sem_init(semaphore_, 0, initial_count) != 0)
        throw std::runtime_error("in ThreadUtil::Semaphore::Semaphore: sem_init(3) failed (" + std::to_string(errno) + ") (1)!");
}


Semaphore::Semaphore(char * const shared_memory, const bool init, const unsigned initial_count)
    : semaphore_(reinterpret_cast<sem_t *>(shared_memory)), type_(MULTI_PROCESS)
{
    if (init and ::sem_init(semaphore_, 1, initial_count) != 0)
        throw std::runtime_error("in ThreadUtil::Semaphore::Semaphore: sem_init(3) failed (" + std::to_string(errno) + ") (2)!");
}


Semaphore::~Semaphore() {
    TestAndThrowOrReturn(::sem_destroy(semaphore_) != 0, "sem_destroy(3) failed!");
    if (type_ == SINGLE_PROCESS)
        delete semaphore_;
}


void Semaphore::wait() {
try_again:
    if (::sem_wait(semaphore_) != 0) {
        if (errno == EINTR) {
            errno = 0;
            goto try_again;
        }
        throw std::runtime_error("in ThreadUtil::Semaphore::wait: sem_wait(3) failed (" + std::to_string(errno) + ")!");
    }
}


void Semaphore::post() {
try_again:
    if (::sem_post(semaphore_) != 0) {
        if (errno == EINTR) {
            errno = 0;
            goto try_again;
        }
        throw std::runtime_error("in ThreadUtil::Semaphore::post: sem_post(3) failed (" + std::to_string(errno) + ")!");
    }
}


const size_t MAX_BUF_SIZE(2048);


void Logger::reopen(const std::string &log_filename) {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    if (destroy_file_)
        delete log_file_;

    if (log_filename.empty() and log_filename_.empty())
        throw std::runtime_error("in ThreadUtil::Logger::reopen: no log file name available!");

    if (not log_filename.empty())
        log_filename_ = log_filename;

    log_file_ = new File(log_filename_.c_str(), "a");
    if (log_file_->fail())
        throw std::runtime_error("in ThreadUtil::Logger::reopen: can't open \"" + log_filename_ + "\" for logging!");
}


void Logger::log(const char *fmt, ...) {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    char msg_buffer[MAX_BUF_SIZE];

    va_list args;
    va_start(args, fmt);
    ::vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);

    internalLog(std::string(msg_buffer));
}


void Logger::log(const std::string &message) {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    *log_file_ << TimeUtil::GetCurrentDateAndTime() << " [" << ::getpid() << "]: " << message << File::endl;
    if (log_file_->fail())
        throw std::runtime_error("in ThreadUtil::Logger::logAndDie: failed to write to the log file \"" + log_filename_ + "\"!");
}


void Logger::sysLog(const char *fmt, ...) {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    char msg_buffer[MAX_BUF_SIZE];

    va_list args;
    va_start(args, fmt);
    ::vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);

    internalSysLog(std::string(msg_buffer));
}


void Logger::sysLog(const std::string &message) {
    std::lock_guard<std::mutex> mutex_locker(mutex_);
    internalSysLog(message);
}


void Logger::logAndDie(const char *fmt, ...) {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    char msg_buffer[MAX_BUF_SIZE];

    va_list args;
    va_start(args, fmt);
    ::vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);

    internalLogAndDie(std::string(msg_buffer));
}


void Logger::logAndDie(const std::string &message) {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    internalLogAndDie(message);
}


void Logger::sysLogAndDie(const char *fmt, ...) {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    char msg_buffer[MAX_BUF_SIZE];

    va_list args;
    va_start(args, fmt);
    ::vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);

    internalSysLogAndDie(std::string(msg_buffer));
}


void Logger::sysLogAndDie(const std::string &message) {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    internalSysLogAndDie(message);
}


// Logger::internalLog -- this low-level function intentionally does not do its own locking.
//
void Logger::internalLog(const std::string &message) {
    *log_file_ << TimeUtil::GetCurrentDateAndTime() << " [" << ::getpid() << "]: " << message << File::endl;
    if (log_file_->fail())
        throw std::runtime_error("in ThreadUtil::Logger::internalLog: failed to write to the log file \"" + log_filename_ + "\"!");
}


// Logger::internalSysLog -- this low-level function intentionally does not do its own locking.
//
void Logger::internalSysLog(const std::string &message) {
    *log_file_ << TimeUtil::GetCurrentDateAndTime() << " [" << ::getpid() << "]: " << message;
    if (errno != 0)
        *log_file_ << " [" << std::to_string(errno) << ']';
    *log_file_ << File::endl;
    if (log_file_->fail())
        throw std::runtime_error("in ThreadUtil::Logger::internalSysLog: failed to write to the log file \""
                                 + log_filename_ + "\"!");
}


// Logger::internalSysLogAndDie -- this low-level function intentionally does not do its own locking.
//
void Logger::internalSysLogAndDie(const std::string &message) {
    if (already_dead_)
        std::exit(EXIT_FAILURE);

    *log_file_ << TimeUtil::GetCurrentDateAndTime() << " [" << ::getpid() << "]: Exiting: " << message;
    if (errno != 0)
        *log_file_ << " [" << std::to_string(errno) << ']';
    *log_file_ << File::endl;
    if (log_file_->fail()) {
        already_dead_ = true;
        throw std::runtime_error("in ThreadUtil::Logger::internalSysLogAndDie: failed to write to the log file \""
                                 + log_filename_ + "\"!");
    }

    std::exit(EXIT_FAILURE);
}


// Logger::internalLogAndDie -- this low-level function intentionally does not do its own locking.
//
void Logger::internalLogAndDie(const std::string &message) {
    if (already_dead_)
        std::exit(EXIT_FAILURE);

    *log_file_ << TimeUtil::GetCurrentDateAndTime() << " [" << ::getpid() << "]: Exiting: " << message << File::endl;
    if (log_file_->fail()) {
        already_dead_ = true;
        throw std::runtime_error("in ThreadUtil::Logger::internalLogAndDie: failed to write to the log file \""
                                 + log_filename_ + "\"!");
    }

    std::exit(EXIT_FAILURE);
}


Spinlock::Spinlock(const Scope scope) {
    const int pshared(scope == PROCESS_LOCAL ? PTHREAD_PROCESS_PRIVATE : PTHREAD_PROCESS_SHARED);
    if (unlikely((errno = ::pthread_spin_init(&spinlock_, pshared)) != 0))
        throw std::runtime_error("in ThreadUtil::Spinlock::Spinlock: pthread_spin_init(3) failed ("
                                 + std::to_string(errno) + ")!");
}


Spinlock::~Spinlock() {
    if (unlikely((errno = ::pthread_spin_destroy(&spinlock_)) != 0)) {
        if (std::uncaught_exception())
            return;
        throw std::runtime_error("in ThreadUtil::Spinlock::~Spinlock: pthread_spin_destroy(3) failed ("
                                 + std::to_string(errno) + ")!");
    }
}


void Spinlock::lock() {
    if (unlikely((errno = ::pthread_spin_lock(&spinlock_)) != 0))
        throw std::runtime_error("in ThreadUtil::Spinlock::lock: pthread_spin_lock(3) failed (" + std::to_string(errno) + ")!");
}


bool Spinlock::tryLock() {
    errno = ::pthread_spin_trylock(&spinlock_);
    if (likely(errno == 0)) // Since we're supposed to use spinlocks for non-contended, short critical sections,
        return true;    // this should be the common case!

    if (unlikely(errno != EBUSY))
        throw std::runtime_error("in ThreadUtil::Spinlock::trylock: pthread_spin_trylock(3) failed ("
                                 + std::to_string(errno) + ")!");

    // If we got here, the error was EBUSY and we therefore failed to acquire the lock:
    return false; // Lock is busy, i.e. currently held by another process!
}


void Spinlock::unlock() {
    if (unlikely((errno = ::pthread_spin_unlock(&spinlock_)) != 0)) {
        if (std::uncaught_exception())
            return;
        throw std::runtime_error("in ThreadUtil::Spinlock::unlock: pthread_spin_unlock(3) failed (" + std::to_string(errno)
                                 + ")!");
    }
}


pid_t GetThreadId() {
    return ::syscall(SYS_gettid);
}


} // namespace ThreadUtil
