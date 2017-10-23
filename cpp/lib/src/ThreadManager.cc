#include "ThreadManager.h"
#include "util.h"


ThreadManager::ThreadManager(const unsigned no_of_threads, void *ThreadFunc(void *), void *thread_data)
    : thread_ids_(no_of_threads)
{
    for (unsigned thread_no(0); thread_no < no_of_threads; ++thread_no) {
        if (::pthread_create(&thread_ids_[thread_no], nullptr, ThreadFunc, thread_data) != 0)
            logger->error("thread creation of thread #" + std::to_string(thread_no) + " failed!");
    }
}


ThreadManager::~ThreadManager() {
    for (const auto thread_id : thread_ids_) {
        if (::pthread_cancel(thread_id) != 0)
            logger->error("failed to cancel thread with ID " + std::to_string(thread_id) + "!");
    }
}
