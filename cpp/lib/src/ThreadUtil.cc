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
        throw std::runtime_error("in ThreadUtil::Semaphore::Semaphore: sem_init(3) failed!");
}


Semaphore::Semaphore(char * const shared_memory, const bool init, const unsigned initial_count)
    : semaphore_(reinterpret_cast<sem_t *>(shared_memory)), type_(MULTI_PROCESS)
{
    if (init and ::sem_init(semaphore_, 1, initial_count) != 0)
        throw std::runtime_error("in ThreadUtil::Semaphore::Semaphore: sem_init(3) failed!");
}


Semaphore::~Semaphore() {
    if (::sem_destroy(semaphore_) != 0)
        logger->error("in ThreadUtil::Semaphore::~Semaphore: sem_destroy(3) failed!");
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
        throw std::runtime_error("in ThreadUtil::Semaphore::wait: sem_wait(3) failed!");
    }
}


void Semaphore::post() {
try_again:
    if (::sem_post(semaphore_) != 0) {
        if (errno == EINTR) {
            errno = 0;
            goto try_again;
        }
        throw std::runtime_error("in ThreadUtil::Semaphore::post: sem_post(3) failed!");
    }
}


Spinlock::Spinlock(const Scope scope) {
    const int pshared(scope == PROCESS_LOCAL ? PTHREAD_PROCESS_PRIVATE : PTHREAD_PROCESS_SHARED);
    if (unlikely((errno = ::pthread_spin_init(&spinlock_, pshared)) != 0))
        throw std::runtime_error("in ThreadUtil::Spinlock::Spinlock: pthread_spin_init(3) failed!");
}


Spinlock::~Spinlock() {
    if (unlikely((errno = ::pthread_spin_destroy(&spinlock_)) != 0)) {
        if (std::uncaught_exception())
            return;
        logger->error("in ThreadUtil::Spinlock::~Spinlock: pthread_spin_destroy(3) failed!");
    }
}


void Spinlock::lock() {
    if (unlikely((errno = ::pthread_spin_lock(&spinlock_)) != 0))
        throw std::runtime_error("in ThreadUtil::Spinlock::lock: pthread_spin_lock(3) failed!");
}


bool Spinlock::tryLock() {
    errno = ::pthread_spin_trylock(&spinlock_);
    if (likely(errno == 0)) // Since we're supposed to use spinlocks for non-contended, short critical sections,
        return true;    // this should be the common case!

    if (unlikely(errno != EBUSY))
        throw std::runtime_error("in ThreadUtil::Spinlock::trylock: pthread_spin_trylock(3) failed!");

    // If we got here, the error was EBUSY and we therefore failed to acquire the lock:
    return false; // Lock is busy, i.e. currently held by another process!
}


void Spinlock::unlock() {
    if (unlikely((errno = ::pthread_spin_unlock(&spinlock_)) != 0)) {
        if (std::uncaught_exception())
            return;
        throw std::runtime_error("in ThreadUtil::Spinlock::unlock: pthread_spin_unlock(3) failed!");
    }
}


pid_t GetThreadId() {
    return ::syscall(SYS_gettid);
}


} // namespace ThreadUtil
