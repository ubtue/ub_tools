/** \brief Classes related to the Zotero Harvester's interoperation with the Zeder database
 *  \author Madeeswaran Kannan
 *
 *  \copyright 2020 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include <functional>
#include <memory>
#include <set>
#include <unordered_map>
#include "IniFile.h"
#include "MARC.h"
#include "Zeder.h"
#include "ZoteroHarvesterConfig.h"


namespace ZoteroHarvester {


namespace ZederInterop {


// Maps Zotero Harvester configuration INI keys to Zeder column names that have a one-to-one correspondence.
extern const std::map<Config::JournalParams::IniKey, std::string> INI_KEY_TO_ZEDER_COLUMN_MAP;

// Maps Zotero Harvester configuration INI keys (that don't have a one-to-one correspondence with a Zeder column) to a resolver function.
// The resolver function accepts two parameters: A Zeder::Entry reference and its flavour; returns a string that represents the value of the
// INI key.
extern const std::map<Config::JournalParams::IniKey, std::function<std::string(const Zeder::Entry &, const Zeder::Flavour)>>
    INI_KEY_TO_ZEDER_RESOLVER_MAP;


// Uses the above defined maps to retrieve the corresponding value of the INI key.
std::string GetJournalParamsIniValueFromZederEntry(const Zeder::Entry &zeder_entry, const Zeder::Flavour zeder_flavour,
                                                   const Config::JournalParams::IniKey ini_key);

Zeder::Flavour GetZederInstanceForJournal(const Config::JournalParams &journal_params);
Zeder::Flavour GetZederInstanceForGroup(const Config::GroupParams &group_params);
Zeder::Flavour GetZederInstanceFromMarcRecord(const MARC::Record &record);


std::string ResolveUploadOperation(const Zeder::Entry &zeder_entry, const Zeder::Flavour /*unused*/);


} // end namespace ZederInterop


} // end namespace ZoteroHarvester
