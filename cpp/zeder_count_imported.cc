/** \brief Utility to determine Zeder entries that have yet to be imported into zts_harvester.
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
#include "JournalConfig.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"
#include "Zeder.h"


namespace {


void LoadToolConfig(const IniFile &config_file, const Zeder::Flavour flavour,
                    std::unordered_map<std::string, std::string> * const filter_regexps)
{
    const auto flavour_section(config_file.getSection(Zeder::FLAVOUR_TO_STRING_MAP.at(flavour)));
    for (const auto &column_name : flavour_section->getEntryNames())
        (*filter_regexps)[column_name] = flavour_section->getString(column_name);
}


void DownloadFullDump(const Zeder::Flavour flavour, const std::unordered_map<std::string, std::string> &filter_regexps,
                      Zeder::EntryCollection * const downloaded_entries)
{
    const auto endpoint_url(Zeder::GetFullDumpEndpointPath(flavour));
    const std::unordered_set<unsigned> entries_to_download;  // intentionally empty
    const std::unordered_set<std::string> columns_to_download;  // intentionally empty
    std::unique_ptr<Zeder::FullDumpDownloader::Params> downloader_params(new Zeder::FullDumpDownloader::Params(endpoint_url,
                                                                         entries_to_download, columns_to_download, filter_regexps));

    auto downloader(Zeder::FullDumpDownloader::Factory(Zeder::FullDumpDownloader::Type::FULL_DUMP, std::move(downloader_params)));
    if (not downloader->download(downloaded_entries))
        LOG_ERROR("Couldn't download full dump for " + Zeder::FLAVOUR_TO_STRING_MAP.at(flavour));
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        ::Usage("flavour config_file harvester_config_file");

    const Zeder::Flavour flavour(Zeder::ParseFlavour(argv[1]));
    const IniFile tool_config(argv[2]);
    const IniFile harvester_config(argv[3]);
    const JournalConfig::Reader bundle_reader(harvester_config);
    std::unordered_map<std::string, std::string> column_filter_regexps;
    Zeder::EntryCollection full_dump;

    LoadToolConfig(tool_config, flavour, &column_filter_regexps);
    DownloadFullDump(flavour, column_filter_regexps, &full_dump);

    std::set<unsigned int> imported_ids;
    for (const auto &section : harvester_config) {
        const auto section_name(section.getSectionName());
        const auto group(bundle_reader.zotero(section_name).value(JournalConfig::Zotero::GROUP, ""));
        if (group != Zeder::FLAVOUR_TO_STRING_MAP.at(flavour))
            continue;

        imported_ids.insert(StringUtil::ToUnsigned(bundle_reader.zeder(section_name).value(JournalConfig::Zeder::ID)));
    }

    std::set<unsigned int> full_dump_ids, not_imported_ids;
    std::transform(full_dump.begin(), full_dump.end(), std::inserter(full_dump_ids, full_dump_ids.begin()),
                   [](const Zeder::Entry &entry) -> unsigned int { return entry.getId(); });
    std::set_difference(full_dump_ids.begin(), full_dump_ids.end(), imported_ids.begin(),
                        imported_ids.end(), std::inserter(not_imported_ids, not_imported_ids.begin()));

    LOG_INFO("Zeder '" + Zeder::FLAVOUR_TO_STRING_MAP.at(flavour) + "' instance: " + std::to_string(full_dump.size()) + " entries");
    LOG_INFO("Number of imported entries: " + std::to_string(imported_ids.size()));
    LOG_INFO("Number of yet-to-be-imported entries: " + std::to_string(not_imported_ids.size()));

    std::set<std::string> buffer;

    std::transform(imported_ids.begin(), imported_ids.end(), std::inserter(buffer, buffer.begin()),
                   [](unsigned int id) -> std::string { return std::to_string(id); });
    LOG_INFO("\nImported entries: " + StringUtil::Join(buffer.begin(), buffer.end(), ","));

    buffer.clear();
    std::transform(not_imported_ids.begin(), not_imported_ids.end(), std::inserter(buffer, buffer.begin()),
                   [](unsigned int id) -> std::string { return std::to_string(id); });
    LOG_INFO("\nYet-to-be-imported entries: " + StringUtil::Join(buffer.begin(), buffer.end(), ","));

    return EXIT_SUCCESS;
}
