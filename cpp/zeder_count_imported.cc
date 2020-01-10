/** \brief Utility to determine Zeder entries that have yet to be imported into Zotero Harvester.
 *  \author Madeeswaran Kannan (madeeswaran.kannan@uni-tuebingen.de)
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

#include <stdexcept>
#include <cstdlib>
#include <algorithm>
#include <set>
#include "IniFile.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"
#include "Zeder.h"
#include "ZoteroHarvesterConfig.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("harvester_config_file flavour [filter_regexps]\n"
            "    filter_regexps - Whitespace-separated regex filter expressions for Zeder columns.\n"
            "                     Format: <column-1>:<regex-1> <column-2>:<regex-2>...\n");
    std::exit(EXIT_FAILURE);
}


void ParseArgs(const int argc, char * const argv[], std::unordered_map<std::string, std::string> * const filter_regexps) {
    for (int i(3); i < argc; ++i) {
        const std::string current_arg(argv[i]);
        const auto seperator(current_arg.find(":"));
        if (seperator == std::string::npos)
            LOG_ERROR("couldn't find separator character in filter expression '" + current_arg + "'");

        (*filter_regexps)[current_arg.substr(0, seperator)] = current_arg.substr(seperator + 1);
    }
}


void DownloadFullDump(const Zeder::Flavour flavour, const std::unordered_map<std::string, std::string> &filter_regexps,
                      Zeder::EntryCollection * const downloaded_entries)
{
    const auto endpoint_url(Zeder::GetFullDumpEndpointPath(flavour));
    const std::unordered_set<unsigned> entries_to_download {};  // intentionally empty
    const std::unordered_set<std::string> columns_to_download {};  // intentionally empty
    std::unique_ptr<Zeder::FullDumpDownloader::Params> downloader_params(new Zeder::FullDumpDownloader::Params(endpoint_url,
                                                                         entries_to_download, columns_to_download, filter_regexps));

    auto downloader(Zeder::FullDumpDownloader::Factory(Zeder::FullDumpDownloader::Type::FULL_DUMP, std::move(downloader_params)));
    if (not downloader->download(downloaded_entries))
        LOG_ERROR("Couldn't download full dump for " + Zeder::FLAVOUR_TO_STRING_MAP.at(flavour));
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    const IniFile harvester_config(argv[1]);
    const Zeder::Flavour flavour(Zeder::ParseFlavour(argv[2]));
    std::unordered_map<std::string, std::string> column_filter_regexps;
    Zeder::EntryCollection full_dump;

    ParseArgs(argc, argv, &column_filter_regexps);
    DownloadFullDump(flavour, column_filter_regexps, &full_dump);

    std::set<unsigned int> full_dump_ids, not_imported_ids, imported_ids;

    std::transform(full_dump.begin(), full_dump.end(), std::inserter(full_dump_ids, full_dump_ids.begin()),
                [](const Zeder::Entry &entry) -> unsigned int { return entry.getId(); });

    for (const auto &section : harvester_config) {
        const auto section_name(section.getSectionName());
        const auto group(section.getString(ZoteroHarvester::Config::JournalParams::GetIniKeyString(
            ZoteroHarvester::Config::JournalParams::GROUP)));
        if (group != Zeder::FLAVOUR_TO_STRING_MAP.at(flavour))
            continue;

        const auto entry_id(StringUtil::ToUnsigned(section.getString(ZoteroHarvester::Config::JournalParams::GetIniKeyString(
            ZoteroHarvester::Config::JournalParams::ZEDER_ID))));
        if (full_dump_ids.find(entry_id) != full_dump_ids.end())
            imported_ids.insert(entry_id);      // only count those that belong to the set of downloaded entries
    }


    std::set_difference(full_dump_ids.begin(), full_dump_ids.end(), imported_ids.begin(),
                        imported_ids.end(), std::inserter(not_imported_ids, not_imported_ids.begin()));

    LOG_INFO("Zeder '" + Zeder::FLAVOUR_TO_STRING_MAP.at(flavour) + "' instance: " + std::to_string(full_dump.size()) + " filtered entries");
    LOG_INFO("Number of filtered entries already imported: " + std::to_string(imported_ids.size()));
    LOG_INFO("Number of filtered entries yet to be imported: " + std::to_string(not_imported_ids.size()));

    std::set<std::string> buffer;

    std::transform(imported_ids.begin(), imported_ids.end(), std::inserter(buffer, buffer.begin()),
                   [](unsigned int id) -> std::string { return std::to_string(id); });
    LOG_INFO("\nAlready imported entries: " + StringUtil::Join(buffer.begin(), buffer.end(), ","));

    buffer.clear();
    std::transform(not_imported_ids.begin(), not_imported_ids.end(), std::inserter(buffer, buffer.begin()),
                   [](unsigned int id) -> std::string { return std::to_string(id); });
    LOG_INFO("\nYet-to-be-imported entries: " + StringUtil::Join(buffer.begin(), buffer.end(), ","));

    return EXIT_SUCCESS;
}
