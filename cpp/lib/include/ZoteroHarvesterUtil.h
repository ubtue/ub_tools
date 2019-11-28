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
#include <functional>
#include <unordered_map>
#include <set>
#include <string>
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


class HarvestableItemManager;


struct HarvestableItem {
    friend class HarvestableItemManager;

    // Sortable unique ID that indicates the position of the harvestable item in a specific journal's harvest queue
    const unsigned id_;
    const Url url_;
    const Config::JournalParams &journal_;
private:
    HarvestableItem(const unsigned id, const std::string &url, const Config::JournalParams &journal)
     : id_(id), url_(url), journal_(journal) {}
public:
    ~HarvestableItem() = default;
    HarvestableItem(const HarvestableItem &rhs) = default;
    HarvestableItem &operator=(const HarvestableItem &rhs) = delete;
    bool operator==(const HarvestableItem &rhs) const;

    std::string toString() const;
};


class HarvestableItemManager {
    std::unordered_map<const Config::JournalParams *, ThreadUtil::ThreadSafeCounter<unsigned>> counters_;
public:
    HarvestableItemManager(const std::vector<std::unique_ptr<Config::JournalParams>> &journal_params);

    HarvestableItem newHarvestableItem(const std::string &url, const Config::JournalParams &journal_params);
};


} // end namespace Util


} // end namespace ZoteroHarvester


namespace std {


template <>
struct hash<ZoteroHarvester::Util::HarvestableItem> {
    size_t operator()(const ZoteroHarvester::Util::HarvestableItem &m) const {
        // http://stackoverflow.com/a/1646913/126995
        size_t res = 17;
        res = res * 31 + hash<unsigned>()(m.id_);
        res = res * 31 + hash<string>()(m.url_.toString());
        res = res * 31 + hash<const void *>()(&m.journal_);
        return res;
    }
};


} // end namespace std


namespace ZoteroHarvester {


namespace Util {


struct TaskletContext;


// Wrapper around the default logger to facilitate order-preserving logging in multi-threaded Zotero Harvester contexts.
class ZoteroLogger : public ::Logger {
    struct ContextData {
        static constexpr unsigned BUFFER_SIZE = 64 * 1024;

        Util::HarvestableItem item_;
        std::string buffer_;
    public:
        explicit ContextData(const Util::HarvestableItem &item) : item_(item)
         { buffer_.reserve(static_cast<size_t>(BUFFER_SIZE)); }
    };

    std::unordered_map<HarvestableItem, ContextData> active_contexts_;
    std::recursive_mutex active_context_mutex_;

    void queueMessage(const std::string &level, std::string msg, const TaskletContext &tasklet_context);
private:
    ZoteroLogger() = default;
    virtual ~ZoteroLogger() = default;
public:
    [[noreturn]] virtual void error(const std::string &msg) override __attribute__((noreturn));
    virtual void warning(const std::string &msg) override;
    virtual void info(const std::string &msg) override;
    virtual void debug(const std::string &msg) override;
    void pushContext(const Util::HarvestableItem &context_item);
    void popContext(const Util::HarvestableItem &context_item);

    // Replaces the global logger instance with one of this class
    static void Init();
 };


struct TaskletContext {
    const HarvestableItem associated_item_;
    const std::string description_;
public:
    TaskletContext(const HarvestableItem &associated_item, const std::string &description)
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


template <typename Parameter, typename Result> class Future;


template <typename Parameter, typename Result>
class Tasklet {
    friend class Future<Parameter, Result>;

    enum Status { NOT_STARTED, RUNNING, COMPLETED_SUCCESS, COMPLETED_ERROR };

    static void *ThreadRoutine(void * parameter);

    TaskletContext context_;
    ThreadUtil::ThreadSafeCounter<unsigned> * const running_instance_counter_;
    pthread_t thread_id_;
    mutable std::mutex mutex_;
    Status status_;
    const std::function<void(const Parameter &, Result * const)> runnable_;
    std::unique_ptr<Result> result_;
    std::unique_ptr<const Parameter> parameter_;
public:
    Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const running_instance_counter, const HarvestableItem &associated_item,
            const std::string &description, const std::function<void(const Parameter &, Result * const)> &runnable,
            std::unique_ptr<Result> default_result, std::unique_ptr<Parameter> parameter);
    virtual ~Tasklet();
public:
    void start();
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
    std::unique_ptr<Result> yieldResult();
    void await();
};


extern ThreadUtil::ThreadSafeCounter<unsigned> tasklet_instance_counter;


template<typename Parameter, typename Result> void *Tasklet<Parameter, Result>::ThreadRoutine(void * parameter) {
    Tasklet<Parameter, Result> * const tasklet(reinterpret_cast<Tasklet<Parameter, Result> *>(parameter));
    const auto zotero_logger(dynamic_cast<ZoteroLogger *>(::logger));

    pthread_setname_np(pthread_self(), tasklet->context_.description_.c_str());
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
}


template<typename Parameter, typename Result>
Tasklet<Parameter, Result>::Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const running_instance_counter,
                                    const HarvestableItem &associated_item, const std::string &description,
                                    const std::function<void(const Parameter &, Result * const)> &runnable,
                                    std::unique_ptr<Result> default_result, std::unique_ptr<Parameter> parameter)
 : context_(associated_item, description), running_instance_counter_(running_instance_counter), status_(Status::NOT_STARTED),
   runnable_(runnable), result_(std::move(default_result)), parameter_(std::move(parameter))
{
    ++tasklet_instance_counter;
}


template<typename Parameter, typename Result> Tasklet<Parameter, Result>::~Tasklet() {
    if (not isComplete()) {
        LOG_WARNING("tasklet '" + std::to_string(thread_id_) + "' is still running!"
                    + "\ndescription:" + context_.description_);

        if (::pthread_cancel(thread_id_) != 0)
            LOG_ERROR("failed to cancel tasklet thread '" + std::to_string(thread_id_) + "'!");
    }

    --tasklet_instance_counter;
}


template<typename Parameter, typename Result> void Tasklet<Parameter, Result>::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != Status::NOT_STARTED) {
        LOG_ERROR("tasklet '" + std::to_string(thread_id_) + "' has already been started!"
                  + "\nstatus = " + std::to_string(status_) + "\ndescription:" + context_.description_);
    }

    if (::pthread_create(&thread_id_, nullptr, ThreadRoutine, this) != 0)
        LOG_ERROR("tasklet thread creation failed!\ntasklet description: " + context_.description_);
}


