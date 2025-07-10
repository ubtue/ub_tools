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
#include "ZoteroHarvesterRecordAggregator.h"
#include "JSON.h"
#include "TimeUtil.h"
#include "UrlUtil.h"
#include "ZoteroHarvesterDownload.h"
#include "util.h"


namespace ZoteroHarvester {

namespace RecordAggregator {


std::unique_ptr<unsigned> RequestPageCount(const Config::JournalParams &journal) {
    const TimeLimit DEFAULT_TIME_LIMIT(3000);

    std::string url = journal.entry_point_url_ + "?journal=" + UrlUtil::UrlEncode(journal.name_)
                      + "&page_size=" + std::to_string(journal.paged_rss_size_) + "&info=1";

    std::string result;
    if (not ::Download(url, DEFAULT_TIME_LIMIT, &result)) {
        LOG_WARNING("Download failed for page count URL: " + url);
        return nullptr;
    }

    JSON::Parser parser(result);
    std::shared_ptr<JSON::JSONNode> root;
    if (not parser.parse(&root)) {
        LOG_WARNING("JSON parse error: " + parser.getErrorMessage() + " | Response: " + result);
        return nullptr;
    }

    std::shared_ptr<JSON::ObjectNode> obj = JSON::JSONNode::CastToObjectNodeOrDie("root", root);
    if (obj->hasNode("total_pages")) {
        return std::make_unique<unsigned>(static_cast<unsigned>(obj->getIntegerValue("total_pages")));
    } else {
        LOG_WARNING("Missing 'total_pages' in response JSON: " + result);
        return nullptr;
    }
}

std::string ExpandPaginationUrl(const Config::JournalParams &journal, unsigned page_size, unsigned page_num) {
    return journal.entry_point_url_ + "?journal=" + UrlUtil::UrlEncode(journal.name_) + "&page_size=" + std::to_string(page_size)
           + "&page_num=" + std::to_string(page_num);
}

void AddPagedJournal(PagedRSSJournalState *paged_rss_journal_state) {
    auto total_pages_opt = RequestPageCount(*paged_rss_journal_state->journal_);
    if (not total_pages_opt || *total_pages_opt == 0) {
        LOG_ERROR("Failed to retrieve page count or no pages for journal '" + paged_rss_journal_state->journal_->name_ + "'");
    }

    unsigned total_pages = *total_pages_opt;

    if (paged_rss_journal_state->journal_->paged_rss_range_.empty()) {
        for (unsigned page = 1; page <= total_pages; ++page) {
            paged_rss_journal_state->urls_.push_back(
                ExpandPaginationUrl(*paged_rss_journal_state->journal_, paged_rss_journal_state->journal_->paged_rss_size_, page));
        }
    } else {
        for (unsigned page : paged_rss_journal_state->journal_->paged_rss_range_) {
            if (page >= 1 && page <= total_pages) {
                paged_rss_journal_state->urls_.push_back(
                    ExpandPaginationUrl(*paged_rss_journal_state->journal_, paged_rss_journal_state->journal_->paged_rss_size_, page));
            } else {
                LOG_ERROR("Requested page " + std::to_string(page) + " is out of range for journal '"
                          + paged_rss_journal_state->journal_->name_ + "' (total pages: " + std::to_string(total_pages) + ")");
            }
        }
    }
}


} // end namespace RecordAggregator


} // end namespace ZoteroHarvester