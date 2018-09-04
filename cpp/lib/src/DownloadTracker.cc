/*
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include "DownloadTracker.h"
#include "RegexMatcher.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include <memory>


inline static void UpdateDownloadTrackerEntryFromDbRow(const DbRow &row, DownloadTracker::Entry * const entry) {
    if (row.empty())
        LOG_ERROR("Couldn't extract DownloadTracker entry from empty DbRow");

    entry->url_ = row["url"];
    entry->last_harvest_time_ = SqlUtil::DatetimeToTimeT(row["last_harvest_time"]);
    entry->journal_name_ = row["journal_name"];
    entry->error_message_ = row["error_message"];
    entry->hash_ = row["checksum"];
}


inline static std::string DeliveryModeToSqlEnum(BSZUpload::DeliveryMode delivery_mode) {
    return StringUtil::ToLower(std::find_if(BSZUpload::STRING_TO_DELIVERY_MODE_MAP.begin(), BSZUpload::STRING_TO_DELIVERY_MODE_MAP.end(),
                                           [delivery_mode](const std::pair<std::string, int> &entry) -> bool { return static_cast<int>
                                           (delivery_mode) == entry.second; })->first);
}


inline static BSZUpload::DeliveryMode SqlEnumToDeliveryMode(const std::string &delivery_mode) {
    return static_cast<BSZUpload::DeliveryMode>(BSZUpload::STRING_TO_DELIVERY_MODE_MAP.at(StringUtil::ToUpper(delivery_mode)));
}


bool DownloadTracker::hasAlreadyBeenDownloaded(BSZUpload::DeliveryMode delivery_mode, const std::string &url, const std::string &hash, Entry * const entry) const
{
    if (unlikely(delivery_mode == BSZUpload::DeliveryMode::NONE))
        LOG_ERROR("delivery mode NONE not allowed for url '" + url + "'");

    db_connection_->queryOrDie("SELECT * FROM harvested_urls WHERE url='" + db_connection_->escapeString(url) + "' " +
                               "AND delivery_mode='" + DeliveryModeToSqlEnum(delivery_mode) + "'");
    auto result_set(db_connection_->getLastResultSet());
    if (result_set.empty())
        return false;

    const auto first_row(result_set.getNextRow());
    Entry temp_entry;
    UpdateDownloadTrackerEntryFromDbRow(first_row, &temp_entry);

    if (hash != "" and hash != temp_entry.hash_)
        return false;

    UpdateDownloadTrackerEntryFromDbRow(first_row, entry);
    return true;
}


void DownloadTracker::addOrReplace(BSZUpload::DeliveryMode delivery_mode, const std::string &url, const std::string &journal_name,
                                   const std::string &hash, const std::string &error_message)
{
    if (unlikely((hash.empty() and error_message.empty()) or (not hash.empty() and not error_message.empty())))
        LOG_ERROR("exactly one of \"hash\" and \"error_message\" must be non-empty!");
    else if (unlikely(delivery_mode == BSZUpload::DeliveryMode::NONE))
        LOG_ERROR("delivery mode NONE not allowed for url '" + url + "'");

    const time_t now(std::time(nullptr));
    const auto timestamp(SqlUtil::TimeTToDatetime(now));

    db_connection_->queryOrDie("SELECT * FROM harvested_urls WHERE url='" + db_connection_->escapeString(url) + "' " +
                               "AND delivery_mode='" + DeliveryModeToSqlEnum(delivery_mode) + "'");
    auto result_set(db_connection_->getLastResultSet());

    if (result_set.empty()) {
        db_connection_->queryOrDie("INSERT INTO harvested_urls SET url='" + db_connection_->escapeString(url) + "',"
                                   "last_harvest_time='" + timestamp + "'," +
                                   "journal_name='" + db_connection_->escapeString(journal_name) + "'," +
                                   "checksum='" + hash + "'," +
                                   "error_message='" + db_connection_->escapeString(error_message) + "'," +
                                   "delivery_mode = '" + DeliveryModeToSqlEnum(delivery_mode) + "'");
    } else {
        db_connection_->queryOrDie("UPDATE harvested_urls SET last_harvest_time='" + timestamp + "'," +
                                   "checksum='" + hash + "'," +
                                   "error_message='" + db_connection_->escapeString(error_message) + "' " +
                                   "WHERE id=" + result_set.getNextRow()["id"]);
    }
}


size_t DownloadTracker::listMatches(BSZUpload::DeliveryMode delivery_mode, const std::string &url_regex, std::vector<Entry> * const entries) const {
    if (unlikely(delivery_mode == BSZUpload::DeliveryMode::NONE))
        LOG_ERROR("delivery mode NONE not allowed");

    std::unique_ptr<RegexMatcher> matcher(RegexMatcher::RegexMatcherFactoryOrDie(url_regex));

    entries->clear();
    db_connection_->queryOrDie("SELECT * FROM harvested_urls WHERE delivery_mode='" + DeliveryModeToSqlEnum(delivery_mode) + "'");
    auto result_set(db_connection_->getLastResultSet());

    while (const DbRow row = result_set.getNextRow()) {
        Entry retrieved_entry;
        UpdateDownloadTrackerEntryFromDbRow(row, &retrieved_entry);
        if (matcher->matched(retrieved_entry.url_))
            entries->push_back(retrieved_entry);
    }

    return entries->size();
}


// Helper function for DownloadTracker::deleteMatches and DownloadTracker::deleteOldEntries.
template<typename Predicate> static size_t DeleteEntries(BSZUpload::DeliveryMode delivery_mode, DbConnection * const db_connection,
                                                         const Predicate &deletion_predicate)
{
    db_connection->queryOrDie("SELECT * FROM harvested_urls WHERE delivery_mode='" + DeliveryModeToSqlEnum(delivery_mode) + "'");
    auto result_set(db_connection->getLastResultSet());

    std::vector<std::string> deleted_ids;
    while (const DbRow row = result_set.getNextRow()) {
        DownloadTracker::Entry retrieved_entry;
        UpdateDownloadTrackerEntryFromDbRow(row, &retrieved_entry);
        if (deletion_predicate(retrieved_entry.url_, retrieved_entry.last_harvest_time_))
            deleted_ids.emplace_back(row["id"]);
    }

    for (const auto &deleted_id : deleted_ids)
        db_connection->queryOrDie("DELETE FROM harvested_urls where id=" + deleted_id);

    return deleted_ids.size();
}


size_t DownloadTracker::deleteMatches(BSZUpload::DeliveryMode delivery_mode, const std::string &url_regex) {
    if (unlikely(delivery_mode == BSZUpload::DeliveryMode::NONE))
        LOG_ERROR("delivery mode NONE not allowed");

    std::shared_ptr<RegexMatcher> matcher(RegexMatcher::RegexMatcherFactoryOrDie(url_regex));
    return DeleteEntries(delivery_mode, db_connection_,
                         [matcher](const std::string &url, const time_t /*last_harvest_time*/){ return matcher->matched(url); });
}


