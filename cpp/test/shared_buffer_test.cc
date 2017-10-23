#include <iostream>
#include <mutex>
#include <vector>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#include "SharedBuffer.h"
#include "StringUtil.h"
#include "ThreadManager.h"
#include "util.h"


std::mutex io_mutex;


void *Consumer(void *shared_data) {
    int old_state;
    if (::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state) != 0)
        logger->error("consumer thread failed to enable cancelability!");

    SharedBuffer<unsigned> * const shared_buffer(reinterpret_cast<SharedBuffer<unsigned> * const>(shared_data));
    for (;;) {
        const unsigned u(shared_buffer->pop_front());
        std::unique_lock<std::mutex> mutex_locker(io_mutex);
        std::cout << u << '\n';
    }

    return nullptr;
}


void Usage() {
    std::cerr << "usage: " << progname << " number_count consumer_thread_count\n";
    std::cerr << "       Generates \"number_count\" and uses \"consumer_thread_count\" thread to print them.\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc != 3)
        Usage();
    
    unsigned number_count;
    if (not StringUtil::ToUnsigned(argv[1], &number_count) or number_count == 0)
        Usage();
    
    unsigned consumer_thread_count;
    if (not StringUtil::ToUnsigned(argv[2], &consumer_thread_count) or consumer_thread_count == 0)
        Usage();

    SharedBuffer<unsigned> number_buffer(consumer_thread_count);
    ThreadManager thread_manager(consumer_thread_count, Consumer, &number_buffer);

    for (unsigned u(1); u <= number_count; ++u)
        number_buffer.push_back(u);

    while (not number_buffer.empty())
        ::sleep(1);
}


