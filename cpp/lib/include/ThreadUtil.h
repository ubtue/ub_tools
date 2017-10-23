/** \file    ThreadUtil.h
 *  \brief   Declaration of thread-related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2003-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
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

#ifndef THREAD_UTIL_H
#define THREAD_UTIL_H


#include <mutex>
#include <stdexcept>
#include <pthread.h>
#include <semaphore.h>


namespace ThreadUtil {


class Semaphore {
    sem_t *semaphore_;
    enum Type { SINGLE_PROCESS = 0, MULTI_PROCESS = 1 } type_;
public:
    /** \brief  Creates a single process semphore.  (Shared by all threads of the current process.)
     *  \param  initial_count  Initial value for semaphore.
     */
    explicit Semaphore(const unsigned initial_count = 0);

    /** \brief  Creates a semaphore in a shared memory region.
     *  \param  shared_memory  Must point to a shared memory region with at least sizeof(sem_t) bytes.
     *  \param  init           Must be true only for the first call for each semaphore.  Typically this might be done in a parent process.
     *  \param  initial_count  Initial value for semaphore, ignored if "init" is false.
     */
    explicit Semaphore(char * const shared_memory, const bool init, const unsigned initial_count = 0);

    ~Semaphore();
    void wait();
    void post();
private:
    Semaphore(const Semaphore &);                  // Intentionally unimplemented!
    const Semaphore &operator=(const Semaphore &); // Intentionally unimplemented!
};


/** \class  Spinlock.
 *  \brief  Implements a spinlock class.
 *  \note   Typically you probably want to use this class in conjunction with the SpinlockLocker class.
 */
class Spinlock {
    pthread_spinlock_t spinlock_;
public:
    enum Scope { PROCESS_LOCAL, PROCESS_GLOBAL };
public:
    explicit Spinlock(const Scope scope = PROCESS_LOCAL);
    ~Spinlock();
    void lock();
    bool tryLock();
    void unlock();
private:
    Spinlock(const Spinlock &);                  // Intentionally unimplemented!
    const Spinlock &operator=(const Spinlock &); // Intentionally unimplemented!
};


/** \brief  Helper class that makes use of the C++ scope rules to ensure that a spinlock is automatically unlocked.
 *  \note   This class provides for a critical section guarded by the spinlock that is passed into the public
 *          constructor.  A critical section ends when an instance of this class goes out of scope.  The mechanism
 *          provided by this class also guarantees that when an exception is thrown that a spinlock managed by an
 *          instance of this class will be unlocked.
 */
class SpinlockLocker {
    Spinlock *spinlock_;
public:
    explicit SpinlockLocker(Spinlock * const spinlock) : spinlock_(spinlock) { spinlock_->lock(); }
    ~SpinlockLocker() { spinlock_->unlock(); }
private:
    SpinlockLocker(const SpinlockLocker &);                  // Intentionally unimplemented!
    const SpinlockLocker &operator=(const SpinlockLocker &); // Intentionally unimplemented!
};


/** \class  ThreadSafeCounter
 *  \brief  Implements a numeric counter that can safely be shared between threads.
 *  \note   Typical usage would be to create an instance of this class in some "main" thread and pass references into
 *          worker threads that call the increment and decrement operators as needed.
 */
template <typename NumericType> class ThreadSafeCounter {
    mutable std::mutex mutex_;
    NumericType counter_;
public:
    explicit ThreadSafeCounter(const NumericType initial_value = 0): counter_(initial_value) { }
    operator NumericType() const;
    NumericType operator++();
    NumericType operator--();
    NumericType operator++(int);
    NumericType operator--(int);
private:
    void screwPointers() { counter_ /= 1; } // Prevent this template class from being instantiated w/ a pointer type!
};


template <typename NumericType> ThreadSafeCounter<NumericType>::operator NumericType() const {
    std::lock_guard<std::mutex> mutex_locker(mutex_);

    return counter_;
}


template <typename NumericType> NumericType ThreadSafeCounter<NumericType>::operator++() {
    std::lock_guard<std::mutex> mutex_locker(mutex_);
    ++counter_;

    return counter_;
}


template <typename NumericType> NumericType ThreadSafeCounter<NumericType>::operator++(int) {
    std::lock_guard<std::mutex> mutex_locker(mutex_);
    const NumericType previous_value(counter_);
    ++counter_;

    return previous_value;
}


template <typename NumericType> NumericType ThreadSafeCounter<NumericType>::operator--() {
    std::lock_guard<std::mutex> mutex_locker(mutex_);
    if (unlikely(counter_ == 0))
        throw std::runtime_error("in ThreadSafeCounter::operator--: trying to decrement a zero counter!");
    --counter_;

    return counter_;
}


template <typename NumericType> NumericType ThreadSafeCounter<NumericType>::operator--(int) {
    std::lock_guard<std::mutex> mutex_locker(mutex_);
    if (unlikely(counter_ == 0))
        throw std::runtime_error("in ThreadSafeCounter::operator--(int): trying to decrement a zero counter!");
    const NumericType previous_value(counter_);
    --counter_;

    return previous_value;
}


/** \class  AutoIncDecCounter
 *  \brief  A class that increments a ThreadSafeCounter when being instantiated and decrements the same counter when
 *          going out of scope.
 */
template <typename NumericType> class AutoIncDecCounter {
    ThreadSafeCounter<NumericType> &thread_safe_counter_;
public:
    explicit AutoIncDecCounter(ThreadSafeCounter<NumericType> * const thread_safe_counter)
        : thread_safe_counter_(*thread_safe_counter) { ++thread_safe_counter_; }
    ~AutoIncDecCounter() { --thread_safe_counter_; }
};


/** \class  LockingPtr
 *  \brief  Handles locking mutex and const casting away volatile from object. This is useful in multithreaded context.
 */
template <typename T> class LockingPtr {
    T *object_;
    std::lock_guard<std::mutex> lock_;
public:
    LockingPtr(volatile const T &object, std::mutex * const mutex): object_(const_cast<T *>(&object)), lock_(*mutex) { }

    T &operator*() { return *object_; }
    T *operator->() { return object_; }
    T *get() { return object_; }
private:
    LockingPtr(const LockingPtr &);            // Intentionally unimplemented!
    LockingPtr &operator=(const LockingPtr &); // Intentionally unimplemented!
};


pid_t GetThreadId();


} // namespace ThreadUtil


#endif // ifndef THREAD_UTIL_H
