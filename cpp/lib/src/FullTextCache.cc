/** \file   FullTextCache.cc
 *  \brief  Implementation of functions relating to our full-text cache.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "FullTextCache.h"
#include <algorithm>
#include <sstream>
#include <tuple>
#include <ctime>
#include "Compiler.h"
#include "DbRow.h"
#include "FileUtil.h"
#include "PdfUtil.h"
#include "Random.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "UrlUtil.h"
#include "VuFind.h"
#include "util.h"


constexpr unsigned MIN_CACHE_EXPIRE_TIME_ON_ERROR(42300 * 60);     // About 1 month in seconds.
constexpr unsigned MAX_CACHE_EXPIRE_TIME_ON_ERROR(42300 * 60 * 2); // About 2 months in seconds.


static const std::map<std::string, FullTextCache::TextType> description_to_text_type_map{ { "Volltext", FullTextCache::FULLTEXT },
                                                                                          { "Inhaltsverzeichnis", FullTextCache::TOC },
                                                                                          { "Inhaltstext", FullTextCache::ABSTRACT },
                                                                                          { "Zusammenfassung", FullTextCache::SUMMARY },
                                                                                          { "Literaturverzeichnis",
                                                                                            FullTextCache::LIST_OF_REFERENCES } };


bool FullTextCache::getDomainFromUrl(const std::string &url, std::string * const domain) const {
    std::string scheme, username_password, authority, port, path, params, query, fragment, relative_url;
    const bool result(
        UrlUtil::ParseUrl(url, &scheme, &username_password, &authority, &port, &path, &params, &query, &fragment, &relative_url));
    if (result)
        *domain = authority;

    return result;
}


bool FullTextCache::entryExpired(const std::string &id, std::vector<std::string> urls) {
    Entry entry;
    if (not getEntry(id, &entry))
        return true;
    const time_t now(std::time(nullptr));
    if (entry.expiration_ == TimeUtil::BAD_TIME_T or now < entry.expiration_) {
        std::vector<std::string> existing_urls(FullTextCache::getEntryUrlsAsStrings(id));

        std::sort(existing_urls.begin(), existing_urls.end());
        std::sort(urls.begin(), urls.end());
        if (urls == existing_urls)
            return false;
    }

    return true;
}


bool FullTextCache::singleUrlExpired(const std::string &id, const std::string &url) {
    Entry entry;
    if (not getEntry(id, &entry))
        return true;
    if (entry.expiration_ == TimeUtil::BAD_TIME_T or (std::time(nullptr) < entry.expiration_)) {
        for (const auto &entry_url : FullTextCache::getEntryUrls(id)) {
            if (entry_url.url_ == url and entry_url.error_message_.empty())
                return false;
        }
    }
    return true;
}


bool FullTextCache::dummyEntryExists(const std::string &id) {
    Entry entry;
    if (not getEntry(id, &entry))
        return false;
    for (const auto &entry_url : FullTextCache::getEntryUrls(id)) {
        if (entry_url.url_ == DUMMY_URL)
            return true;
    }
    return false;
}


void FullTextCache::expireEntries() {
    full_text_cache_urls_.deleteRange("expiration", Elasticsearch::RO_LTE, "now");
}


bool FullTextCache::getEntry(const std::string &id, Entry * const entry) const {
    const auto results(full_text_cache_.simpleSelect({ "expiration" }, "id", id));
    if (results.empty())
        return false;

    entry->id_ = id;
    if (results.front().find("expiration") == results.front().cend())
        entry->expiration_ = TimeUtil::BAD_TIME_T;
    else
        entry->expiration_ = TimeUtil::Iso8601StringToTimeT(results.front().find("expiration")->second);
    return true;
}


bool FullTextCache::hasEntryWithType(const std::string &id, const TextType &text_type) const {
    Entry entry;
    if (not getEntry(id, &entry))
        return false;
    const auto results(full_text_cache_.simpleSelect({ "text_type" }, "id", id));
    for (const auto &result : results) {
        const auto result_type(result.find("text_type"));
        if (result_type == result.cend())
            continue;
        if (result_type->second == std::to_string(text_type))
            return true;
    }
    return false;
}


bool FullTextCache::hasEntry(const std::string &id) const {
    Entry entry;
    return getEntry(id, &entry);
}


inline std::string GetValueOrEmptyString(const std::map<std::string, std::string> &map, const std::string &key) {
    const auto pair(map.find(key));
    return pair == map.cend() ? "" : pair->second;
}


std::vector<FullTextCache::EntryUrl> FullTextCache::getEntryUrls(const std::string &id) const {
    const auto results(full_text_cache_urls_.simpleSelect({ "url", "domain", "error_message" }, "id", id));

    std::vector<EntryUrl> entry_urls;
    entry_urls.reserve(results.size());
    for (const auto &map : results) {
        EntryUrl entry_url;
        entry_url.id_ = id;
        entry_url.url_ = GetValueOrEmptyString(map, "url");
        entry_url.domain_ = GetValueOrEmptyString(map, "domain");
        entry_url.error_message_ = GetValueOrEmptyString(map, "error_message");
        entry_urls.emplace_back(entry_url);
    }
    return entry_urls;
}


std::vector<std::string> FullTextCache::getEntryUrlsAsStrings(const std::string &id) const {
    const auto results(full_text_cache_urls_.simpleSelect({ "url" }, "id", id));

    std::vector<std::string> urls;
    for (const auto &map : results) {
        const auto url(GetValueOrEmptyString(map, "url"));
        if (not url.empty())
            urls.emplace_back(url);
    }
    return urls;
}


unsigned FullTextCache::getErrorCount() const {
    return full_text_cache_urls_.count({ { "(error_message", "[ * TO * ]) AND -(error_message:DUMMY\\ ERROR)" } });
}


bool FullTextCache::getFullText(const std::string &id, std::string * const full_text) const {
    const auto results(full_text_cache_.simpleSelect({ "full_text" }, "id", id));

    if (results.empty())
        return false;

    *full_text = GetValueOrEmptyString(results.front(), "full_text");
    return true;
}


const std::string US("\x1F"); // ASCII unit separator


std::vector<FullTextCache::EntryGroup> FullTextCache::getEntryGroupsByDomainAndErrorMessage() const {
    const auto results(full_text_cache_urls_.simpleSelect({ "url", "domain", "error_message", "id" }, {}, UINT_MAX));

    std::unordered_map<std::string, std::tuple<std::string, std::string, unsigned>> domains_and_errors_to_ids_and_urls_and_counts_map;
    for (const auto &map : results) {
        const auto url_pair(map.find("url"));
        const auto domain_pair(map.find("domain"));
        const auto error_message_pair(map.find("error_message"));
        if (url_pair == map.cend() or domain_pair == map.cend() or error_message_pair == map.cend()
            or error_message_pair->second == FullTextCache::DUMMY_ERROR)
            continue;

        const auto id_pair(map.find("id"));
        const auto key(domain_pair->second + US + error_message_pair->second);
        auto id_and_url_and_count(domains_and_errors_to_ids_and_urls_and_counts_map.find(key));
        if (id_and_url_and_count != domains_and_errors_to_ids_and_urls_and_counts_map.end())
            ++std::get<2>(id_and_url_and_count->second);
        else
            domains_and_errors_to_ids_and_urls_and_counts_map[key] = std::make_tuple(id_pair->second, url_pair->second, 1);
    }

    std::vector<EntryGroup> groups;
    groups.reserve(domains_and_errors_to_ids_and_urls_and_counts_map.size());
    for (const auto &domain_and_error_to_id_and_url_and_count : domains_and_errors_to_ids_and_urls_and_counts_map) {
        std::vector<std::string> parts;
        if (unlikely(StringUtil::Split(domain_and_error_to_id_and_url_and_count.first, US, &parts) != 2))
            LOG_ERROR("This should never happen (" + std::to_string(parts.size())
                      + "): " + StringUtil::CStyleEscape(domain_and_error_to_id_and_url_and_count.first));

        groups.emplace_back(EntryGroup(std::get<2>(domain_and_error_to_id_and_url_and_count.second), parts[0], parts[1],
                                       std::get<0>(domain_and_error_to_id_and_url_and_count.second),
                                       std::get<1>(domain_and_error_to_id_and_url_and_count.second)));
    }

    std::sort(groups.begin(), groups.end(), [](const EntryGroup &eg1, const EntryGroup &eg2) { return eg1.count_ > eg2.count_; });
    return groups;
}


std::vector<FullTextCache::EntryUrl> FullTextCache::getJoinedEntriesByDomainAndErrorMessage(const std::string &domain_,
                                                                                            const std::string &error_message_) const {
    const auto results(full_text_cache_urls_.simpleSelect({ "url", "domain", "error_message", "id" },
                                                          { { "domain", domain_ }, { "error_message", error_message_ } }));

    std::vector<EntryUrl> entries;
    for (const auto &map : results) {
        const std::string url(GetValueOrEmptyString(map, "url"));
        const std::string domain(GetValueOrEmptyString(map, "domain"));
        const std::string error_message(GetValueOrEmptyString(map, "error_message"));
        const std::string id(GetValueOrEmptyString(map, "id"));

        entries.emplace_back(EntryUrl(id, url, domain, error_message));
    }

    std::sort(entries.begin(), entries.end(), [](const EntryUrl &eu1, const EntryUrl &eu2) { return eu1.id_ < eu2.id_; });
    return entries;
}


FullTextCache::EntryUrl FullTextCache::getJoinedEntryByDomainAndErrorMessage(const std::string &domain_,
                                                                             const std::string &error_message_) const {
    const auto results(full_text_cache_urls_.simpleSelect({ "url", "domain", "error_message", "id" },
                                                          { { "domain", domain_ }, { "error_message", error_message_ } },
                                                          /* max_count = */ 1));
    if (unlikely(results.size() != 1))
        LOG_ERROR("failed to get one entry!");

    const std::string url(GetValueOrEmptyString(results.front(), "url"));
    const std::string domain(GetValueOrEmptyString(results.front(), "domain"));
    const std::string error_message(GetValueOrEmptyString(results.front(), "error_message"));
    const std::string id(GetValueOrEmptyString(results.front(), "id"));

    return EntryUrl(id, url, domain, error_message);
}