size_t DownloadTracker::deleteSingleEntry(BSZUpload::DeliveryMode delivery_mode, const std::string &url) {
    if (unlikely(delivery_mode == BSZUpload::DeliveryMode::NONE))
        LOG_ERROR("delivery mode NONE not allowed for url '" + url + "'");

    db_connection_->queryOrDie("SELECT * FROM harvested_urls WHERE url='" + db_connection_->escapeString(url) + "' " +
                               "AND delivery_mode='" + DeliveryModeToSqlEnum(delivery_mode) + "'");
    auto result_set(db_connection_->getLastResultSet());
    if (result_set.empty())
        return 0;
    else
        return 1;
}


size_t DownloadTracker::deleteOldEntries(BSZUpload::DeliveryMode delivery_mode, const time_t cutoff_timestamp) {
    if (unlikely(delivery_mode == BSZUpload::DeliveryMode::NONE))
        LOG_ERROR("delivery mode NONE not allowed");

    return DeleteEntries(delivery_mode, db_connection_,
                         [cutoff_timestamp](const std::string &/*url*/, const time_t last_harvest_time)
                             { return last_harvest_time <= cutoff_timestamp; });
}


size_t DownloadTracker::clear(BSZUpload::DeliveryMode delivery_mode) {
    if (unlikely(delivery_mode == BSZUpload::DeliveryMode::NONE))
        LOG_ERROR("delivery mode NONE not allowed");

    const auto count(size(delivery_mode));
    db_connection_->queryOrDie("DELETE FROM harvested_urls WHERE delivery_mode='" + DeliveryModeToSqlEnum(delivery_mode) + "'");
    return count;
}


size_t DownloadTracker::size(BSZUpload::DeliveryMode delivery_mode) const {
    if (unlikely(delivery_mode == BSZUpload::DeliveryMode::NONE))
        LOG_ERROR("delivery mode NONE not allowed");

    db_connection_->queryOrDie("SELECT id FROM harvested_urls WHERE delivery_mode='" + DeliveryModeToSqlEnum(delivery_mode) + "'");
    return db_connection_->getLastResultSet().size();
}


size_t DownloadTracker::listOutdatedJournals(BSZUpload::DeliveryMode delivery_mode, const unsigned cutoff_days,
                                             std::unordered_map<std::string, std::map<BSZUpload::DeliveryMode, time_t>> * const outdated_journals)
{
    db_connection_->queryOrDie("SELECT journal_name, last_harvest_time, delivery_mode FROM harvested_urls"
                               "WHERE last_harvest_time < DATEADD(day, -" + std::to_string(cutoff_days) + ", GETDATE()) " +
                               "AND delivery_mode='" + DeliveryModeToSqlEnum(delivery_mode) + "'");
    auto result_set(db_connection_->getLastResultSet());
    while (const DbRow row = result_set.getNextRow()) {
        const auto journal_name(row["journal_name"]);
        const auto last_harvest_time(SqlUtil::DatetimeToTimeT(row["last_harvest_time"]));
        const auto saved_delivery_mode(SqlEnumToDeliveryMode(row["delivery_mode"]));

        auto match(outdated_journals->find(journal_name));
        if (match != outdated_journals->end()) {
            // save the most recent timestamp
            auto timestamp_match(match->second.find(saved_delivery_mode));
            if (timestamp_match == match->second.end() or timestamp_match->second < last_harvest_time)
                match->second[saved_delivery_mode] = last_harvest_time;
        } else {
            const std::map<BSZUpload::DeliveryMode, time_t> entry{ { saved_delivery_mode, last_harvest_time } };
            outdated_journals->insert(std::make_pair(journal_name, entry));
        }
    }
    return outdated_journals->size();
}
