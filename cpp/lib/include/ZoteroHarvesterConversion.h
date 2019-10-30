/** \brief Classes related to the Zotero Harvester's JSON-to-MARC conversion API
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
#pragma once


#include <memory>
#include <set>
#include <unordered_map>
#include "JSON.h"
#include "RegexMatcher.h"
#include "ZoteroHarvesterConfig.h"
#include "ZoteroHarvesterUtil.h"


namespace ZoteroHarvester {


namespace Conversion {


struct MetadataRecord {
    enum SSGType { INVALID, FG_0, FG_1, FG_01, KRIM_21 };

    struct Creator {
        std::string first_name_;
        std::string last_name_;
        std::string affix_;
        std::string title_;
        std::string type_;
        std::string ppn_;
        std::string gnd_number_;
    };

    std::string item_type_;
    std::string title_;
    std::string short_title_;
    std::vector<Creator> creators_;
    std::string abstract_note_;
    std::string publication_title_;
    std::string volume_;
    std::string issue_;
    std::string pages_;
    std::string date_;
    std::string doi_;
    std::string language_;
    std::string url_;
    std::string issn_;
    std::string license_;
    SSGType ssg_;
    std::vector<std::string> keywords_;
    std::map<std::string, std::string> custom_metadata_;
};


} // end namespace Conversion


} // end namespace ZoteroHarvester