unsigned FullTextCache::getSize() const {
    return full_text_cache_.size();
}


FullTextCache::TextType FullTextCache::MapTextDescriptionToTextType(const std::string &text_description) {
    const auto text_type(description_to_text_type_map.find(text_description));
    if (text_type == description_to_text_type_map.cend())
        return UNKNOWN;
    return text_type->second;
}


void FullTextCache::extractPDFAndImportHTMLPages(const std::string &id, const std::string &full_text_location, const TextType &text_type) {
    const FileUtil::AutoTempDirectory auto_temp_dir("/tmp/ADT");
    const std::string html_export_directory(auto_temp_dir.getDirectoryPath());
    PdfUtil::ExtractHTMLAsPages(full_text_location, html_export_directory);
    FileUtil::Directory html_pages(html_export_directory, ".*-\\d+\\.html");
    for (const auto &html_page : html_pages) {
        static const auto page_number_matcher(RegexMatcher::RegexMatcherFactoryOrDie(".*-(\\d+)\\.html$"));
        const std::string page_file_name(html_page.getFullName());
        if (not page_number_matcher->matched(page_file_name))
            LOG_ERROR("Invalid naming scheme for file \"" + page_file_name + "\"");
        const std::string page_number((*page_number_matcher)[1]);
        // Read in the file to full_text string
        std::ifstream page_file(page_file_name);
        std::stringstream full_text_stream;
        full_text_stream << page_file.rdbuf();
        std::string page_text(full_text_stream.str());
        full_text_cache_html_.simpleInsert(
            { { "id", id }, { "page", page_number }, { "full_text", page_text }, { "text_type", std::to_string(text_type) } });
    }
}


