/*  \brief Functionality referring to the Upload functionality of BSZ
 *
 *  \copyright 2018, 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <map>
#include <unordered_map>
#include "DbConnection.h"
#include "SqlUtil.h"


namespace BSZUpload {


enum DeliveryMode { NONE, TEST, LIVE };


const std::map<std::string, int> STRING_TO_DELIVERY_MODE_MAP {
    { "NONE", static_cast<int>(DeliveryMode::NONE) },
    { "TEST", static_cast<int>(DeliveryMode::TEST) },
    { "LIVE", static_cast<int>(DeliveryMode::LIVE) }
};


const std::map<int, std::string> DELIVERY_MODE_TO_STRING_MAP {
    { static_cast<int>(DeliveryMode::NONE), "NONE" },
    { static_cast<int>(DeliveryMode::TEST), "TEST" },
    { static_cast<int>(DeliveryMode::LIVE), "LIVE" }
};


// Tracks records that have been uploaded to the BSZ server.
class DeliveryTracker {
    DbConnection *db_connection_;
public:
    struct Entry {
        std::string url_;
        std::string journal_name_;
        time_t delivered_at_;
        std::string hash_;
    };
public:
    explicit DeliveryTracker(DbConnection * const db): db_connection_(db) {}
    ~DeliveryTracker() = default;
public:
    bool urlAlreadyDelivered(const std::string &url, Entry * const entry = nullptr) const;
    bool hashAlreadyDelivered(const std::string &hash, Entry * const entry = nullptr) const;

    /** \brief Lists all journals that haven't had a single URL delivered for a given number of days.
     *  \return The number of outdated journals.
     */
    size_t listOutdatedJournals(const unsigned cutoff_days, std::unordered_map<std::string, time_t> * const outdated_journals);

    /** \brief Returns when the last URL of the given journal was delivered to the BSZ.
     *  \return Timestamp of the last delivery if found, TimeUtil::BAD_TIME_T otherwise.
     */
    time_t getLastDeliveryTime(const std::string &journal_name) const;
private:
    inline void truncateURL(std::string * const url) const {
        if (url->length() > static_cast<std::size_t>(SqlUtil::VARCHAR_UTF8_MAX_INDEX_LENGTH))
            url->erase(SqlUtil::VARCHAR_UTF8_MAX_INDEX_LENGTH);
    }
};


} // namespace BSZUpload
