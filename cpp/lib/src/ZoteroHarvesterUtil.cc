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

#include "ZoteroHarvesterUtil.h"
#include <csignal>
#include <unistd.h>
#include "GzStream.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "TimeUtil.h"
#include "ZoteroHarvesterConversion.h"
#include "ZoteroHarvesterZederInterop.h"
#include "util.h"


namespace ZoteroHarvester {


namespace Util {


bool HarvestableItem::operator==(const HarvestableItem &rhs) const {
    return id_ == rhs.id_ and &journal_ == &rhs.journal_ and url_.toString() == rhs.url_.toString();
}


std::string HarvestableItem::toString() const {
    std::string as_string(std::to_string(id_));
    std::string journal_name(TextUtil::CollapseAndTrimWhitespace(journal_.name_));
    TextUtil::UTF8Truncate(&journal_name, 20);

    as_string += " [" + journal_name + "...] | " + url_.toString() + " {" + std::to_string(std::hash<HarvestableItem>()(*this)) + "}";
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


ZoteroLogger::ContextData::ContextData(const Util::HarvestableItem &item): item_(item) {
    buffer_.reserve(static_cast<size_t>(BUFFER_SIZE));
    buffer_ = "\n\n";
}


void ZoteroLogger::queueContextMessage(const std::string &level, std::string msg, const ::pthread_t tasklet_thread_id,
                                       const TaskletContext &tasklet_context) {
    std::lock_guard<std::recursive_mutex> locker(active_context_mutex_);
    if (fatal_error_all_stop_.load())
        return;

    auto harvestable_item_and_context(active_contexts_.find(std::make_pair(tasklet_thread_id, tasklet_context.associated_item_)));
    if (harvestable_item_and_context == active_contexts_.end()) {
        error("message from unknown tasklet!");
    }

    formatMessage(level, &msg);
    harvestable_item_and_context->second.buffer_ += msg;
}


void ZoteroLogger::queueGlobalMessage(const std::string &level, std::string msg) {
    std::lock_guard<std::recursive_mutex> locker(log_buffer_mutex_);
    if (fatal_error_all_stop_.load())
        return;

    formatMessage(level, &msg);
    log_buffer_.emplace_back(std::move(msg));
}


void ZoteroLogger::flushBufferAndPrintProgressImpl(const unsigned num_active_tasks, const unsigned num_queued_tasks) {
    std::lock_guard<std::recursive_mutex> locker(log_buffer_mutex_);
    if (fatal_error_all_stop_.load())
        return;

    if (::isatty(getFileDescriptor()) == 1) {
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

    if (::isatty(getFileDescriptor()) == 1) {
        // update progress bar
        progress_bar_buffer_ =
            "TASKS: ACTIVE = " + std::to_string(num_active_tasks) + ", QUEUED = " + std::to_string(num_queued_tasks) + "\r";
        writeToBackingLog(progress_bar_buffer_);
    }
}


void ZoteroLogger::writeToBackingLog(const std::string &msg) {
    std::lock_guard<std::mutex> locker(mutex_);
    if (::write(getFileDescriptor(), msg.data(), msg.length()) != static_cast<ssize_t>(msg.length()))
        LOG_ERROR("write(2) failed!");
    ::fsync(getFileDescriptor());
}


void ZoteroLogger::error(const std::string &msg) {
    // This is unrecoverable, so flush the global buffer and print out a preamble
    // with Zotero related info before displaying the actual error message and terminating the process
    if (fatal_error_all_stop_.load()) {
        // Sleep indefinitely until the original call terminates the process.
        while (true)
            ::usleep(10 * 1000);
    }
    fatal_error_all_stop_.store(true);

    // Flush the global buffer
    {
        std::lock_guard<std::recursive_mutex> global_buffer_locker(log_buffer_mutex_);
        while (not log_buffer_.empty()) {
            writeToBackingLog(log_buffer_.front());
            log_buffer_.pop_front();
        }
    }

    writeToBackingLog(std::string(100, '=') + "\n");
    writeToBackingLog("!!! FATAL ERROR !!!\n");
    writeToBackingLog(std::string(100, '=') + "\n");

    const auto context(TASKLET_CONTEXT_MANAGER.getThreadLocalContext());
    std::string faulty_tasklet_buffer;

    // Flush all active contexts (except the faulting one)
    writeToBackingLog("Dumping active contexts...\n");
    writeToBackingLog(std::string(100, '=') + "\n");
    {
        std::lock_guard<std::recursive_mutex> context_locker(active_context_mutex_);
        for (auto &item_and_context : active_contexts_) {
            if (context != nullptr and context->associated_item_.operator==(item_and_context.first.second))
                faulty_tasklet_buffer.swap(item_and_context.second.buffer_);
            else {
                item_and_context.second.buffer_ += "\n\n";
                writeToBackingLog(item_and_context.second.buffer_);
            }
        }
    }
    writeToBackingLog(std::string(100, '=') + "\n");

    if (context == nullptr) {
        writeToBackingLog("Fatal error in main thread:\n");
        ::Logger::error(msg); // pass-through
    }

    // Flush the tasklet's buffer
    writeToBackingLog("Faulty Zotero tasklet:");
    writeToBackingLog("\tparent tasklet: " + context->description_ + " (handle: " + std::to_string(::pthread_self()) + ")\n");
    writeToBackingLog("\titem: " + context->associated_item_.toString() + "\n\n");
    writeToBackingLog(std::string(100, '=') + "\n");
    writeToBackingLog("Faulty Zotero tasklet log buffer:\n");
    writeToBackingLog(std::string(100, '=') + "\n");
    writeToBackingLog(faulty_tasklet_buffer);
    writeToBackingLog(std::string(100, '=') + "\n");

    // Write the final error message and terminate the process
    ::Logger::error(msg + "\n" + std::string(100, '='));
}


void ZoteroLogger::warning(const std::string &msg) {
    if (fatal_error_all_stop_.load())
        return;
    else if (min_log_level_ < LL_WARNING)
        return;

    const auto context(TASKLET_CONTEXT_MANAGER.getThreadLocalContext());
    const auto thread_id(::pthread_self());
    if (context == nullptr)
        queueGlobalMessage("WARN", msg);
    else
        queueContextMessage("WARN", msg, thread_id, *context);
}


void ZoteroLogger::info(const std::string &msg) {
    if (fatal_error_all_stop_.load())
        return;
    else if (min_log_level_ < LL_INFO)
        return;

    const auto context(TASKLET_CONTEXT_MANAGER.getThreadLocalContext());
    const auto thread_id(::pthread_self());
    if (context == nullptr)
        queueGlobalMessage("INFO", msg);
    else
        queueContextMessage("INFO", msg, thread_id, *context);
}


void ZoteroLogger::debug(const std::string &msg) {
    if (fatal_error_all_stop_.load())
        return;
    else if ((min_log_level_ < LL_DEBUG) and (MiscUtil::SafeGetEnv("UTIL_LOG_DEBUG") != "true"))
        return;

    const auto context(TASKLET_CONTEXT_MANAGER.getThreadLocalContext());
    const auto thread_id(::pthread_self());
    if (context == nullptr)
        queueGlobalMessage("DEBUG", msg);
    else
        queueContextMessage("DEBUG", msg, thread_id, *context);
}


void ZoteroLogger::registerTasklet(const ::pthread_t tasklet_thread_id, const HarvestableItem &associated_item) {
    std::lock_guard<std::recursive_mutex> locker(active_context_mutex_);
    if (fatal_error_all_stop_.load())
        return;

    auto context_key(std::make_pair(tasklet_thread_id, associated_item));
    auto harvestable_item_and_context(active_contexts_.find(context_key));
    if (harvestable_item_and_context != active_contexts_.end())
        error("Harvestable item " + associated_item.toString() + " (thread: " + std::to_string(tasklet_thread_id) + ") already registered");

    active_contexts_.emplace(context_key, associated_item);
}


void ZoteroLogger::deregisterTasklet(const ::pthread_t tasklet_thread_id, const HarvestableItem &associated_item) {
    std::lock_guard<std::recursive_mutex> locker(active_context_mutex_);
    if (fatal_error_all_stop_.load())
        return;

    auto harvestable_item_and_context(active_contexts_.find(std::make_pair(tasklet_thread_id, associated_item)));
    if (harvestable_item_and_context == active_contexts_.end())
        error("Harvestable " + associated_item.toString() + " (thread: " + std::to_string(tasklet_thread_id) + ") not registered");

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


ZoteroLogger &ZoteroLogger::Get() {
    assert(zotero_logger_initialized == true);
    return *reinterpret_cast<ZoteroLogger *>(logger);
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


class WaitOnSemaphore {
    ThreadUtil::Semaphore * const semaphore_;

public:
    explicit WaitOnSemaphore(ThreadUtil::Semaphore * const semaphore): semaphore_(semaphore) { semaphore_->wait(); }
    ~WaitOnSemaphore() { semaphore_->post(); }
};


const std::map<UploadTracker::DeliveryState, std::string> UploadTracker::DELIVERY_STATE_TO_STRING_MAP{
    { AUTOMATIC, "automatic" }, { MANUAL, "manual" }, { ERROR, "error" },
    { IGNORE, "ignore" },       { RESET, "reset" },   { ONLINE_FIRST, "online_first" },
};


const std::map<std::string, UploadTracker::DeliveryState> UploadTracker::STRING_TO_DELIVERY_STATE_MAP{
    { "automatic", AUTOMATIC }, { "manual", MANUAL }, { "error", ERROR },
    { "ignore", IGNORE },       { "reset", RESET },   { "online_first", ONLINE_FIRST },
};


const std::set<UploadTracker::DeliveryState> UploadTracker::DELIVERY_STATES_TO_RETRY{ ERROR, RESET };


std::string UploadTracker::Entry::toString() const {
    std::string out;
    out += "\tid: " + std::to_string(id_) + "\n";
    out += "\turl: " + url_ + "\n";
    out += "\tdelivery_state: " + DELIVERY_STATE_TO_STRING_MAP.at(delivery_state_) + "\n";
    out += "\terror_message: " + error_message_ + "\n";
    out += "\tdelivered_at: " + delivered_at_str_ + "\n";
    out += "\tzeder id: " + std::to_string(zeder_id_) + "\n";
    out += "\tzeder instance: " + zeder_instance_ + "\n";
    out += "\tmain_title: " + main_title_ + "\n";
    out += "\thash: " + hash_ + "\n";
    return out;
}


static void UpdateUploadTrackerEntryFromDbRow(const DbRow &row, UploadTracker::Entry * const entry) {
    if (row.empty())
        LOG_ERROR("Couldn't extract DeliveryTracker entry from empty DbRow");

    entry->id_ = StringUtil::ToUnsigned(row["entry_id"]);
    entry->url_ = row["url"];
    entry->delivery_state_ = UploadTracker::STRING_TO_DELIVERY_STATE_MAP.at(row["delivery_state"]);
    entry->error_message_ = row["error_message"];
    entry->delivered_at_str_ = row["delivered_at"];
    entry->delivered_at_ = SqlUtil::DatetimeToTimeT(entry->delivered_at_str_);
    entry->zeder_id_ = StringUtil::ToUnsigned(row["zeder_id"]);
    entry->zeder_instance_ = row["zeder_instance"];
    entry->main_title_ = row["main_title"];
    entry->hash_ = row["hash"];
}


std::string GetDeliveryStatesSubquery(const std::set<UploadTracker::DeliveryState> &delivery_states, DbConnection * const db_connection) {
    std::vector<std::string> delivery_states_as_strings;
    for (const auto delivery_state : delivery_states)
        delivery_states_as_strings.emplace_back(UploadTracker::DELIVERY_STATE_TO_STRING_MAP.at(delivery_state));

    return db_connection->joinAndEscapeAndQuoteStrings(delivery_states_as_strings);
}


bool UploadTracker::urlAlreadyInDatabase(const std::string &url, const std::set<DeliveryState> &delivery_states_to_ignore,
                                         Entry * const entry, DbConnection * const db_connection) const {
    std::string query(
        "SELECT dmru.url, dmr.delivered_at, dmr.delivery_state, dmr.error_message, dmr.id AS entry_id, zj.zeder_id, zj.zeder_instance, "
        "dmr.main_title, dmr.hash "
        "FROM delivered_marc_records_urls AS dmru "
        "LEFT JOIN delivered_marc_records AS dmr ON dmru.record_id = dmr.id "
        "LEFT JOIN zeder_journals AS zj ON dmr.zeder_journal_id = zj.id ");

    // We use the content of 856 for tracking which is not necessarily the same url that has been used for downloading
    // Thus if detect a DOI scheme in the URL we relax the precise match to a DOI match
    if (MiscUtil::ContainsDOI(url))
        query += "WHERE dmru.url LIKE '%" + MiscUtil::extractDOI(url);
    else
        query += "WHERE dmru.url = '" + db_connection->escapeString(SqlUtil::TruncateToVarCharMaxIndexLength(url)) + "'";

    if (not delivery_states_to_ignore.empty())
        query += " AND dmr.delivery_state NOT IN (" + GetDeliveryStatesSubquery(delivery_states_to_ignore, db_connection) + ")";

    db_connection->queryOrDie(query);
    auto result_set(db_connection->getLastResultSet());
    if (result_set.empty())
        return false;

    const auto first_row(result_set.getNextRow());
    if (entry != nullptr)
        UpdateUploadTrackerEntryFromDbRow(first_row, entry);
    return true;
}


bool UploadTracker::hashAlreadyInDatabase(const std::string &hash, const std::set<DeliveryState> &delivery_states_to_ignore,
                                          std::vector<Entry> * const entries, DbConnection * const db_connection) const {
    std::string query(
        "SELECT dmru.url, dmr.delivered_at, dmr.delivery_state, dmr.error_message, dmr.id AS entry_id, zj.zeder_id, zj.zeder_instance, "
        "dmr.main_title, dmr.hash "
        "FROM delivered_marc_records_urls AS dmru "
        "LEFT JOIN delivered_marc_records AS dmr ON dmru.record_id = dmr.id "
        "LEFT JOIN zeder_journals AS zj ON dmr.zeder_journal_id = zj.id "
        "WHERE dmr.hash ="
        + db_connection->escapeAndQuoteString(hash));

    if (not delivery_states_to_ignore.empty())
        query += " AND dmr.delivery_state NOT IN (" + GetDeliveryStatesSubquery(delivery_states_to_ignore, db_connection) + ")";

    db_connection->queryOrDie(query);
    auto result_set(db_connection->getLastResultSet());
    if (result_set.empty())
        return false;

    if (entries == nullptr)
        return true;

    Entry buffer;
    while (const DbRow row = result_set.getNextRow()) {
        UpdateUploadTrackerEntryFromDbRow(row, &buffer);
        entries->emplace_back(buffer);
    }

    return true;
}


bool UploadTracker::recordAlreadyInDatabase(const std::string &record_hash, const std::set<std::string> &record_urls,
                                            const std::set<DeliveryState> &delivery_states_to_ignore, std::vector<Entry> * const entries,
                                            DbConnection * const db_connection) const {
    Entry buffer;
    bool already_in_database(false);
    for (const auto &url : record_urls) {
        if (urlAlreadyInDatabase(url, delivery_states_to_ignore, &buffer, db_connection)) {
            if (buffer.hash_ != record_hash) {
                LOG_INFO("record with URL '" + url + "' already in database but with a different hash");
                LOG_DEBUG("\tcurrent hash: " + record_hash);
                LOG_DEBUG("\t" + buffer.toString());
            } else
                LOG_INFO("record with URL '" + url + "' already in database with the same hash (" + record_hash + ")");

            already_in_database = true;
            if (entries != nullptr)
                entries->emplace_back(buffer);
            break;
        }
    }

    if (not already_in_database) {
        std::vector<Entry> hash_bucket;
        if (hashAlreadyInDatabase(record_hash, delivery_states_to_ignore, &hash_bucket, db_connection)) {
            if (hash_bucket.size() > 1) {
                LOG_WARNING("multiple records were already in database with the same hash (" + record_hash + ")!");
                for (const auto &entry : hash_bucket)
                    LOG_DEBUG(entry.toString());
            } else
                LOG_INFO("record with URL '" + hash_bucket[0].url_ + "' already in database with the same hash (" + record_hash + ")");

            already_in_database = true;
            if (entries != nullptr)
                *entries = hash_bucket;
        }
    }

    return already_in_database;
}


bool UploadTracker::journalHasRecordToRetry(const unsigned zeder_id, const Zeder::Flavour zeder_flavour) const {
    WaitOnSemaphore lock(&connection_pool_semaphore_);
    DbConnection db_connection(DbConnection::UBToolsFactory());

    const std::string zeder_instance(GetZederInstanceString(zeder_flavour));
    std::string delivery_states_subquery("(");
    for (const auto delivery_state : DELIVERY_STATES_TO_RETRY) {
        if (delivery_states_subquery != "(")
            delivery_states_subquery += ",";
        delivery_states_subquery += db_connection.escapeAndQuoteString(DELIVERY_STATE_TO_STRING_MAP.at(delivery_state));
    }
    delivery_states_subquery += ")";

    db_connection.queryOrDie("SELECT count(*) AS counted_records FROM delivered_marc_records "
                             "LEFT JOIN zeder_journals ON zeder_journals.id=delivered_marc_records.zeder_journal_id "
                             "WHERE zeder_journals.zeder_id=" + db_connection.escapeAndQuoteString(std::to_string(zeder_id)) + " "
                             "AND zeder_journals.zeder_instance=" + db_connection.escapeAndQuoteString(zeder_instance) + " "
                             "AND delivery_state IN " + delivery_states_subquery);

    return *db_connection.getLastResultSet().getColumnSet("counted_records").begin() != "0";
}


bool UploadTracker::urlAlreadyInDatabase(const std::string &url, const std::set<DeliveryState> &delivery_states_to_ignore,
                                         Entry * const entry) const {
    WaitOnSemaphore lock(&connection_pool_semaphore_);
    DbConnection db_connection(DbConnection::UBToolsFactory());

    return urlAlreadyInDatabase(url, delivery_states_to_ignore, entry, &db_connection);
}


bool UploadTracker::hashAlreadyInDatabase(const std::string &hash, const std::set<DeliveryState> &delivery_states_to_ignore,
                                          std::vector<Entry> * const entries) const {
    WaitOnSemaphore lock(&connection_pool_semaphore_);
    DbConnection db_connection(DbConnection::UBToolsFactory());

    return hashAlreadyInDatabase(hash, delivery_states_to_ignore, entries, &db_connection);
}


bool UploadTracker::recordAlreadyInDatabase(const MARC::Record &record, const std::set<DeliveryState> &delivery_states_to_ignore,
                                            std::vector<Entry> * const entries) const {
    WaitOnSemaphore lock(&connection_pool_semaphore_);
    DbConnection db_connection(DbConnection::UBToolsFactory());

    const auto hash(Conversion::CalculateMarcRecordHash(record));
    const auto urls(GetMarcRecordUrls(record));

    return recordAlreadyInDatabase(hash, urls, delivery_states_to_ignore, entries, &db_connection);
}


time_t UploadTracker::getLastUploadTime(const unsigned zeder_id, const Zeder::Flavour zeder_flavour) const {
    WaitOnSemaphore lock(&connection_pool_semaphore_);
    DbConnection db_connection(DbConnection::UBToolsFactory());

    const std::string zeder_instance(GetZederInstanceString(zeder_flavour));
    db_connection.queryOrDie("SELECT MAX(delivered_at) AS max_delivered_at FROM delivered_marc_records "
                             "LEFT JOIN zeder_journals ON delivered_marc_records.zeder_journal_id = zeder_journals.id "
                             "WHERE zeder_id=" + std::to_string(zeder_id) + " "
                             "AND zeder_instance=" + db_connection.escapeAndQuoteString(zeder_instance));
    auto result_set(db_connection.getLastResultSet());
    if (result_set.empty())
        return TimeUtil::BAD_TIME_T;

    const auto max_delivered_at(result_set.getNextRow()["max_delivered_at"]);
    if (max_delivered_at.empty())
        return TimeUtil::BAD_TIME_T;

    return SqlUtil::DatetimeToTimeT(max_delivered_at);
}


std::vector<UploadTracker::Entry> GetEntriesFromLastResultSet(DbConnection * const db_connection) {
    auto result_set(db_connection->getLastResultSet());
    std::vector<UploadTracker::Entry> entries;
    UploadTracker::Entry entry;
    while (const DbRow row = result_set.getNextRow()) {
        UpdateUploadTrackerEntryFromDbRow(row, &entry);
        entries.emplace_back(entry);
    }
    return entries;
}


std::vector<UploadTracker::Entry> UploadTracker::getEntriesByZederIdAndFlavour(const unsigned zeder_id,
                                                                               const Zeder::Flavour zeder_flavour) {
    WaitOnSemaphore lock(&connection_pool_semaphore_);
    DbConnection db_connection(DbConnection::UBToolsFactory());

    const std::string zeder_instance(GetZederInstanceString(zeder_flavour));
    db_connection.queryOrDie("SELECT dmru.url, dmr.delivered_at, zj.zeder_id, zj.zeder_instance, dmr.main_title, dmr.hash, "
                             "dmr.delivery_state, dmr.error_message, dmr.id AS entry_id "
                             "FROM delivered_marc_records_urls AS dmru "
                             "LEFT JOIN delivered_marc_records AS dmr ON dmru.record_id = dmr.id "
                             "LEFT JOIN zeder_journals AS zj ON dmr.zeder_journal_id = zj.id "
                             "WHERE zj.zeder_id=" + db_connection.escapeString(std::to_string(zeder_id)) + " "
                             "AND zj.zeder_instance=" + db_connection.escapeAndQuoteString(zeder_instance) + " "
                             "ORDER BY dmr.delivered_at, dmr.id ASC");

    return GetEntriesFromLastResultSet(&db_connection);
}


bool UploadTracker::archiveRecord(const MARC::Record &record, const DeliveryState delivery_state, const std::string &error_message) {
    WaitOnSemaphore lock(&connection_pool_semaphore_);
    DbConnection db_connection(DbConnection::UBToolsFactory());

    const auto hash(Conversion::CalculateMarcRecordHash(record));
    const auto main_title(record.getMainTitle());
    const auto urls(GetMarcRecordUrls(record));

    // Try to find an existing record & update it, if possible
    std::vector<Entry> entries;
    if (recordAlreadyInDatabase(hash, urls, /*delivery_states_to_ignore = */ {}, &entries, &db_connection)) {
        for (const auto &entry : entries) {
            if (DELIVERY_STATES_TO_RETRY.find(entry.delivery_state_) == DELIVERY_STATES_TO_RETRY.end())
                return false;

            db_connection.queryOrDie("UPDATE delivered_marc_records "
                                     "SET hash=" + db_connection.escapeAndQuoteString(hash) +
                                     ",delivery_state=" + db_connection.escapeAndQuoteString(DELIVERY_STATE_TO_STRING_MAP.at(delivery_state)) +
                                     ",error_message=" + db_connection.escapeAndQuoteNonEmptyStringOrReturnNull(error_message) +
                                     ",delivered_at=NOW()"
                                     ",main_title=" + db_connection.escapeAndQuoteString(SqlUtil::TruncateToVarCharMaxIndexLength(main_title)) +
                                     ",record=" + db_connection.escapeAndQuoteString(GzStream::CompressString(record.toBinaryString(), GzStream::GZIP)) +
                                     " WHERE id=" + db_connection.escapeAndQuoteString(std::to_string(entry.id_)));

            db_connection.queryOrDie("DELETE FROM delivered_marc_records_urls "
                                     "WHERE record_id=" + db_connection.escapeAndQuoteString(std::to_string(entry.id_)) + " "
                                     "AND url NOT IN (" + db_connection.joinAndEscapeAndQuoteStrings(urls) + ")");

            for (const auto &url : urls) {
                db_connection.queryOrDie("INSERT IGNORE INTO delivered_marc_records_urls (record_id, url) VALUES ("
                                         + db_connection.escapeAndQuoteString(db_connection.escapeAndQuoteString(std::to_string(entry.id_)))
                                         + "," + db_connection.escapeAndQuoteString(url) + ")");
            }
            return true;
        }
    }

    const auto zeder_id(record.getFirstSubfieldValue("ZID", 'a'));
    const auto zeder_instance(GetZederInstanceString(ZederInterop::GetZederInstanceFromMarcRecord(record)));
    db_connection.queryOrDie("INSERT INTO delivered_marc_records "
                             "SET zeder_journal_id=(SELECT id FROM zeder_journals WHERE "
                             "zeder_id=" + db_connection.escapeAndQuoteString(zeder_id) + " "
                             "AND zeder_instance=" + db_connection.escapeAndQuoteString(zeder_instance) + ")"
                             ",hash=" + db_connection.escapeAndQuoteString(hash) +
                             ",delivery_state=" + db_connection.escapeAndQuoteString(DELIVERY_STATE_TO_STRING_MAP.at(delivery_state)) +
                             ",error_message=" + db_connection.escapeAndQuoteNonEmptyStringOrReturnNull(error_message) +
                             ",main_title=" + db_connection.escapeAndQuoteString(SqlUtil::TruncateToVarCharMaxIndexLength(main_title)) +
                             ",record=" + db_connection.escapeAndQuoteString(GzStream::CompressString(record.toBinaryString(), GzStream::GZIP)));

    // Fetch the last inserted row's ID to add the URLs
    db_connection.queryOrDie("SELECT LAST_INSERT_ID() id");
    auto result_set(db_connection.getLastResultSet());
    if (result_set.empty())
        LOG_ERROR("couldn't query last insert id from delivered_marc_records!");
    const auto last_insert_id(result_set.getNextRow()["id"]);

    for (const auto &url : urls) {
        db_connection.queryOrDie("INSERT INTO delivered_marc_records_urls SET record_id=" + last_insert_id
                                 + ", url=" + db_connection.escapeAndQuoteString(SqlUtil::TruncateToVarCharMaxIndexLength(url)));
    }

    return true;
}


std::string UploadTracker::GetZederInstanceString(const Zeder::Flavour zeder_flavour) {
    // These strings need to be updated in the SQL schema as well.
    switch (zeder_flavour) {
    case Zeder::Flavour::IXTHEO:
        return "ixtheo";
    case Zeder::Flavour::KRIMDOK:
        return "krimdok";
    default:
        LOG_ERROR("unknown zeder flavour '" + std::to_string(zeder_flavour) + "'");
    }
}


std::string UploadTracker::GetZederInstanceString(const std::string &group) {
    if (group == "IxTheo" or group == "RelBib")
        return "ixtheo";
    else if (group == "KrimDok")
        return "krimdok";
    LOG_ERROR("could not determine zeder instance for group: " + group);
}


void UploadTracker::registerZederJournal(const unsigned zeder_id, const std::string &zeder_instance, const std::string &journal_name) {
    WaitOnSemaphore lock(&connection_pool_semaphore_);
    DbConnection db_connection(DbConnection::UBToolsFactory());

    // intentionally use INSERT INTO with ON DUPLICATE KEY UPDATE instead of REPLACE INTO
    // (we do NOT want the auto increment index to change if title hasn't changed)
    db_connection.queryOrDie("INSERT INTO zeder_journals (zeder_id, zeder_instance, journal_name) VALUES ("
                             + db_connection.escapeAndQuoteString(std::to_string(zeder_id)) + ", "
                             + db_connection.escapeAndQuoteString(zeder_instance) + ", " + db_connection.escapeAndQuoteString(journal_name)
                             + ") " + "ON DUPLICATE KEY UPDATE journal_name=" + db_connection.escapeAndQuoteString(journal_name));
}


void UploadTracker::deleteOnlineFirstEntriesOlderThan(const unsigned zeder_id, const std::string &zeder_instance,
                                                      const unsigned &update_window) {
    WaitOnSemaphore lock(&connection_pool_semaphore_);
    DbConnection db_connection(DbConnection::UBToolsFactory());
    db_connection.queryOrDie(std::string("DELETE FROM delivered_marc_records WHERE zeder_journal_id=")
                             + "(SELECT id FROM zeder_journals WHERE zeder_id="
                             + db_connection.escapeAndQuoteString(std::to_string(zeder_id))
                             + " AND zeder_instance=" + db_connection.escapeAndQuoteString(zeder_instance) + ')'
                             + " AND delivery_state=" + db_connection.escapeAndQuoteString("online_first")
                             + " AND delivered_at < (SUBDATE(NOW(), " + std::to_string(update_window) + "))");
}


std::set<std::string> GetMarcRecordUrls(const MARC::Record &record) {
    std::set<std::string> urls;

    for (const auto &field : record.getTagRange("856")) {
        const auto url(field.getFirstSubfieldWithCode('u'));
        if (not url.empty())
            urls.emplace(url);
    }

    const auto harvest_url_field(record.findTag("URL"));
    if (harvest_url_field != record.end())
        urls.emplace(harvest_url_field->getFirstSubfieldWithCode('a'));

    return urls;
}


} // end namespace Util


} // end namespace ZoteroHarvester
