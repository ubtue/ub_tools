/** \brief Utility to automatically generate maps used for zotkat.
 *  \author Mario Trojan
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
#include <iostream>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "IniFile.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "util.h"
#include "ZoteroHarvesterConfig.h"
#include "ZoteroHarvesterUtil.h"
#include "ZoteroHarvesterZederInterop.h"


// avoid conflicts with LICENSE defined in mysql_version.h
#ifdef LICENSE
    #undef LICENSE
#endif


namespace {


[[noreturn]] void Usage() {
    ::Usage("[options] [zotero_enhancement_maps_directory]\n"
            "\n"
            "\tOptions:\n"
            "\t[--min-log-level=log_level]          Possible log levels are ERROR, WARNING (default), INFO and DEBUG\n"
            "\t[zotero_enhancement_maps_directory]  or default " + UBTools::GetTuelibPath() + "zotero-enhancement-maps" + "\n");
}


void DownloadZederInstanceEntries(const Zeder::Flavour flavour,
                                  std::unordered_map<Zeder::Flavour, Zeder::EntryCollection> * const downloaded_entries)
{
    LOG_INFO("Downloading Zeder entries for " + Zeder::FLAVOUR_TO_STRING_MAP.at(flavour));
    Zeder::EntryCollection downloaded_flavour_entries;

    const auto endpoint_url(Zeder::GetFullDumpEndpointPath(flavour));
    const std::unordered_set<unsigned> entries_to_download {}; // intentionally empty
    const std::unordered_set<std::string> columns_to_download {};  // intentionally empty
    const std::unordered_map<std::string, std::string> filter_regexps {}; // intentionally empty
    std::unique_ptr<Zeder::FullDumpDownloader::Params> downloader_params(
        new Zeder::FullDumpDownloader::Params(endpoint_url, entries_to_download, columns_to_download, filter_regexps));

    auto downloader(Zeder::FullDumpDownloader::Factory(Zeder::FullDumpDownloader::Type::FULL_DUMP, std::move(downloader_params)));
    if (not downloader->download(&downloaded_flavour_entries, true))
        LOG_ERROR("couldn't download full dump for " + Zeder::FLAVOUR_TO_STRING_MAP.at(flavour));

    downloaded_entries->emplace(flavour, downloaded_flavour_entries);
}


std::unordered_set<std::string> GetZederEntryISSNs(const Zeder::Entry &entry, const Zeder::Flavour flavour) {
    std::unordered_set<std::string> issns;

    std::unordered_set<ZoteroHarvester::Config::JournalParams::IniKey> ini_keys {
        ZoteroHarvester::Config::JournalParams::IniKey::ONLINE_ISSN,
        ZoteroHarvester::Config::JournalParams::IniKey::PRINT_ISSN
    };
    for (const auto ini_key : ini_keys) {
        const std::string issn(ZoteroHarvester::ZederInterop::GetJournalParamsIniValueFromZederEntry(entry, flavour,
                               ini_key));

        if (not issn.empty()) {
            if (MiscUtil::IsPossibleISSN(issn))
                issns.emplace(issn);
            else {
                LOG_WARNING("Skipping invalid ISSN: " + issn
                            + " (Zeder ID: " + std::to_string(entry.getId())
                            + ", Instance: " + Zeder::FLAVOUR_TO_STRING_MAP.at(flavour) + ")");
            }
        }
    }

    return issns;
}


void GenerateIssnToLicenseMap(const std::string &zotero_enhancement_maps_directory,
                              const std::unordered_map<Zeder::Flavour, Zeder::EntryCollection> &downloaded_entries)
{
    const std::string map_path(zotero_enhancement_maps_directory + "/ISSN_to_licence.map");
    LOG_INFO("Generating " + map_path);

    std::map<std::string, std::string> issn_to_license_map;
    for (const auto &[flavour, downloaded_flavour_entries] : downloaded_entries) {
        for (const auto &downloaded_flavour_entry : downloaded_flavour_entries) {
            std::string license(ZoteroHarvester::ZederInterop::GetJournalParamsIniValueFromZederEntry(downloaded_flavour_entry, flavour,
                                ZoteroHarvester::Config::JournalParams::IniKey::LICENSE));

            if (not license.empty()) {
                StringUtil::ASCIIToLower(&license);
                const std::string name(ZoteroHarvester::ZederInterop::GetJournalParamsIniValueFromZederEntry(downloaded_flavour_entry, flavour,
                                       ZoteroHarvester::Config::JournalParams::IniKey::NAME));
                const auto issns(GetZederEntryISSNs(downloaded_flavour_entry, flavour));
                for (const auto &issn : issns)
                    issn_to_license_map[issn] = license + " # " + name;
            }
        }
    }

    std::string output;
    for (const auto &[issn, license] : issn_to_license_map)
        output += issn + "=" + license + '\n';

    FileUtil::WriteStringOrDie(map_path, output);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc > 2)
        Usage();

    std::string zotero_enhancement_maps_directory(UBTools::GetTuelibPath() + "zotero-enhancement-maps");
    if (argc == 2)
        zotero_enhancement_maps_directory = argv[1];

    LOG_INFO("Generating Zotero Enhancement Maps in " + zotero_enhancement_maps_directory);
    std::unordered_map<Zeder::Flavour, Zeder::EntryCollection> downloaded_entries;
    const std::vector<Zeder::Flavour> flavours({ Zeder::IXTHEO, Zeder::KRIMDOK});
    for (const auto flavour : flavours)
        DownloadZederInstanceEntries(flavour, &downloaded_entries);

    GenerateIssnToLicenseMap(zotero_enhancement_maps_directory, downloaded_entries);

    return EXIT_SUCCESS;
}
