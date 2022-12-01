/** \file   FullTextCache.h
 *  \brief  Anything relating to our full-text cache.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <string>
#include <vector>
#include "Elasticsearch.h"


class FullTextCache {
    Elasticsearch full_text_cache_, full_text_cache_urls_, full_text_cache_html_;

public:
    struct Entry {
        std::string id_;
        time_t expiration_;
    };
    struct EntryUrl {
        std::string id_;
        std::string url_;
        std::string domain_;
        std::string error_message_;

    public:
        EntryUrl() = default;
        EntryUrl(const std::string &id, const std::string &url, const std::string &domain, const std::string &error_message)
            : id_(id), url_(url), domain_(domain), error_message_(error_message) { }
    };
    struct EntryGroup {
        unsigned count_;
        std::string domain_;
        std::string error_message_;
        EntryUrl example_entry_;

    public:
        EntryGroup() = default;
        EntryGroup(const unsigned count, const std::string &domain, const std::string &error_message, const std::string &id,
                   const std::string &url)
            : count_(count), domain_(domain), error_message_(error_message), example_entry_(id, url, domain, error_message) { }
    };

public:
    FullTextCache()
        : full_text_cache_("full_text_cache"), full_text_cache_urls_("full_text_cache_urls"),
          full_text_cache_html_("full_text_cache_html") { }

    /** \brief Test whether an entry in the cache has expired or not.
     *  \return True if we don't find "id" in the database, or the entry is older than now-CACHE_EXPIRE_TIME_DELTA,
     *          or at least one url has changed, else false.
     *  \note Deletes expired entries and associated data in the key/value database found at "full_text_db_path".
     */
    bool entryExpired(const std::string &key, std::vector<std::string> urls);
    bool singleUrlExpired(const std::string &key, const std::string &url);
    bool dummyEntryExists(const std::string &key);
    enum TextType {
        FULLTEXT = 1,
        TOC = 2,
        ABSTRACT = 4,
        SUMMARY = 8,
        LIST_OF_REFERENCES = 16,
        UNKNOWN = 0
    }; // Must match constants in TuelibMixin.java
    static constexpr auto DUMMY_URL = "DUMMY URL";
    static constexpr auto DUMMY_DOMAIN = "DUMMY DOMAIN";
    static constexpr auto DUMMY_ERROR = "DUMMY ERROR";

    /** \brief Delete all records whose expiration field is in the past */
    void expireEntries();
    inline std::unordered_multiset<std::string> getDomains() const { return full_text_cache_urls_.selectAllNonUnique("domain"); }
    bool getDomainFromUrl(const std::string &url, std::string * const domain) const;
    bool getEntry(const std::string &id, Entry * const entry) const;
    std::vector<EntryUrl> getEntryUrls(const std::string &id) const;
    std::vector<std::string> getEntryUrlsAsStrings(const std::string &id) const;

    /** \brief Get the number of cache entries with at least one error */
    unsigned getErrorCount() const;

    /** \brief Get the full text (as string) for the given id */
    bool getFullText(const std::string &id, std::string * const full_text) const;

    /** \brief Get all entries grouped by domain and error message.
     *  \note  The returned entries are sorted in descending order of the count_ field of the EntryGroup structs.
     */
    std::vector<EntryGroup> getEntryGroupsByDomainAndErrorMessage() const;

    /** \brief Get all entries for a domain and error message */
    std::vector<EntryUrl> getJoinedEntriesByDomainAndErrorMessage(const std::string &domain, const std::string &error_message) const;

    /** \brief Get an example entry for a domain and error message */
    EntryUrl getJoinedEntryByDomainAndErrorMessage(const std::string &domain, const std::string &error_message) const;

    /** \brief Get the number of datasets in full_text_cache table */
    unsigned getSize() const;


    /** \brief Extract and Import Page oriented Full Text */
    void extractPDFAndImportHTMLPages(const std::string &id, const std::string &full_text_location, const TextType &text_type = FULLTEXT);

    /* \note If "data" is empty only an entry will be made in the SQL database but not in the key/value store.  Also
     *       either "data" must be non-empty or "error_message" must be non-empty.
     */
    void insertEntry(const std::string &id, const std::string &full_text, const std::vector<EntryUrl> &entry_urls,
                     const TextType &text_type = FULLTEXT, const bool is_publisher_provided = false);

    bool deleteEntry(const std::string &id);

    static TextType MapTextDescriptionToTextType(const std::string &text_description);

    bool hasUrlWithTextType(const std::string &id, const TextType &text_type);

    bool hasEntryWithType(const std::string &id, const TextType &text_type) const;
    bool hasEntry(const std::string &id) const;
};


inline FullTextCache::TextType operator|(const FullTextCache::TextType &lhs, const FullTextCache::TextType &rhs) {
    return static_cast<FullTextCache::TextType>(static_cast<std::underlying_type_t<FullTextCache::TextType>>(lhs)
                                                | static_cast<std::underlying_type_t<FullTextCache::TextType>>(rhs));
}


inline FullTextCache::TextType operator|=(FullTextCache::TextType &lhs, const FullTextCache::TextType &rhs) {
    return lhs = lhs | rhs;
}
