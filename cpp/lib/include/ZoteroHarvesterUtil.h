/** \brief Utility classes related to the Zotero Harvester
 *  \author Madeeswaran Kannan
 *
 *  \copyright 2019-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include <atomic>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <pthread.h>
#include "DbConnection.h"
#include "JSON.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "SqlUtil.h"
#include "ThreadUtil.h"
#include "Url.h"
#include "Zeder.h"
#include "ZoteroHarvesterConfig.h"
#include "util.h"


namespace ZoteroHarvester {


// This namespace contains classes that provide the necessary primitives to implement
// scalable harvesting of metadata using the Zotero Translation Server.
namespace Util {


class HarvestableItemManager;


// Represents a URI that contains one or more harvestable resources. All operations are
// keyed to a specific instance of this class. It holds the relevant contextual information
// about the resource it points to (such as its parent journal and its respective parameters).
// It also doubles as a unique handle that can used to track and sort operations that are
// executing concurrently.
struct HarvestableItem {
    friend class HarvestableItemManager;

    // Sortable unique ID that indicates the position of the harvestable item in
    // a specific journal's harvest queue.
    const unsigned id_;

    // URL pointing to the harvestable resource.
    const Url url_;

    // Journal to which this harvestable resource belongs.
    const Config::JournalParams &journal_;

private:
    HarvestableItem(const unsigned id, const std::string &url, const Config::JournalParams &journal)
        : id_(id), url_(url), journal_(journal) { }

public:
    ~HarvestableItem() = default;
    HarvestableItem(const HarvestableItem &rhs) = default;
    HarvestableItem &operator=(const HarvestableItem &rhs) = delete;
    bool operator==(const HarvestableItem &rhs) const;

    std::string toString() const;
};


// Allocates HarvestableItem instances whilst ensuring that there are collisions of
// unique IDs between different journals. The IDs provide a way to preserve the relative
// ordering of asynchronously executing operations.
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


// Wrapper around the default logger that facilitates order-preserving logging in multi-threaded contexts.
// Ensures that given a specific asynchronous operation/task, the ordering of log statements is preserved.
// This is achieved by tracking active asynchronous contexts and accumulating messages in separate buffers.
// When a context deregisters itself from the logger, its buffer is queued in the logger's global buffer
// which is flushed in the main thread.
//
// The error, warning, info and debug member function overrides attempt to obtain the calling thread's
// TaskletContext. When found, the message is written directly to the context's buffer. When not, it's
// queued in the global buffer and eventually flushed.
class ZoteroLogger : public ::Logger {
    using ContextKey = std::pair<::pthread_t, HarvestableItem>;
    struct ContextKeyHasher {
        std::size_t operator()(const ContextKey &key) const {
            // http://stackoverflow.com/a/1646913/126995
            std::size_t res = 17;
            res = res * 31 + std::hash<::pthread_t>()(key.first);
            res = res * 31 + std::hash<HarvestableItem>()(key.second);
            return res;
        }
    };


    struct ContextData {
        static constexpr unsigned BUFFER_SIZE = 64 * 1024;

        Util::HarvestableItem item_;
        std::string buffer_;

    public:
        explicit ContextData(const Util::HarvestableItem &item);
    };


    std::unordered_map<ContextKey, ContextData, ContextKeyHasher> active_contexts_;
    std::deque<std::string> log_buffer_;
    std::string progress_bar_buffer_;
    std::recursive_mutex active_context_mutex_;
    std::recursive_mutex log_buffer_mutex_;
    mutable std::atomic_bool fatal_error_all_stop_;

    void queueContextMessage(const std::string &level, std::string msg, const ::pthread_t tasklet_thread_id,
                             const TaskletContext &tasklet_context);
    void queueGlobalMessage(const std::string &level, std::string msg);
    void flushBufferAndPrintProgressImpl(const unsigned num_active_tasks, const unsigned num_queued_tasks);
    void writeToBackingLog(const std::string &msg);

private:
    ZoteroLogger() = default;
    virtual ~ZoteroLogger() = default;

public:
    [[noreturn]] virtual void error(const std::string &msg) override __attribute__((noreturn));
    virtual void warning(const std::string &msg) override;
    virtual void info(const std::string &msg) override;
    virtual void debug(const std::string &msg) override;
    void registerTasklet(const ::pthread_t tasklet_thread_id, const HarvestableItem &associated_item);
    void deregisterTasklet(const ::pthread_t tasklet_thread_id, const HarvestableItem &associated_item);

    // Replaces the global logger instance with one of this class
    // so that all LOG_XXX calls are routed through it.
    // Must ONLY be called once at the beginning of the main thread.
    static void Init();

    static ZoteroLogger &Get();

    // Flushes the logger's buffer and prints a progress message.
    // Must be called in a loop (and ONLY) in the main thread.
    static void FlushBufferAndPrintProgress(const unsigned num_active_tasks, const unsigned num_queued_tasks);
};


// Represents the context of an asynchronous operation/task. Each operation has an
// associated HarvestableItem upon which the it is performed and a human-readable
// description of the (type of) operation itself.
struct TaskletContext {
    const HarvestableItem associated_item_;
    const std::string description_;

public:
    TaskletContext(const HarvestableItem &associated_item, const std::string &description)
        : associated_item_(associated_item), description_(description) { }
};


// Used to associate an asynchronous operation's context (TaskletContext)
// to the underlying thread that hosts it. A global key that is used to
// allocate memory in each executing thread's TLS (thread-local segment).
// The memory is used to store a pointer to the corresponding TaskletContext instance.
class TaskletContextManager {
    ::pthread_key_t tls_key_;

public:
    TaskletContextManager();
    ~TaskletContextManager();

    void setThreadLocalContext(const TaskletContext &context) const;
    TaskletContext *getThreadLocalContext() const;
};


extern const TaskletContextManager TASKLET_CONTEXT_MANAGER;


template <typename Parameter, typename Result>
class Future;


// Base class of all asynchronous operations. Provides an interface to spin up
// a new thread of execution and run arbitrary code on it. Tasklets are self-contained
// in that they host their own copy of inputs and outputs and maintain their own state.
template <typename Parameter, typename Result>
class Tasklet {
    friend class Future<Parameter, Result>;

public:
    // Determines how the final result is delivered to the user of the class.
    // YIELD causes the tasklet instance to relinquish ownership of the result to
    // the user, while COPY returns a copy of the computed result.
    enum ResultPolicy { YIELD, COPY };

private:
    enum Status { NOT_STARTED, RUNNING, COMPLETED_SUCCESS, COMPLETED_ERROR };


    // The thread routine that gets executed once the tasklet is started.
    // The single parameter points to the calling Tasklet instance. The user
    // of the Tasklet class must ensure that instance pointed to is valid
    // until the thread routine returns.
    static void *ThreadRoutine(Tasklet<Parameter, Result> * const parameter);


    TaskletContext context_;
    ::pthread_t thread_id_;
    mutable std::mutex mutex_;
    Status status_;

    // Incremented by one for the duration of the task.
    ThreadUtil::ThreadSafeCounter<unsigned> * const running_instance_counter_;

    // Functor that executes the actual payload code.
    std::function<void(const Parameter &, Result * const)> runnable_;

    std::unique_ptr<const Parameter> parameter_;
    std::unique_ptr<Result> result_;
    ResultPolicy result_policy_;

    // Some SFINAE/tag-dispatching magic to apply the constraint that the return
    // type must be copy constructable if the ResultPolicy is COPY.
    // For copy-constructable classes
    inline Result *getResultImpl(std::true_type) { return new Result(*result_); }

    // For non-copy-constructable classes
    inline Result *getResultImpl(std::false_type) { return nullptr; }

    inline void setStatus(const Status new_status) {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = new_status;
    }

public:
    Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const running_instance_counter, const HarvestableItem &associated_item,
            const std::string &description, const std::function<void(const Parameter &, Result * const)> &runnable,
            std::unique_ptr<Result> default_result, std::unique_ptr<Parameter> parameter, const ResultPolicy result_policy);
    virtual ~Tasklet();

    // Spins up a new thread and executes the payload.
    void start();

    inline std::string toString() const { return context_.description_; }
    inline ::pthread_t getID() const { return thread_id_; }
    inline Status getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_;
    }
    inline bool isComplete() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_ == Status::COMPLETED_SUCCESS or status_ == Status::COMPLETED_ERROR;
    }
    inline const HarvestableItem &getHarvestableItem() const { return context_.associated_item_; }
    inline const Parameter &getParameter() const { return *parameter_.get(); }

    // Returns the result based on the ResultPolicy. If the tasklet is complete, returns
    // immediately. Otherwise, blocks the calling thread until the tasklet has run to completion.
    std::unique_ptr<Result> getResult();

    // Blocks the calling thread until the task has run to completion.
    void await();
};


// Used for debugging.
extern ThreadUtil::ThreadSafeCounter<unsigned> tasklet_instance_counter;


template <typename Parameter, typename Result>
void *Tasklet<Parameter, Result>::ThreadRoutine(Tasklet<Parameter, Result> * const parameter) {
    Tasklet<Parameter, Result> * const tasklet(reinterpret_cast<Tasklet<Parameter, Result> *>(parameter));
    const auto thread_id(tasklet->thread_id_);
    assert(thread_id == ::pthread_self());
    SqlUtil::ThreadSafetyGuard sql_guard(SqlUtil::ThreadSafetyGuard::ThreadType::WORKER_THREAD);

    ::pthread_setname_np(thread_id, tasklet->context_.description_.c_str());
    // Store the tasklet context in the thread-local data segment.
    // We do not need to worry about cleaning it up as the context will
    // be automatically released when the tasklet gets destroyed.
    TASKLET_CONTEXT_MANAGER.setThreadLocalContext(tasklet->context_);
    // Register the tasklet context with the logger to track messages from this thread.
    ZoteroLogger::Get().registerTasklet(tasklet->getID(), tasklet->context_.associated_item_);
    ++(*tasklet->running_instance_counter_);

    Status completion_status(Status::COMPLETED_SUCCESS);
    try {
        tasklet->setStatus(Status::RUNNING);
        tasklet->runnable_(*tasklet->parameter_.get(), tasklet->result_.get());
    } catch (const std::runtime_error &exception) {
        LOG_WARNING("exception in tasklet '" + std::to_string(thread_id) + "': " + exception.what()
                    + "\ntasklet description: " + tasklet->context_.description_);
        completion_status = Status::COMPLETED_ERROR;
    } catch (...) {
        LOG_WARNING("unknown exception in tasklet '" + std::to_string(thread_id) + "'"
                    + "\ntasklet description: " + tasklet->context_.description_);
        completion_status = Status::COMPLETED_ERROR;
    }

    // Deregister the tasklet context and flush its log messages.
    ZoteroLogger::Get().deregisterTasklet(tasklet->getID(), tasklet->context_.associated_item_);
    --(*tasklet->running_instance_counter_);

    // Detach the thread so that its resources are automatically cleaned up.
    ::pthread_detach(thread_id);
    // Flagged at the very end of the routine to prevent data races.
    tasklet->setStatus(completion_status);

    return nullptr;
}


template <typename Parameter, typename Result>
Tasklet<Parameter, Result>::Tasklet(ThreadUtil::ThreadSafeCounter<unsigned> * const running_instance_counter,
                                    const HarvestableItem &associated_item, const std::string &description,
                                    const std::function<void(const Parameter &, Result * const)> &runnable,
                                    std::unique_ptr<Result> default_result, std::unique_ptr<Parameter> parameter,
                                    const ResultPolicy result_policy)
    : context_(associated_item, description), status_(Status::NOT_STARTED), running_instance_counter_(running_instance_counter),
      runnable_(runnable), parameter_(std::move(parameter)), result_(std::move(default_result)), result_policy_(result_policy) {
    ++tasklet_instance_counter;
}


template <typename Parameter, typename Result>
Tasklet<Parameter, Result>::~Tasklet() {
    if (not isComplete()) {
        LOG_WARNING("tasklet '" + std::to_string(thread_id_) + "' is still running!" + "\ndescription:" + context_.description_);

        if (::pthread_cancel(thread_id_) != 0)
            LOG_ERROR("failed to cancel tasklet thread '" + std::to_string(thread_id_) + "'!");
    }

    --tasklet_instance_counter;
}


template <typename Parameter, typename Result>
void Tasklet<Parameter, Result>::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != Status::NOT_STARTED) {
        LOG_ERROR("tasklet '" + std::to_string(thread_id_) + "' has already been started!" + "\nstatus = " + std::to_string(status_)
                  + "\ndescription:" + context_.description_);
    }

    auto thread_routine(reinterpret_cast<void *(*)(void *)>(ThreadRoutine));
    if (::pthread_create(&thread_id_, nullptr, thread_routine, this) != 0)
        LOG_ERROR("tasklet thread creation failed!\ntasklet description: " + context_.description_);
}


template <typename Parameter, typename Result>
std::unique_ptr<Result> Tasklet<Parameter, Result>::getResult() {
    await();

    std::lock_guard<std::mutex> lock(mutex_);

    if (status_ != Status::COMPLETED_SUCCESS) {
        LOG_ERROR("tasklet '" + std::to_string(thread_id_) + "' has no result!" + "\nstatus = " + std::to_string(status_)
                  + "\ndescription:" + context_.description_);
    }

    if (result_ == nullptr) {
        LOG_ERROR("tasklet '" + std::to_string(thread_id_) + "' has already yielded its result!"
                  + "\ndescription:" + context_.description_);
    }

    if (result_policy_ == ResultPolicy::YIELD)
        return std::move(result_);
    else {
        std::unique_ptr<Result> result_copy(getResultImpl(std::is_copy_constructible<Result>()));
        return std::move(result_copy);
    }
}


template <typename Parameter, typename Result>
void Tasklet<Parameter, Result>::await() {
    // Sleep/Wait until the task is complete.
    while (not isComplete())
        ::usleep(32 * 1000);
}


// Used for debugging.
extern ThreadUtil::ThreadSafeCounter<unsigned> future_instance_counter;


// Wrapper around a Tasklet that can be passed around in place of its result. Once the
// tasklet has run to completion, the Future can be used to retrieve its result.
template <typename Parameter, typename Result>
class Future {
    enum Status { WAITING, NO_RESULT, HAS_RESULT };


    std::shared_ptr<Util::Tasklet<Parameter, Result>> source_tasklet_;
    std::unique_ptr<Result> result_;
    Status status_;

public:
    Future(std::shared_ptr<Util::Tasklet<Parameter, Result>> source_tasklet);
    Future(std::unique_ptr<Result> result);
    ~Future();
    Future(const Future<Parameter, Result> &rhs) = delete;

    bool isComplete() const;

    // Returns false if the tasklet encountered an error, true otherwise.
    bool hasResult() const;

    // Blocks the calling thread until the task has run to completion.
    void await();

    // Returns the result of the tasklet. Will block if the task is still running.
    Result &getResult();

    inline const Parameter &getParameter() const { return source_tasklet_->getParameter(); }
    inline std::string toString() const { return source_tasklet_->toString(); }
};


template <typename Parameter, typename Result>
Future<Parameter, Result>::Future(std::shared_ptr<Util::Tasklet<Parameter, Result>> source_tasklet)
    : source_tasklet_(source_tasklet), status_(WAITING) {
    ++future_instance_counter;
}


template <typename Parameter, typename Result>
Future<Parameter, Result>::Future(std::unique_ptr<Result> result): result_(std::move(result)), status_(HAS_RESULT) {
    ++future_instance_counter;
}


template <typename Parameter, typename Result>
Future<Parameter, Result>::~Future() {
    --future_instance_counter;
}


template <typename Parameter, typename Result>
bool Future<Parameter, Result>::isComplete() const {
    if (status_ == Status::HAS_RESULT)
        return true;
    else
        switch (source_tasklet_->getStatus()) {
        case Util::Tasklet<Parameter, Result>::Status::COMPLETED_SUCCESS:
        case Util::Tasklet<Parameter, Result>::Status::COMPLETED_ERROR:
            return true;
        default:
            return false;
        }
}


template <typename Parameter, typename Result>
bool Future<Parameter, Result>::hasResult() const {
    if (status_ == Status::HAS_RESULT)
        return true;
    else
        switch (source_tasklet_->getStatus()) {
        case Util::Tasklet<Parameter, Result>::Status::COMPLETED_SUCCESS:
            return true;
        default:
            return false;
        }
}


template <typename Parameter, typename Result>
void Future<Parameter, Result>::await() {
    if (status_ == Status::WAITING) {
        source_tasklet_->await();
        if (hasResult()) {
            result_.reset(source_tasklet_->getResult().release());
            status_ = Status::HAS_RESULT;
        } else
            status_ = Status::NO_RESULT;
    }
}


template <typename Parameter, typename Result>
Result &Future<Parameter, Result>::getResult() {
    await();
    if (status_ != Status::HAS_RESULT)
        LOG_ERROR("no result for future bound to tasklet " + source_tasklet_->toString());

    return *result_;
}


// Tracks harvested records that have been uploaded to the BSZ server.
class UploadTracker {
    static constexpr unsigned CONNECTION_POOL_SIZE = 50;

public:
    enum DeliveryState : unsigned { AUTOMATIC, MANUAL, ERROR, IGNORE, RESET, ONLINE_FIRST };
    static const std::map<DeliveryState, std::string> DELIVERY_STATE_TO_STRING_MAP;
    static const std::map<std::string, DeliveryState> STRING_TO_DELIVERY_STATE_MAP;
    static const std::set<DeliveryState> DELIVERY_STATES_TO_RETRY;

    struct Entry {
        unsigned id_;
        std::string url_;
        std::string main_title_;
        unsigned zeder_id_;
        std::string zeder_instance_;
        DeliveryState delivery_state_;
        std::string error_message_;
        time_t delivered_at_;
        std::string delivered_at_str_;
        std::string hash_;

    public:
        std::string toString() const;
    };

private:
    mutable ThreadUtil::Semaphore connection_pool_semaphore_;

public:
    explicit UploadTracker(): connection_pool_semaphore_(CONNECTION_POOL_SIZE) { }

    bool urlAlreadyInDatabase(const std::string &url, const std::set<DeliveryState> &delivery_states_to_ignore = {},
                              Entry * const entry = nullptr) const;
    bool hashAlreadyInDatabase(const std::string &hash, const std::set<DeliveryState> &delivery_states_to_ignore = {},
                               std::vector<Entry> * const entries = nullptr) const;
    bool recordAlreadyInDatabase(const MARC::Record &record, const std::set<DeliveryState> &delivery_states_to_ignore = {},
                                 std::vector<Entry> * const entries = nullptr) const;

    bool journalHasRecordToRetry(const unsigned zeder_id, const Zeder::Flavour zeder_flavour) const;

    std::vector<Entry> getEntriesByZederIdAndFlavour(const unsigned zeder_id, const Zeder::Flavour zeder_flavour);

    // Returns when the last URL of the given journal was delivered to the BSZ. If found,
    // returns the timestamp of the last delivery, TimeUtil::BAD_TIME_T otherwise.
    time_t getLastUploadTime(const unsigned zeder_id, const Zeder::Flavour zeder_flavour) const;

    void registerZederJournal(const unsigned zeder_id, const std::string &zeder_instance, const std::string &journal_name);
    void deleteOnlineFirstEntriesOlderThan(const unsigned zeder_id, const std::string &zeder_instance, const unsigned &update_window);

    // Saves the record blob and its associated metadata in the host's database.
    // Returns true on success, false otherwise.
    bool archiveRecord(const MARC::Record &record, const DeliveryState delivery_state, const std::string &error_message = "");

    static std::string GetZederInstanceString(const Zeder::Flavour zeder_flavour);
    static std::string GetZederInstanceString(const std::string &group);

private:
    bool urlAlreadyInDatabase(const std::string &url, const std::set<DeliveryState> &delivery_states_to_ignore, Entry * const entry,
                              DbConnection * const db_connection) const;
    bool hashAlreadyInDatabase(const std::string &hash, const std::set<DeliveryState> &delivery_states_to_ignore,
                               std::vector<Entry> * const entries, DbConnection * const db_connection) const;
    bool recordAlreadyInDatabase(const std::string &record_hash, const std::set<std::string> &record_urls,
                                 const std::set<DeliveryState> &delivery_states_to_ignore, std::vector<Entry> * const entries,
                                 DbConnection * const db_connection) const;
};


// Returns URLs found in 856 and URL fields.
std::set<std::string> GetMarcRecordUrls(const MARC::Record &record);


} // end namespace Util


} // end namespace ZoteroHarvester
