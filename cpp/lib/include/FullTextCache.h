/** \file   FullTextCache.h
 *  \brief  Anything relating to our full-text cache.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#ifndef FullTextCache_H
#define FullTextCache_H


#include <string>
#include <vector>
#include "DbConnection.h"


class FullTextCache {
    DbConnection *db_connection_;
public:
    FullTextCache();
    ~FullTextCache() { delete db_connection_; }
    struct Entry {
        std::string id_;
        time_t expiration_;
    };
    struct EntryUrl {
        std::string id_;
        std::string url_;
        std::string domain_;
        std::string error_message_;
    };
    struct JoinedEntry {
        std::string id_;
        time_t expiration_;
        std::string url_;
        std::string domain_;
        std::string error_message_;
    };
    struct EntryGroup {
        unsigned count_;
        std::string domain_;
        std::string error_message_;
        JoinedEntry example_entry_;
    };

    /** \brief Test whether an entry in the cache has expired or not.
     *  \return True if we don't find "id" in the database, or the entry is older than now-CACHE_EXPIRE_TIME_DELTA,
     *          or at least one url has changed, else false.
     *  \note Deletes expired entries and associated data in the key/value database found at "full_text_db_path".
     */
    bool entryExpired(const std::string &key, std::vector<std::string> urls);

    /** \brief Delete all records whose expiration field is in the past */
    void expireEntries();
    std::vector<std::string> getDomains();
    bool getDomainFromUrl(const std::string &url, std::string &domain);
    bool getEntry(const std::string &id, Entry * const entry);
    std::vector<EntryUrl> getEntryUrls(const std::string &id);
    std::vector<std::string> getEntryUrlsAsStrings(const std::string &id);

    /** \brief Get the full text (as string) for the given id */
    bool getFullText(const std::string &id, std::string * const full_text);

    /** \brief Get all entries grouped by domain and error message */
    std::vector<EntryGroup> getEntryGroupsByDomainAndErrorMessage();

    /** \brief Get all entries for a domain and error message */
    std::vector<JoinedEntry> getJoinedEntriesByDomainAndErrorMessage(const std::string &domain,
                                                                     const std::string &error_message);

    /** \brief Get an example entry for a domain and error message */
    JoinedEntry getJoinedEntryByDomainAndErrorMessage(const std::string &domain,
                                                      const std::string &error_message);

    /** \brief Get the number of datasets in full_text_cache table */
    unsigned getSize();

    /* \note If "data" is empty only an entry will be made in the SQL database but not in the key/value store.  Also
     *       either "data" must be non-empty or "error_message" must be non-empty.
     */
    void insertEntry(const std::string &id, const std::string &full_text,
                     const std::vector<EntryUrl> &entry_urls);
};


#endif // ifndef FullTextCache_H
