/** \brief Utility classes related to the Zotero Harvester
 *  \author Madeeswaran Kannan
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include <memory>
#include <set>
#include <functional>
#include <pthread.h>
#include "DbConnection.h"
#include "JSON.h"
#include "RegexMatcher.h"
#include "SqlUtil.h"
#include "ThreadUtil.h"
#include "Url.h"
#include "ZoteroHarvesterConfig.h"
#include "util.h"


namespace ZoteroHarvester {


namespace Util {


struct Harvestable {
    // Sortable unique ID that indicates the position of the harvestable item in the global harvest queue
    const unsigned id_;
    const Url url_;
    const Config::JournalParams &journal_;
private:
    Harvestable(const unsigned id, const std::string url, const Config::JournalParams &journal)
     : id_(id), url_(url), journal_(journal) {}
public:
    Harvestable(const Harvestable &rhs) = default;

    static Harvestable New(const std::string &url, const Config::JournalParams &journal);
};


struct TaskletContext;


// Wrapper around the default logger to facilitate order-preserving logging in multi-threaded Zotero Harvester contexts.
class ZoteroLogger : public ::Logger {
    struct ContextData {
        static constexpr unsigned BUFFER_SIZE = 64 * 1024;

        Util::Harvestable item_;
        std::string buffer_;
    public:
        explicit ContextData(const Util::Harvestable &item) : item_(item)
         { buffer_.reserve(static_cast<size_t>(BUFFER_SIZE)); }
    };

    std::unordered_map<unsigned, ContextData> active_contexts_;
    std::mutex active_context_mutex_;

    void queueMessage(const std::string &level, std::string msg, const TaskletContext &tasklet_context);
private:
    ZoteroLogger() = default;
    virtual ~ZoteroLogger() = default;
public:
    [[noreturn]] virtual void error(const std::string &msg) override __attribute__((noreturn));
    virtual void warning(const std::string &msg) override;
    virtual void info(const std::string &msg) override;
    virtual void debug(const std::string &msg) override;
    void pushContext(const Util::Harvestable &context_item);
    void popContext(const Util::Harvestable &context_item);

    // Replaces the global logger instance with one of this class
    static void Init();
 };


template <typename T>
class ThreadLocal {
    pthread_key_t key_;
    const std::function<std::unique_ptr<T>()> initializer_;

    static void Destructor(void *data) {
        std::unique_ptr<T> wrapper(reinterpret_cast<T *>(data));
    }
public:
    ThreadLocal(const std::function<std::unique_ptr<T>()> &&initialzer)
     : initializer_(initialzer)
    {
        if (pthread_key_create(&key_, &Destructor) != 0)
            LOG_ERROR("could not create thread local data key");
    }
    ~ThreadLocal() {
        if (pthread_key_delete(key_) != 0)
            LOG_ERROR("could not delete thread local data key");
    }
    ThreadLocal(const ThreadLocal<T> &) = delete;
    ThreadLocal<T> &operator=(const ThreadLocal<T> &) = delete;

    T *get() {
        auto thread_data(pthread_getspecific(key_));
        if (thread_data == nullptr) {
            auto new_data(initializer_());
            if (pthread_setspecific(key_, new_data.get()) != 0)
                LOG_ERROR("could not set thread local data for thread " + std::to_string(pthread_self()));
            thread_data = new_data.release();
        }

        return reinterpret_cast<T *>(thread_data);
    }
    T &operator*() {
        return *get();
    }
    T *operator->() {
        return get();
    }
};


struct TaskletContext {
    const Harvestable associated_item_;
    const std::string description_;
public:
    TaskletContext(const Harvestable &associated_item, const std::string &description)
     : associated_item_(associated_item), description_(description) {}
};


class TaskletContextManager {
    pthread_key_t tls_key_;
public:
    TaskletContextManager();
    ~TaskletContextManager();

    void setThreadLocalContext(const TaskletContext &context) const;
    TaskletContext *getThreadLocalContext() const;
};


extern const TaskletContextManager TASKLET_CONTEXT_MANAGER;


template <typename Parameter, typename Result>
class Tasklet {
    enum Status { NOT_STARTED, RUNNING, COMPLETED_SUCCESS, COMPLETED_ERROR };

    static void *ThreadRoutine(void * parameter) {
        Tasklet<Parameter, Result> * const tasklet(reinterpret_cast<Tasklet<Parameter, Result> *>(parameter));
        const auto zotero_logger(dynamic_cast<ZoteroLogger *>(::logger));

        pthread_detach(pthread_self());
        TASKLET_CONTEXT_MANAGER.setThreadLocalContext(tasklet->context_);
        zotero_logger->pushContext(tasklet->context_.associated_item_);
        ++(*tasklet->running_instance_counter_);

        try {
            {
                std::lock_guard<std::mutex> lock(tasklet->mutex_);
                tasklet->status_ = Status::RUNNING;
            }
            tasklet->runnable_(*tasklet->parameter_.get(), tasklet->result_.get());
            {
                std::lock_guard<std::mutex> lock(tasklet->mutex_);
                tasklet->status_ = Status::COMPLETED_SUCCESS;
            }
        } catch (const std::runtime_error &exception) {
            LOG_WARNING("exception in tasklet '" + std::to_string(tasklet->thread_id_) + "': " + exception.what()
                        + "\ntasklet description: "  + tasklet->context_.description_);
            {
                std::lock_guard<std::mutex> lock(tasklet->mutex_);
                tasklet->status_ = Status::COMPLETED_ERROR;
            }
        } catch (...) {
            LOG_WARNING("unknown exception in tasklet '" + std::to_string(tasklet->thread_id_) + "'"
                        + "\ntasklet description: "  + tasklet->context_.description_);
            {
                std::lock_guard<std::mutex> lock(tasklet->mutex_);
                tasklet->status_ = Status::COMPLETED_ERROR;
            }
        }

        zotero_logger->popContext(tasklet->context_.associated_item_);
        --(*tasklet->running_instance_counter_);
        pthread_exit(nullptr);

        return nullptr;
    }

    TaskletContext context_;
    ThreadUtil::ThreadSafeCounter<unsigned> * const running_instance_counter_;
    pthread_t thread_id_;
    mutable std::mutex mutex_;
    Status status_;
    const std::function<void(const Parameter &, Result * const)> runnable_;
    std::unique_ptr<const Parameter> parameter_;
    std::unique_ptr<Result> result_;
public:
    Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const running_instance_counter, const Harvestable &associated_item,
            const std::string &description, const std::function<void(const Parameter &, Result * const)> &runnable,
            std::unique_ptr<Parameter> parameter, std::unique_ptr<Result> default_result)
     : context_(associated_item, description), running_instance_counter_(running_instance_counter), status_(Status::NOT_STARTED),
       runnable_(runnable), parameter_(std::move(parameter)), result_(std::move(default_result)) {}
    virtual ~Tasklet() {
        if (::pthread_cancel(thread_id_) != 0)
            LOG_ERROR("failed to cancel tasklet thread '" + std::to_string(thread_id_) + "'!");
        if (status_ == Status::RUNNING) {
            LOG_WARNING("tasklet '" + std::to_string(thread_id_) + "' is still running!"
                      + "\ndescription:" + context_.description_);
        }
    }
public:
    void start() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (status_ != Status::NOT_STARTED) {
            LOG_ERROR("tasklet '" + std::to_string(thread_id_) + "' has already been started!"
                      + "\nstatus = " + std::to_string(status_) + "\ndescription:" + context_.description_);
        }

        if (::pthread_create(&thread_id_, nullptr, ThreadRoutine, this) != 0)
            LOG_ERROR("tasklet thread creation failed!\ntasklet description: " + context_.description_);


    }
    inline pthread_t getID() const { return thread_id_; }
    inline Status getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_;
    }
    inline bool isComplete() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_ == Status::COMPLETED_SUCCESS or status_ == Status::COMPLETED_ERROR;
    }
    inline const Parameter &getParameter() const { return *parameter_.get(); }
    std::unique_ptr<Result> yieldResult() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (status_ != Status::COMPLETION_SUCCESS) {
            LOG_ERROR("tasklet '" + std::to_string(thread_id_) + "' has no result!"
                      + "\nstatus = " + std::to_string(status_) + "\ndescription:" + context_.description_);
        }

        if (result_ == nullptr) {
            LOG_ERROR("tasklet '" + std::to_string(thread_id_) + "' has already yielded its result!"
                      + "\ndescription:" + context_.description_);
        }

        return result_.release();
    }
    void await() {
        // wait until the tasklet has started, as the thread wouldn't allocated until it does
        while (getStatus() == Status::NOT_STARTED) {
            ::usleep(16 * 1000 * 1000);
        }

        if (::pthread_join(thread_id_, nullptr) != 0)
            LOG_ERROR("failed to join tasklet thread '" + std::to_string(thread_id_) + "'!");
    }
};


// Tracks harvested records that have been uploaded to the BSZ server.
class UploadTracker {
    std::unique_ptr<DbConnection> db_connection_;
public:
    struct Entry {
        std::string url_;
        std::string journal_name_;
        time_t delivered_at_;
        std::string hash_;
    };
public:
    explicit UploadTracker(std::unique_ptr<DbConnection> db): db_connection_(std::move(db)) {}

    /** \brief Checks if "url" or ("url", "hash") have already been uploaded.
     *  \return True if we have find an entry for "url" or ("url", "hash"), else false.
     */
    bool hasAlreadyBeenUploaded(const std::string &url, const std::string &hash = "", Entry * const entry = nullptr) const;

    /** \brief Lists all journals that haven't had a single URL delivered for a given number of days.
     *  \return The number of outdated journals.
     */
    size_t listOutdatedJournals(const unsigned cutoff_days, std::unordered_map<std::string, time_t> * const outdated_journals);

    /** \brief Returns when the last URL of the given journal was delivered to the BSZ.
     *  \return Timestamp of the last delivery if found, TimeUtil::BAD_TIME_T otherwise.
     */
    time_t getLastUploadTime(const std::string &journal_name) const;
private:
    inline void truncateURL(std::string * const url) const {
        if (url->length() > static_cast<std::size_t>(SqlUtil::VARCHAR_UTF8_MAX_LENGTH))
            url->erase(SqlUtil::VARCHAR_UTF8_MAX_LENGTH);
    }
};


} // end namespace Util


} // end namespace ZoteroHarvester