void FullTextCache::importHTMLFile(const std::string &id, const std::string &html_file_location, const TextType &text_type) {
    std::ifstream html_file(html_file_location);
    std::stringstream full_text_stream;
    full_text_stream << html_file.rdbuf();
    std::string full_text(full_text_stream.str());
    if (not StringUtil::FindCaseInsensitive(full_text, "<html"))
        LOG_ERROR("\"" + html_file_location + "\" does not seem to be a valid html file");
    full_text_cache_html_.simpleInsert({ { "id", id }, { "full_text", full_text }, { "text_type", std::to_string(text_type) } });
}


void FullTextCache::insertEntry(const std::string &id, const std::string &full_text, const std::vector<EntryUrl> &entry_urls,
                                const TextType &text_type, const bool is_publisher_provided) {
    const time_t now(std::time(nullptr));
    Random::Rand rand(now);
    time_t expiration(TimeUtil::BAD_TIME_T);
    for (const auto &entry_url : entry_urls) {
        if (full_text.empty() and entry_url.error_message_.empty())
            LOG_ERROR("you must provide either data to be cached or a non-empty error message! (id " + id + ")");

        if (not entry_url.error_message_.empty())
            expiration = now + MIN_CACHE_EXPIRE_TIME_ON_ERROR + rand(MAX_CACHE_EXPIRE_TIME_ON_ERROR - MIN_CACHE_EXPIRE_TIME_ON_ERROR);
    }

    if (expiration == TimeUtil::BAD_TIME_T) {
        if (not full_text.empty())
            full_text_cache_.simpleInsert({ { "id", id },
                                            { "full_text", full_text },
                                            { "text_type", std::to_string(text_type) },
                                            { "is_publisher_provided", is_publisher_provided ? "true" : "false" } });
    } else {
        const std::string expiration_string = TimeUtil::TimeTToString(expiration, TimeUtil::ISO_8601_FORMAT);
        if (full_text.empty())
            full_text_cache_.simpleInsert({ { "id", id }, { "expiration", expiration_string } });
        else
            full_text_cache_.simpleInsert({ { "id", id },
                                            { "expiration", expiration_string },
                                            { "full_text", full_text },
                                            { "text_type", std::to_string(text_type) } });
    }

    for (const auto &entry_url : entry_urls) {
        if (entry_url.error_message_.empty())
            full_text_cache_urls_.simpleInsert(
                { { "id", id }, { "url", entry_url.url_ }, { "domain", entry_url.domain_ }, { "text_type", std::to_string(text_type) } });
        else
            full_text_cache_urls_.simpleInsert({ { "id", id },
                                                 { "url", entry_url.url_ },
                                                 { "domain", entry_url.domain_ },
                                                 { "error_message", entry_url.error_message_ } });
    }
}


bool FullTextCache::hasUrlWithTextType(const std::string &id, const TextType &text_type) {
    const auto results(full_text_cache_urls_.simpleSelect({ "text_type" }, "id", id));
    if (results.empty())
        return false;

    for (const auto &result : results) {
        if (result.find("text_type")->second == std::to_string(text_type))
            return true;
    }
    return false;
}


bool FullTextCache::deleteEntry(const std::string &id) {
    return full_text_cache_.deleteDocument(id) and full_text_cache_urls_.deleteDocument(id) and full_text_cache_html_.deleteDocument(id);
}
