/** \file   SharedBuffer.h
 *  \brief  Template class of a buffer that can be used to commuicate safely between multiple threads.
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


#include <condition_variable>
#include <deque>
#include <mutex>


/** Implements a buffer or queue that can be shared between threads. */
template <typename ItemType>
class SharedBuffer {
    const size_t max_size_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<ItemType> buffer_;

public:
    explicit SharedBuffer(const size_t max_size): max_size_(max_size) { }

    bool empty() {
        std::unique_lock<std::mutex> mutex_locker(mutex_);
        volatile const bool is_empty(buffer_.empty());
        return is_empty;
    }

    void push_back(const ItemType new_item) {
        std::unique_lock<std::mutex> mutex_locker(mutex_);
        condition_.wait(mutex_locker, [this]() { return buffer_.size() < max_size_; });
        buffer_.emplace_back(new_item);
        mutex_locker.unlock();
        condition_.notify_all();
    }

    ItemType pop_front() {
        std::unique_lock<std::mutex> mutex_locker(mutex_);
        condition_.wait(mutex_locker, [this]() { return not buffer_.empty(); });
        const ItemType item(buffer_.front());
        buffer_.pop_front();
        mutex_locker.unlock();
        condition_.notify_all();
        return item;
    }
};
