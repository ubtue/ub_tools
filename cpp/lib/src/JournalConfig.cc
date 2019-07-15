/** \file   JournalConfig.cc
 *  \brief  Central repository for all journal-related config data
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
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
 *  GNU Affero %General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "JournalConfig.h"


namespace JournalConfig {


const std::unordered_map<std::string, Print::Entries> Print::key_id_resolver_map{
    { "ppn",  Print::Entries::PPN  },
    { "issn", Print::Entries::ISSN },
};


const std::string Print::prefix("print");


const std::unordered_map<std::string, Online::Entries> Online::key_id_resolver_map{
    { "ppn",  Online::Entries::PPN  },
    { "issn", Online::Entries::ISSN },
};


const std::string Online::prefix("online");


const std::unordered_map<std::string, Zeder::Entries> Zeder::key_id_resolver_map{
    { "id",            Zeder::Entries::ID            },
    { "modified_time", Zeder::Entries::MODIFIED_TIME },
    { "update_window", Zeder::Entries::UPDATE_WINDOW },
};


const std::string Zeder::prefix("zeder");


const std::unordered_map<std::string, Zotero::Entries> Zotero::key_id_resolver_map{
    { "type",               Zotero::Entries::TYPE               },
    { "group",              Zotero::Entries::GROUP              },
    { "url",                Zotero::Entries::URL                },
    { "strptime_format",    Zotero::Entries::STRPTIME_FORMAT    },
    { "extraction_regex",   Zotero::Entries::EXTRACTION_REGEX   },
    { "review_regex",       Zotero::Entries::REVIEW_REGEX       },
    { "max_crawl_depth",    Zotero::Entries::MAX_CRAWL_DEPTH    },
    { "delivery_mode",      Zotero::Entries::DELIVERY_MODE      },
    { "expected_languages", Zotero::Entries::EXPECTED_LANGUAGES },
    { "banned_url_regex",   Zotero::Entries::BANNED_URL_REGEX   },
};


const std::string Zotero::prefix("zotero");


void Reader::loadFromIni(const IniFile &config) {
    for (const auto &section : config) {
        Bundles new_bundle_bundle;
        new_bundle_bundle.bundle_print_.load(section);
        new_bundle_bundle.bundle_online_.load(section);
        new_bundle_bundle.bundle_zeder_.load(section);
        new_bundle_bundle.bundle_zotero_.load(section);

        sections_to_bundles_map_[section.getSectionName()] = new_bundle_bundle;
    }
}


const Reader::Bundles &Reader::find(const std::string &section) const {
    const auto match(sections_to_bundles_map_.find(section));
    if (match == sections_to_bundles_map_.end())
        throw std::runtime_error("Couldn't find section '" + section + "'");

    return match->second;
}


} // namespace JournalConfig
