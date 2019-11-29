/*  \brief Functionality referring to the Upload functionality of BSZ
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

#include "BSZUpload.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include <memory>


namespace BSZUpload {


inline static void UpdateDeliveryTrackerEntryFromDbRow(const DbRow &row, DeliveryTracker::Entry * const entry) {
    if (row.empty())
        LOG_ERROR("Couldn't extract DeliveryTracker entry from empty DbRow");

    entry->url_ = row["url"];
    entry->delivered_at_ = SqlUtil::DatetimeToTimeT(row["delivered_at"]);
    entry->journal_name_ = row["journal_name"];
    entry->hash_ = row["hash"];
}

bool DeliveryTracker::urlAlreadyDelivered(const std::string &url, Entry * const entry) const {
    std::string truncated_url(url);
    truncateURL(&truncated_url);

    db_connection_->queryOrDie("SELECT url, delivered_at, journal_name, hash FROM delivered_marc_records WHERE url='"
                               + db_connection_->escapeString(truncated_url) + "'");
    auto result_set(db_connection_->getLastResultSet());
    if (result_set.empty())
        return false;

    const auto first_row(result_set.getNextRow());
    UpdateDeliveryTrackerEntryFromDbRow(first_row, entry);
    return true;
}


bool DeliveryTracker::hashAlreadyDelivered(const std::string &hash, Entry * const entry) const {
    db_connection_->queryOrDie("SELECT url, delivered_at, journal_name, hash FROM delivered_marc_records WHERE hash='"
                               + db_connection_->escapeString(hash) + "'");
    auto result_set(db_connection_->getLastResultSet());
    if (result_set.empty())
        return false;

    const auto first_row(result_set.getNextRow());
    UpdateDeliveryTrackerEntryFromDbRow(first_row, entry);
    return true;
}


size_t DeliveryTracker::listOutdatedJournals(const unsigned cutoff_days, std::unordered_map<std::string, time_t> * const outdated_journals) {
    db_connection_->queryOrDie("SELECT url, delivered_at, journal_name, hash FROM harvested_urls"
                               "WHERE last_harvest_time < DATEADD(day, -" + std::to_string(cutoff_days) + ", GETDATE())");
    auto result_set(db_connection_->getLastResultSet());
    Entry temp_entry;
    while (const DbRow row = result_set.getNextRow()) {
        UpdateDeliveryTrackerEntryFromDbRow(row, &temp_entry);

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


time_t DeliveryTracker::getLastDeliveryTime(const std::string &journal_name) const {
    db_connection_->queryOrDie("SELECT delivered_at FROM delivered_marc_records WHERE journal_name='" +
                                db_connection_->escapeString(journal_name) + "' ORDER BY delivered_at DESC");
    auto result_set(db_connection_->getLastResultSet());
    if (result_set.empty())
        return TimeUtil::BAD_TIME_T;

    return SqlUtil::DatetimeToTimeT(result_set.getNextRow()["delivered_at"]);
}


} // namespace BSZUpload
