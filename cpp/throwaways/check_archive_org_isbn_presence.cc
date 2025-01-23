/** \brief Utility for checking which ISBNS can be found on archive.org.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <atomic>
#include <chrono>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include "Downloader.h"
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--summarize-tags] [--verbose] worker_thread_count marc_data isbn_list_output");
}


std::atomic<bool> work_available(true);


void WorkerThread(Downloader * const downloader, std::deque<std::set<std::string>> * const task_queue, std::mutex * const task_queue_mutex,
                  unsigned * const record_found_count, File * const isbn_list_output, std::mutex * const output_mutex,
                  unsigned * const failed_download_count) {
    std::set<std::string> isbns;
    for (;;) {
        {
            std::lock_guard<std::mutex> task_queue_mutex_locker(*task_queue_mutex);
            if (not task_queue->empty()) {
                isbns = task_queue->front();
                task_queue->pop_front();
            }
        }

        if (isbns.empty()) {
            if (not work_available)
                return;
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        for (const auto &isbn : isbns) {
            const std::string url("https://archive.org/metadata/isbn_" + isbn + "/created");
            if (not downloader->newUrl(url, 2 * Downloader::DEFAULT_TIME_LIMIT)) {
                std::lock_guard<std::mutex> output_mutex_locker(*output_mutex);
                LOG_WARNING("URL \"" + url + " failed to download! (" + downloader->getLastErrorMessage() + ")");
                ++*failed_download_count;
                continue;
            }

            if (downloader->getMessageBody().find("result") != std::string::npos) {
                std::lock_guard<std::mutex> output_mutex_locker(*output_mutex);
                isbn_list_output->writeln(isbn);
                ++*record_found_count;
                std::cout << *record_found_count << '\n';
                break;
            }
        }

        isbns.clear();
    }
}


void ProcessRecords(MARC::Reader * const marc_reader, std::deque<std::set<std::string>> * const task_queue,
                    std::mutex * const task_queue_mutex) {
    while (const MARC::Record record = marc_reader->read()) {
        const auto isbns(record.getISBNs());
        if (isbns.empty())
            continue;

        std::lock_guard<std::mutex> task_queue_mutex_locker(*task_queue_mutex);
        task_queue->push_back(isbns);
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    const unsigned WORKER_THREAD_COUNT(StringUtil::ToUnsigned(argv[1]));
    std::vector<std::thread> thread_pool((WORKER_THREAD_COUNT));
    std::deque<std::set<std::string>> task_queue;
    std::mutex task_queue_mutex, output_mutex;
    unsigned record_found_count(0);
    auto isbn_list_output(FileUtil::OpenOutputFileOrDie(argv[3]));
    unsigned failed_download_count(0);
    for (size_t i(0); i < WORKER_THREAD_COUNT; ++i)
        thread_pool[i] = std::thread(WorkerThread, new Downloader(), &task_queue, &task_queue_mutex, &record_found_count,
                                     isbn_list_output.get(), &output_mutex, &failed_download_count);

    auto marc_reader(MARC::Reader::Factory(argv[2]));
    ProcessRecords(marc_reader.get(), &task_queue, &task_queue_mutex);

    work_available = false; // Let our worker threads return.
    for (size_t i(0); i < WORKER_THREAD_COUNT; ++i)
        thread_pool[i].join();
    LOG_INFO("Found " + std::to_string(record_found_count) + " monographs on Archive.org. (" + std::to_string(failed_download_count)
             + " failed downloads!)");

    return EXIT_SUCCESS;
}
