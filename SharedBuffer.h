#ifndef SHARED_BUFFER_H
#define SHARED_BUFFER_H


#include <condition_variable>
#include <deque>
#include <mutex>


/** Implements a buffer or queue that can be shared between threads. */
template<typename ItemType> class SharedBuffer {
    const size_t max_size_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<ItemType> buffer_;
public:
    explicit SharedBuffer(const size_t max_size): max_size_(max_size) { }

    void push_back(const ItemType new_item) {
	std::unique_lock<std::mutex> mutex_locker(mutex_);
	condition_.wait(mutex_locker, [this](){ return buffer_.size() < max_size_; });
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
    

#endif // ifndef SHARED_BUFFER_H
