/** \brief Utility classes related to the Zotero Harvester
 *  \author Madeeswaran Kannan
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

#include "StringUtil.h"
#include "ZoteroHarvesterUtil.h"
#include "util.h"


namespace ZoteroHarvester {


namespace Util {


static ThreadUtil::ThreadSafeCounter<unsigned> harvestable_counter(0);


Harvestable Harvestable::New(const std::string &url, const Config::JournalParams &journal) {
    ++harvestable_counter;
     return Harvestable(harvestable_counter, url, journal);
}



} // end namespace Util


} // end namespace ZoteroHarvester
