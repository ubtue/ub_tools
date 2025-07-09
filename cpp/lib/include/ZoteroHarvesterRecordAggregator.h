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
#pragma once


#include <deque>
#include <memory>
#include <optional>
#include <string>
#include "ZoteroHarvesterConfig.h"


namespace ZoteroHarvester {

namespace RecordAggregator {


struct PagedRSSJournalState {
    std::shared_ptr<ZoteroHarvester::Config::JournalParams> journal;
    std::deque<std::string> urls;
};

std::optional<unsigned> RequestPageCount(const ZoteroHarvester::Config::JournalParams &journal);

// Constructs a URL for a specific page of a paged journal.
std::string ExpandPaginationUrl(const ZoteroHarvester::Config::JournalParams &journal, unsigned page_size, unsigned page_num);

// Generates the state for a paged journal based on the total pages and range specified.
std::optional<PagedRSSJournalState> AddPagedJournal(std::shared_ptr<ZoteroHarvester::Config::JournalParams> journal);


} // end namespace RecordAggregator


} // end namespace ZoteroHarvester