/** \brief Classes related to the Zotero Harvester's download of Paged RSS Feeds
 *  \author Hjordis Lindeboom
 *
 *  \copyright 2025 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "PagedJournalUtil.h"
#include "JSON.h"
#include "TimeUtil.h"
#include "UrlUtil.h"
#include "ZoteroHarvesterDownload.h"
#include "util.h"

using namespace ZoteroHarvester;

std::optional<unsigned> PagedRSSRequestPageCount(const Config::JournalParams &journal) {
    const TimeLimit DEFAULT_TIME_LIMIT(3000);

    std::string url = journal.entry_point_url_ + "?journal=" + UrlUtil::UrlEncode(journal.name_)
                      + "&page_size=" + std::to_string(journal.paged_rss_size_) + "&info=1";

    std::string result;
    if (!::Download(url, DEFAULT_TIME_LIMIT, &result)) {
        LOG_WARNING("Download failed for page count URL: " + url);
        return std::nullopt;
    }

    JSON::Parser parser(result);
    std::shared_ptr<JSON::JSONNode> root;
    if (!parser.parse(&root)) {
        LOG_WARNING("JSON parse error: " + parser.getErrorMessage() + " | Response: " + result);
        return std::nullopt;
    }

    std::shared_ptr<JSON::ObjectNode> obj = JSON::JSONNode::CastToObjectNodeOrDie("root", root);
    if (obj->hasNode("total_pages")) {
        return static_cast<unsigned>(obj->getIntegerValue("total_pages"));
    } else {
        LOG_WARNING("Missing 'total_pages' in response JSON: " + result);
        return std::nullopt;
    }
}

std::string PagedRSSExpandUrl(const Config::JournalParams &journal, unsigned page_size, unsigned page_num) {
    return journal.entry_point_url_ + "?journal=" + UrlUtil::UrlEncode(journal.name_) + "&page_size=" + std::to_string(page_size)
           + "&page_num=" + std::to_string(page_num);
}

std::optional<PagedRSSJournalState> PagedRSSAddJournal(std::shared_ptr<Config::JournalParams> journal) {
    auto total_pages_opt = PagedRSSRequestPageCount(*journal);
    if (!total_pages_opt || *total_pages_opt == 0) {
        LOG_WARNING("Failed to retrieve page count or no pages for journal '" + journal->name_ + "'");
        return std::nullopt;
    }

    unsigned total_pages = *total_pages_opt;

    std::deque<std::string> urls;

    if (journal->paged_rss_range_.empty()) {
        for (unsigned page = 1; page <= total_pages; ++page) {
            urls.push_back(PagedRSSExpandUrl(*journal, journal->paged_rss_size_, page));
        }
    } else {
        for (unsigned page : journal->paged_rss_range_) {
            if (page >= 1 && page <= total_pages) {
                urls.push_back(PagedRSSExpandUrl(*journal, journal->paged_rss_size_, page));
            } else {
                LOG_WARNING("Requested page " + std::to_string(page) + " is out of range for journal '" + journal->name_
                            + "' (total pages: " + std::to_string(total_pages) + ")");
            }
        }
    }

    return PagedRSSJournalState{ std::move(journal), std::move(urls) };
}