template<typename Parameter, typename Result> std::unique_ptr<Result> Tasklet<Parameter, Result>::yieldResult() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != Status::COMPLETED_SUCCESS) {
        LOG_ERROR("tasklet '" + std::to_string(thread_id_) + "' has no result!"
                  + "\nstatus = " + std::to_string(status_) + "\ndescription:" + context_.description_);
    }

    if (result_ == nullptr) {
        LOG_ERROR("tasklet '" + std::to_string(thread_id_) + "' has already yielded its result!"
                  + "\ndescription:" + context_.description_);
    }

    return std::move(result_);
}


template<typename Parameter, typename Result> void Tasklet<Parameter, Result>::await() {
    // wait until the tasklet has started, as the thread won't be allocated until it does
    while (getStatus() == Status::NOT_STARTED) {
        ::usleep(32 * 1000);
    }

    if (isComplete())
        return;

    const auto ret_code(::pthread_join(thread_id_, nullptr));
    if (ret_code != 0) {
        LOG_ERROR("failed to join tasklet thread '" + std::to_string(thread_id_) + "'!"
                  " result = " + std::to_string(ret_code));
    }
}


extern ThreadUtil::ThreadSafeCounter<unsigned> future_instance_counter;


template <typename Parameter, typename Result>
class Future {
    enum Status { WAITING, NO_RESULT, YIELDED_RESULT, STATIC_RESULT };

    std::shared_ptr<Util::Tasklet<Parameter, Result>> source_tasklet_;
    std::unique_ptr<Result> result_;
    Status status_;
public:
    Future(std::shared_ptr<Util::Tasklet<Parameter, Result>> source_tasklet);
    Future(std::unique_ptr<Result> result);
    ~Future();
    Future(const Future<Parameter, Result> &rhs) = delete;

    bool isComplete() const;
    bool hasResult() const;
    Result &getResult();
    inline const Parameter &getParameter() const { return source_tasklet_->getParameter(); }
};


template <typename Parameter, typename Result> Future<Parameter, Result>::Future(
    std::shared_ptr<Util::Tasklet<Parameter, Result>> source_tasklet)
 : source_tasklet_(source_tasklet), status_(WAITING)
{
    ++future_instance_counter;
}


template <typename Parameter, typename Result> Future<Parameter, Result>::Future(std::unique_ptr<Result> result)
 : result_(std::move(result)), status_(STATIC_RESULT)
{
    ++future_instance_counter;
}


template <typename Parameter, typename Result> Future<Parameter, Result>::~Future() {
    --future_instance_counter;
}


template <typename Parameter, typename Result> bool Future<Parameter, Result>::isComplete() const {
    if (status_ == Status::STATIC_RESULT)
        return true;
    else switch (source_tasklet_->getStatus()) {
    case Util::Tasklet<Parameter, Result>::Status::COMPLETED_SUCCESS:
    case Util::Tasklet<Parameter, Result>::Status::COMPLETED_ERROR:
        return true;
    default:
        return false;
    }
};


template <typename Parameter, typename Result> bool Future<Parameter, Result>::hasResult() const {
    if (status_ == Status::STATIC_RESULT)
        return true;
    else switch (source_tasklet_->getStatus()) {
    case Util::Tasklet<Parameter, Result>::Status::COMPLETED_SUCCESS:
        return true;
    default:
        return false;
    }
};


template <typename Parameter, typename Result> Result &Future<Parameter, Result>::getResult() {
    if (status_ == Status::WAITING) {
        source_tasklet_->await();
        if (hasResult()) {
            result_.reset(source_tasklet_->yieldResult().release());
            status_ = Status::YIELDED_RESULT;
        } else
            status_ = Status::NO_RESULT;
    }

    return *result_;
}


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
