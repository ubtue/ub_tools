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

#include "ZoteroHarvesterUtil.h"
#include <unistd.h>
#include "MiscUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "util.h"


namespace ZoteroHarvester {


namespace Util {


bool HarvestableItem::operator==(const HarvestableItem &rhs) const {
    return id_ == rhs.id_ and &journal_ == &rhs.journal_ and url_.toString() == rhs.url_.toString();
}


std::string HarvestableItem::toString() const {
    std::string as_string(std::to_string(id_));
    std::string journal_name(TextUtil::CollapseAndTrimWhitespace(journal_.name_));
    TextUtil::UnicodeTruncate(&journal_name, 20);

    as_string += " [" + journal_name + "...] | " + url_.toString();
    return as_string;
}


HarvestableItemManager::HarvestableItemManager(const std::vector<std::unique_ptr<Config::JournalParams>> &journal_params) {
    for (const auto &journal_param : journal_params)
        counters_.emplace(journal_param.get(), 0);
}


HarvestableItem HarvestableItemManager::newHarvestableItem(const std::string &url, const Config::JournalParams &journal_params) {
    auto journal_param_and_counter(counters_.find(&journal_params));
    if (journal_param_and_counter == counters_.end())
        LOG_ERROR("couldn't fetch harvestable item ID for unknown journal '" + journal_params.name_ + "'");

    return HarvestableItem(++journal_param_and_counter->second, url, journal_params);
}


ZoteroLogger::ContextData::ContextData(const Util::HarvestableItem &item)
    : item_(item)
{
    buffer_.reserve(static_cast<size_t>(BUFFER_SIZE));
    buffer_ = "\n\n";
}


void ZoteroLogger::queueContextMessage(const std::string &level, std::string msg, const TaskletContext &tasklet_context) {
    std::lock_guard<std::recursive_mutex> locker(active_context_mutex_);

    auto harvestable_item_and_context(active_contexts_.find(tasklet_context.associated_item_));
    if (harvestable_item_and_context == active_contexts_.end())
        error("message from unknown tasklet!");

    formatMessage(level, &msg);
    harvestable_item_and_context->second.buffer_ += msg;
}


void ZoteroLogger::queueGlobalMessage(const std::string &level, std::string msg) {
    std::lock_guard<std::recursive_mutex> locker(log_buffer_mutex_);

    formatMessage(level, &msg);
    log_buffer_.emplace_back(std::move(msg));
}


void ZoteroLogger::flushBufferAndPrintProgressImpl(const unsigned num_active_tasks, const unsigned num_queued_tasks) {
    std::lock_guard<std::recursive_mutex> locker(log_buffer_mutex_);

    if (::isatty(fd_) == 1) {
        // reset the progress bar
        if (not progress_bar_buffer_.empty()) {
            const std::string empty_string(progress_bar_buffer_.size(), ' ');
            writeToBackingLog("\r" + empty_string + "\r");
        }
    }

    // flush buffer
    while (not log_buffer_.empty()) {
        writeToBackingLog(log_buffer_.front());
        log_buffer_.pop_front();
    }

    if (::isatty(fd_) == 1) {
        // update progress bar
        progress_bar_buffer_ = "TASKS: ACTIVE = " + std::to_string(num_active_tasks) + ", QUEUED = "
                               + std::to_string(num_queued_tasks) + "\r";
        writeToBackingLog(progress_bar_buffer_);
    }
}


void ZoteroLogger::writeToBackingLog(const std::string &msg) {
    std::lock_guard<std::mutex> locker(mutex_);
    ::write(fd_, msg.data(), msg.length());
    ::fsync(fd_);
}


void ZoteroLogger::error(const std::string &msg) {
    // this is unrecoverable, so print out a preamble with Zotero related info before displaying
    // the actual error message and terminating the process

    const auto context(TASKLET_CONTEXT_MANAGER.getThreadLocalContext());
    if (context == nullptr)
        ::Logger::error(msg);       // pass-through

    std::string preamble;
    preamble += "ZOTERO debug info:\n";
    preamble += "\tparent tasklet: " + context->description_ + " (handle: " + std::to_string(::pthread_self()) + ")\n";
    preamble += "\titem: " + context->associated_item_.toString() + "\n\n";

    // flush the tasklet's buffer
    std::lock_guard<std::recursive_mutex> locker(active_context_mutex_);
    auto harvestable_item_and_context(active_contexts_.find(context->associated_item_));
    if (harvestable_item_and_context == active_contexts_.end())
        ::Logger::error("double-fault! message from unknown tasklet! original message:\n\n" + preamble + msg);
    else
        preamble += harvestable_item_and_context->second.buffer_;

    ::Logger::error(preamble + msg);
}


void ZoteroLogger::warning(const std::string &msg) {
    if (min_log_level_ < LL_WARNING)
        return;

    const auto context(TASKLET_CONTEXT_MANAGER.getThreadLocalContext());
    if (context == nullptr)
        queueGlobalMessage("WARN", msg);
    else
        queueContextMessage("WARN", msg, *context);
}


void ZoteroLogger::info(const std::string &msg) {
    if (min_log_level_ < LL_INFO)
        return;

    const auto context(TASKLET_CONTEXT_MANAGER.getThreadLocalContext());
    if (context == nullptr)
        queueGlobalMessage("INFO", msg);
    else
        queueContextMessage("INFO", msg, *context);
}


void ZoteroLogger::debug(const std::string &msg) {
    if ((min_log_level_ < LL_DEBUG) and (MiscUtil::SafeGetEnv("UTIL_LOG_DEBUG") != "true"))
        return;

    const auto context(TASKLET_CONTEXT_MANAGER.getThreadLocalContext());
    if (context == nullptr)
        queueGlobalMessage("DEBUG", msg);
    else
        queueContextMessage("DEBUG", msg, *context);
}


void ZoteroLogger::pushContext(const Util::HarvestableItem &context_item) {
    std::lock_guard<std::recursive_mutex> locker(active_context_mutex_);

    auto harvestable_item_and_context(active_contexts_.find(context_item));
    if (harvestable_item_and_context != active_contexts_.end())
        error("Harvestable item " + context_item.toString() + " already registered");

    active_contexts_.emplace(context_item, context_item);
}


void ZoteroLogger::popContext(const Util::HarvestableItem &context_item) {
    std::lock_guard<std::recursive_mutex> locker(active_context_mutex_);

    auto harvestable_item_and_context(active_contexts_.find(context_item));
    if (harvestable_item_and_context == active_contexts_.end())
        error("Harvestable " + context_item.toString() + " not registered");

    harvestable_item_and_context->second.buffer_ += "\n\n";
    {
        std::lock_guard<std::recursive_mutex> global_buffer_locker(log_buffer_mutex_);
        log_buffer_.emplace_back(std::move(harvestable_item_and_context->second.buffer_));
    }
    active_contexts_.erase(harvestable_item_and_context);
}


static bool zotero_logger_initialized(false);


void ZoteroLogger::Init() {
    delete logger;
    logger = new ZoteroLogger();
    zotero_logger_initialized = true;

    LOG_INFO("Zotero Logger initialized!\n\n\n");
}


void ZoteroLogger::FlushBufferAndPrintProgress(const unsigned num_active_tasks, const unsigned num_queued_tasks) {
    assert(zotero_logger_initialized == true);
    reinterpret_cast<ZoteroLogger *>(logger)->flushBufferAndPrintProgressImpl(num_active_tasks, num_queued_tasks);
}


TaskletContextManager::TaskletContextManager() {
    if (::pthread_key_create(&tls_key_, nullptr) != 0)
        LOG_ERROR("could not create tasklet context thread local key");
}


TaskletContextManager::~TaskletContextManager() {
    if (::pthread_key_delete(tls_key_) != 0)
        LOG_ERROR("could not delete tasklet context thread local key");
}


void TaskletContextManager::setThreadLocalContext(const TaskletContext &context) const {
    const auto tasklet_data(::pthread_getspecific(tls_key_));
    if (tasklet_data != nullptr)
        LOG_ERROR("tasklet local data already set for thread " + std::to_string(::pthread_self()));

    if (::pthread_setspecific(tls_key_, const_cast<TaskletContext *>(&context)) != 0)
        LOG_ERROR("could not set tasklet local data for thread " + std::to_string(::pthread_self()));
}


TaskletContext *TaskletContextManager::getThreadLocalContext() const {
    return reinterpret_cast<TaskletContext *>(::pthread_getspecific(tls_key_));
}


const TaskletContextManager TASKLET_CONTEXT_MANAGER;


ThreadUtil::ThreadSafeCounter<unsigned> tasklet_instance_counter;
ThreadUtil::ThreadSafeCounter<unsigned> future_instance_counter;


static void UpdateUploadTrackerEntryFromDbRow(const DbRow &row, UploadTracker::Entry * const entry) {
    if (row.empty())
        LOG_ERROR("Couldn't extract DeliveryTracker entry from empty DbRow");

    entry->url_ = row["url"];
    entry->delivered_at_ = SqlUtil::DatetimeToTimeT(row["delivered_at"]);
    entry->journal_name_ = row["journal_name"];
    entry->hash_ = row["hash"];
}


bool UploadTracker::urlAlreadyDelivered(const std::string &url, Entry * const entry) const {
    db_connection_->queryOrDie("SELECT url, delivered_at, journal_name, hash FROM delivered_marc_records WHERE url='"
                               + db_connection_->escapeString(SqlUtil::TruncateToVarCharMaxIndexLength(url)) + "'");
    auto result_set(db_connection_->getLastResultSet());
    if (result_set.empty())
        return false;

    const auto first_row(result_set.getNextRow());
    if (entry != nullptr)
        UpdateUploadTrackerEntryFromDbRow(first_row, entry);
    return true;
}


bool UploadTracker::hashAlreadyDelivered(const std::string &hash, Entry * const entry) const {
    db_connection_->queryOrDie("SELECT url, delivered_at, journal_name, hash FROM delivered_marc_records WHERE hash='"
                               + db_connection_->escapeString(hash) + "'");
    auto result_set(db_connection_->getLastResultSet());
    if (result_set.empty())
        return false;

    const auto first_row(result_set.getNextRow());
    if (entry != nullptr)
        UpdateUploadTrackerEntryFromDbRow(first_row, entry);
    return true;
}


size_t UploadTracker::listOutdatedJournals(const unsigned cutoff_days,
                                           std::unordered_map<std::string, time_t> * const outdated_journals) const
{
    db_connection_->queryOrDie("SELECT url, delivered_at, journal_name, hash FROM harvested_urls"
                               "WHERE last_harvest_time < DATEADD(day, -" + std::to_string(cutoff_days) + ", GETDATE())");
    auto result_set(db_connection_->getLastResultSet());
    Entry temp_entry;
    while (const DbRow row = result_set.getNextRow()) {
        UpdateUploadTrackerEntryFromDbRow(row, &temp_entry);

        auto journal_name_and_last_delivery_timestamp(outdated_journals->find(temp_entry.journal_name_));
        if (journal_name_and_last_delivery_timestamp != outdated_journals->end()) {
            // save the most recent timestamp
            if (journal_name_and_last_delivery_timestamp->second < temp_entry.delivered_at_)
               journal_name_and_last_delivery_timestamp->second = temp_entry.delivered_at_;
        } else
            (*outdated_journals)[temp_entry.journal_name_] = temp_entry.delivered_at_;
    }

    return outdated_journals->size();
}


time_t UploadTracker::getLastUploadTime(const std::string &journal_name) const {
    db_connection_->queryOrDie("SELECT delivered_at FROM delivered_marc_records WHERE journal_name='" +
                                db_connection_->escapeString(journal_name) + "' ORDER BY delivered_at DESC");
    auto result_set(db_connection_->getLastResultSet());
    if (result_set.empty())
        return TimeUtil::BAD_TIME_T;

    return SqlUtil::DatetimeToTimeT(result_set.getNextRow()["delivered_at"]);
}


std::recursive_mutex non_threadsafe_locale_modification_guard;


} // end namespace Util


} // end namespace ZoteroHarvester
