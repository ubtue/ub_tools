/** \brief Utility to download Zeder entries.
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
#include "MiscUtil.h"
#include "StringUtil.h"
#include "util.h"
#include "Zeder.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("flavour output_csv [entry_ids] [filter_regexps]\n"
            "         entry_ids - Whitespace-separated list of Zeder ID's to download.\n"
            "    filter_regexps - Whitespace-separated regex filter expressions for Zeder columns.\n"
            "                     Format: <column-1>:<regex-1> <column-2>:<regex-2>...\n");
    std::exit(EXIT_FAILURE);
}


void ParseArgs(const int argc, char * const argv[], std::unordered_set<unsigned> * const entries,
               std::unordered_map<std::string, std::string> * const filter_regexps)
{
    bool parse_as_filter(false);

    for (int i(1); i < argc; ++i) {
        const std::string current_arg(argv[i]);
        unsigned entry_id(0);
        // keep parsing arguments as entry ID's until the parse fails
        if (not StringUtil::ToUnsigned(current_arg, &entry_id))
            parse_as_filter = true;

        if (not parse_as_filter) {
            if (entry_id == 0)
                LOG_ERROR("invalid Zeder entry ID '" + current_arg + "'");
            entries->insert(entry_id);
        } else {
            const auto seperator(current_arg.find(":"));
            if (seperator == std::string::npos)
                LOG_ERROR("couldn't find separator character in filter expression '" + current_arg + "'");

            (*filter_regexps)[current_arg.substr(0, seperator)] = current_arg.substr(seperator + 1);
        }
    }
}


void DownloadEntries(const Zeder::Flavour flavour, const std::unordered_set<unsigned> entries_to_download,
                     const std::unordered_map<std::string, std::string> &filter_regexps, Zeder::EntryCollection * const downloaded_entries)
{
    const auto endpoint_url(Zeder::GetFullDumpEndpointPath(flavour));
    const std::unordered_set<std::string> columns_to_download {};  // intentionally empty
    std::unique_ptr<Zeder::FullDumpDownloader::Params> downloader_params(new Zeder::FullDumpDownloader::Params(endpoint_url,
                                                                         entries_to_download, columns_to_download, filter_regexps));

    auto downloader(Zeder::FullDumpDownloader::Factory(Zeder::FullDumpDownloader::Type::FULL_DUMP, std::move(downloader_params)));
    if (not downloader->download(downloaded_entries))
        LOG_ERROR("Couldn't download full dump for " + Zeder::FLAVOUR_TO_STRING_MAP.at(flavour));
}


void SaveToCsv(const std::string &output_file, const Zeder::EntryCollection &downloaded_entries) {
    std::vector<std::string> attributes_to_export;  // intentionally empty
    std::unique_ptr<Zeder::CsvWriter::Params> exporter_params(new Zeder::CsvWriter::Params(output_file, attributes_to_export));
    auto exporter(Zeder::Exporter::Factory(std::move(exporter_params)));

    exporter->write(downloaded_entries);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 4)
        Usage();

    const Zeder::Flavour flavour(Zeder::ParseFlavour(argv[1]));
    const std::string output_csv(argv[2]);
    argc -= 2;
    argv += 2;

    std::unordered_set<unsigned> entries_to_download;
    std::unordered_map<std::string, std::string> column_filter_regexps;
    Zeder::EntryCollection downloaded_entries;

    ParseArgs(argc, argv, &entries_to_download, &column_filter_regexps);
    DownloadEntries(flavour, entries_to_download, column_filter_regexps, &downloaded_entries);
    SaveToCsv(output_csv, downloaded_entries);

    LOG_INFO("Downloaded " + std::to_string(downloaded_entries.size()) + " entries.");

    return EXIT_SUCCESS;
}
