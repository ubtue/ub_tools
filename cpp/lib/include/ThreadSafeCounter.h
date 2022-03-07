/** \file   ThreadSafeCounter.h
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbibliothek Tübingen.  All rights reserved.
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


template <typename NumericType>
class ThreadSafeCounter {
    NumericType counter_;
    std::mutex mutex_;

public:
    explicit ThreadSafeCounter(const NumericType initial_counter_value = 0): counter_(initial_counter_value) { }

    void operator++() {
        std::unique_lock<std::mutex> mutex_locker(mutex_);
        ++counter_;
    }

    void operator++(int) {
        std::unique_lock<std::mutex> mutex_locker(mutex_);
        ++counter_;
    }

    NumericType get() {
        std::unique_lock<std::mutex> mutex_locker(mutex_);
        return counter_;
    }
};
