/* \brief Loads, manages and stores the timestamps, hashes of previously downloaded metadata records.
 *
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
#pragma once

#include <unordered_map>
#include "DbConnection.h"
#include "BSZUpload.h"
#include "SqlUtil.h"


class DownloadTracker {
    DbConnection *db_connection_;
public:
    struct Entry {
        std::string url_;
        std::string journal_name_;
        time_t last_harvest_time_;
        std::string error_message_;
        std::string hash_;
        BSZUpload::DeliveryMode delivery_mode_;
    };
public:
    explicit DownloadTracker(DbConnection * const db): db_connection_(db) {}
    ~DownloadTracker() = default;

    // Acceptable delivery modes: TEST, LIVE

    /** \brief Checks if "url" or ("url", "hash") have already been downloaded.
     *  \return True if we have find an entry for "url" or ("url", "hash"), else false.
     */
    bool hasAlreadyBeenDownloaded(BSZUpload::DeliveryMode delivery_mode, const std::string &url, const std::string &hash = "", Entry * const entry = nullptr) const;

    void addOrReplace(BSZUpload::DeliveryMode delivery_mode, const std::string &url, const std::string &journal_name, const std::string &hash, const std::string &error_message);

    /** \brief Lists entries that match the URL regex given the delivery mode constraint.
    */
    size_t listMatches(BSZUpload::DeliveryMode delivery_mode, const std::string &url_regex, std::vector<Entry> * const entries) const;

    size_t deleteMatches(BSZUpload::DeliveryMode delivery_mode, const std::string &url_regex);

    /** \return 0 if no matching entry was found, o/w 1. */
    size_t deleteSingleEntry(BSZUpload::DeliveryMode delivery_mode, const std::string &url);

    /** \brief Deletes all entries that have timestamps <= "cutoff_timestamp".
     *  \return  The number of deleted entries.
     */
    size_t deleteOldEntries(BSZUpload::DeliveryMode delivery_mode, const time_t cutoff_timestamp);

    /** \brief Deletes all entries in the database.
     *  \return The number of deleted entries.
     */
    size_t clear(BSZUpload::DeliveryMode delivery_mode);

    size_t size(BSZUpload::DeliveryMode delivery_mode) const;

    /** \brief Lists all journals that haven't had a single URL harvested for a given number of days.
     *  \return The number of outdated journals.
     */
    size_t listOutdatedJournals(BSZUpload::DeliveryMode delivery_mode, const unsigned cutoff_days,
                                std::unordered_map<std::string, std::map<BSZUpload::DeliveryMode, time_t>> * const outdated_journals);
private:
    inline void truncateURL(std::string * const url) const {
        if (url->length() > static_cast<std::size_t>(SqlUtil::VARCHAR_UTF8_MAX_LENGTH))
            url->erase(SqlUtil::VARCHAR_UTF8_MAX_LENGTH);
    }
};


