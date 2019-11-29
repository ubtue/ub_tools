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

#include "MiscUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "ZoteroHarvesterUtil.h"
#include "util.h"


namespace ZoteroHarvester {


namespace Util {


bool HarvestableItem::operator==(const HarvestableItem &rhs) const {
    return id_ == rhs.id_ and &journal_ == &rhs.journal_ and url_.toString() == rhs.url_.toString();
}


std::string HarvestableItem::toString() const {
    std::string out(std::to_string(id_));
    std::string journal_name(TextUtil::CollapseAndTrimWhitespace(journal_.name_));
    TextUtil::UnicodeTruncate(&journal_name, 20);

    out += " [" + journal_name + "...] | " + url_.toString();
    return out;
}


HarvestableItemManager::HarvestableItemManager(const std::vector<std::unique_ptr<Config::JournalParams>> &journal_params) {
    for (const auto &journal_param : journal_params)
        counters_.emplace(journal_param.get(), 0);
}


HarvestableItem HarvestableItemManager::newHarvestableItem(const std::string &url, const Config::JournalParams &journal_params) {
    auto match(counters_.find(&journal_params));
    if (match == counters_.end())
        LOG_ERROR("couldn't fetch harvestable item ID for unknown journal '" + journal_params.name_ + "'");

    return HarvestableItem(++match->second, url, journal_params);
}


void ZoteroLogger::queueMessage(const std::string &level, std::string msg, const TaskletContext &tasklet_context) {
    std::lock_guard<std::recursive_mutex> locker(active_context_mutex_);

    auto match(active_contexts_.find(tasklet_context.associated_item_));
    if (match == active_contexts_.end())
        error("message from unknown tasklet!");

    formatMessage(level, &msg);
    match->second.buffer_ += msg;
}


void ZoteroLogger::error(const std::string &msg) {
    // this is unrecoverable, so print out a preamble with Zotero related info before displaying
    // the actual error message and terminating the process

    const auto context(TASKLET_CONTEXT_MANAGER.getThreadLocalContext());
    if (context == nullptr)
        ::Logger::error(msg);       // pass-through

    std::string preamble;
    preamble += "ZOTERO debug info:\n";
    preamble += "\tparent tasklet: " + context->description_ + " (handle: " + std::to_string(pthread_self()) + ")\n";
    preamble += "\titem:\n";
    preamble += "\t\tid: " + std::to_string(context->associated_item_.id_) + "\n";
    preamble += "\t\tjournal: " + context->associated_item_.journal_.name_ + " ("
                + context->associated_item_.journal_.group_ + "|" + std::to_string(context->associated_item_.journal_.zeder_id_)
                + ")\n";
    preamble += "\t\turl: " + context->associated_item_.url_.toString() + "\n\n";

    // flush the tasklet's buffer
    std::lock_guard<std::recursive_mutex> locker(active_context_mutex_);
    auto match(active_contexts_.find(context->associated_item_));
    if (match == active_contexts_.end())
        ::Logger::error("double-fault! message from unknown tasklet! original message:\n\n" + preamble + msg);
    else
        preamble += match->second.buffer_;

    ::Logger::error(preamble + msg);
}


void ZoteroLogger::warning(const std::string &msg) {
    if (min_log_level_ < LL_WARNING)
        return;

    const auto context(TASKLET_CONTEXT_MANAGER.getThreadLocalContext());
    if (context == nullptr) {
        ::Logger::warning(msg);       // pass-through
        return;
    }

    queueMessage("WARN", msg, *context);
}


void ZoteroLogger::info(const std::string &msg) {
    if (min_log_level_ < LL_INFO)
        return;

    const auto context(TASKLET_CONTEXT_MANAGER.getThreadLocalContext());
    if (context == nullptr) {
        ::Logger::info(msg);       // pass-through
        return;
    }

    queueMessage("INFO", msg, *context);
}


void ZoteroLogger::debug(const std::string &msg) {
    if ((min_log_level_ < LL_DEBUG) and (MiscUtil::SafeGetEnv("UTIL_LOG_DEBUG") != "true"))
        return;

    const auto context(TASKLET_CONTEXT_MANAGER.getThreadLocalContext());
    if (context == nullptr) {
        ::Logger::debug(msg);       // pass-through
        return;
    }

    queueMessage("DEBUG", msg, *context);
}


void ZoteroLogger::pushContext(const Util::HarvestableItem &context_item) {
    std::lock_guard<std::recursive_mutex> locker(active_context_mutex_);

    auto match(active_contexts_.find(context_item));
    if (match != active_contexts_.end())
        error("Harvestable item " + context_item.toString() + " already registered");

    active_contexts_.emplace(context_item, context_item);
}


void ZoteroLogger::popContext(const Util::HarvestableItem &context_item) {
    std::lock_guard<std::recursive_mutex> locker(active_context_mutex_);

    auto match(active_contexts_.find(context_item));
    if (match == active_contexts_.end())
        error("Harvestable " + context_item.toString() + " not registered");

    // flush buffer contents and remove the context
    match->second.buffer_ += "\n\n";
    writeString("", match->second.buffer_, /* format_message = */ false);
    active_contexts_.erase(match);
}


void ZoteroLogger::Init() {
    delete logger;
    logger = new ZoteroLogger();

    LOG_INFO("Zotero Logger initialized!\n\n\n");
}


TaskletContextManager::TaskletContextManager() {
    if (pthread_key_create(&tls_key_, nullptr) != 0)
        LOG_ERROR("could not create tasklet context thread local key");
}


TaskletContextManager::~TaskletContextManager() {
    if (pthread_key_delete(tls_key_) != 0)
        LOG_ERROR("could not delete tasklet context thread local key");
}


void TaskletContextManager::setThreadLocalContext(const TaskletContext &context) const {
    const auto tasklet_data(pthread_getspecific(tls_key_));
    if (tasklet_data != nullptr)
        LOG_ERROR("tasklet local data already set for thread " + std::to_string(pthread_self()));

    if (pthread_setspecific(tls_key_, const_cast<TaskletContext *>(&context)) != 0)
        LOG_ERROR("could not set tasklet local data for thread " + std::to_string(pthread_self()));
}


TaskletContext *TaskletContextManager::getThreadLocalContext() const {
    return reinterpret_cast<TaskletContext *>(pthread_getspecific(tls_key_));
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
                               + db_connection_->escapeString(SqlUtil::TruncateToVarCharMaxLength(url)) + "'");
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

        auto match(outdated_journals->find(temp_entry.journal_name_));
        if (match != outdated_journals->end()) {
            // save the most recent timestamp
            if (match->second < temp_entry.delivered_at_)
               match->second = temp_entry.delivered_at_;
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


} // end namespace Util


} // end namespace ZoteroHarvester
