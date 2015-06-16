#include <iostream>
#include <mutex>
#include <vector>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#include "SharedBuffer.h"
#include "StringUtil.h"
#include "util.h"


std::mutex io_mutex;


void *Consumer(void *shared_data) {
    if (::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) != 0)
	Error("consumer thread failed to enable cancelability!");

    SharedBuffer<unsigned> * const shared_buffer(reinterpret_cast<SharedBuffer<unsigned> * const>(shared_data));
    for (;;) {
	const unsigned u(shared_buffer->pop_front());
	std::unique_lock<std::mutex> mutex_locker(io_mutex);
	std::cout << u << '\n';
    }

    return NULL;
}


void Usage() {
    std::cerr << "usage: " << progname << " number_count consumer_thread_count\n";
    std::cerr << "       Generates \"number_count\" and uses \"consumer_thread_count\" thread to print them.\n";
    std::exit(EXIT_FAILURE);
}


class ThreadManager {
    std::vector<pthread_t> thread_ids_;
public:
    ThreadManager(const unsigned no_of_threads, void *ThreadFunc(void *), void *thread_data = NULL);
    ~ThreadManager();
};


ThreadManager::ThreadManager(const unsigned no_of_threads, void *ThreadFunc(void *), void *thread_data)
    : thread_ids_(no_of_threads)
{
    for (unsigned thread_no(0); thread_no < no_of_threads; ++thread_no) {
	if (::pthread_create(&thread_ids_[thread_no], NULL, ThreadFunc, thread_data) != 0)
            Error("thread creation of thread #" + std::to_string(thread_no) + " failed!");
    }
}


ThreadManager::~ThreadManager() {
    for (const auto thread_id : thread_ids_) {
	if (::pthread_cancel(thread_id) != 0)
	    Error("failed to cancel thread with ID " + std::to_string(thread_id) + "!");
    }
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc != 3)
	Usage();
    
    unsigned number_count;
    if (not StringUtil::ToUnsigned(argv[1], &number_count) or number_count == 0)
	Usage();
    
    unsigned consumer_thread_count;
    if (not StringUtil::ToUnsigned(argv[1], &consumer_thread_count) or consumer_thread_count == 0)
	Usage();

    SharedBuffer<unsigned> number_buffer(consumer_thread_count);
    ThreadManager thread_manager(consumer_thread_count, Consumer, &number_buffer);

    for (unsigned u(1); u <= number_count; ++u)
	number_buffer.push_back(u);

    while (not number_buffer.empty())
	::sleep(1);
}


