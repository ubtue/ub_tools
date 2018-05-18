/** \brief Handles crawling as well as RSS feeds.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <memory>
#include <unordered_map>
#include <cstdlib>
#include "IniFile.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"
#include "Zotero.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbosity=log_level] config_file_path [section1 section2 .. sectionN]\n"
              << "       Possible log levels are ERROR, WARNING, INFO, and DEBUG with the default being WARNING.\n"
              << "       If any section names have been provided, only those will be processed o/w all sections will be processed.\n\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRSS(const IniFile::Section &section) {
    const std::string feed_url(section.getString("feed"));
    LOG_DEBUG("feed_url: " + feed_url);
}


void ProcessCrawl(const IniFile::Section &section, const std::string &marc_output_file,
                  Zotero::AugmentParams &augment_params)
{
    const std::string base_url(section.getString("base_url"));
    std::shared_ptr<RegexMatcher> extraction_regex(RegexMatcher::FactoryOrDie(section.getString("extraction_regex")));
//    const unsigned max_crawl_depth(section.getUnsigned("max_crawl_depth"));
    const std::string issn(section.getString("issn", ""));
    const std::string optional_strptime_format(section.getString("strptime_format", ""));

    std::shared_ptr<Zotero::HarvestParams> harvest_params;
    harvest_params->format_handler_.reset(new Zotero::MarcFormatHandler(marc_output_file, augment_params, harvest_params));
}


std::string GetMarcFormat(const std::string &output_filename) {
    if (StringUtil::EndsWith(output_filename, ".mrc") or StringUtil::EndsWith(output_filename, ".marc"))
        return "marc21";
    if (StringUtil::EndsWith(output_filename, ".xml"))
        return "marcxml";
    LOG_ERROR("can't determine output format from MARC output filename \"" + output_filename + "\"!");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage();

    IniFile ini_file(argv[1]);

    std::shared_ptr<Zotero::HarvestParams> harvest_params(new Zotero::HarvestParams);
    harvest_params->zts_server_url_ = Url(ini_file.getString("", "zts_server_url"));

    std::string map_directory_path(ini_file.getString("", "map_directory_path"));
    if (not StringUtil::EndsWith(map_directory_path, '/'))
        map_directory_path += '/';

    Zotero::AugmentParams augment_params;
    augment_params.maps_ = Zotero::LoadMapFilesFromDirectory(map_directory_path);
    const std::shared_ptr<RegexMatcher> supported_urls_regex(Zotero::LoadSupportedURLsRegex(map_directory_path));

    const std::string PREVIOUSLY_DOWNLOADED_HASHES_PATH(map_directory_path + "previously_downloaded.hashes");
    Zotero::PreviouslyDownloadedHashesManager previously_downloaded_hashes_manager(PREVIOUSLY_DOWNLOADED_HASHES_PATH,
                                                                                   &augment_params.maps_.previously_downloaded_);
    const std::string MARC_OUTPUT_FILE(ini_file.getString("", "marc_output_file"));
    harvest_params->format_handler_ = Zotero::FormatHandler::Factory(GetMarcFormat(MARC_OUTPUT_FILE), MARC_OUTPUT_FILE,
                                                                     augment_params, harvest_params);

    std::unordered_map<std::string, bool> section_name_to_found_flag_map;
    for (int arg_no(2); arg_no < argc; ++arg_no)
        section_name_to_found_flag_map.emplace(argv[arg_no], false);

    enum Type { RSS, CRAWL };
    const std::map<std::string, int> string_to_value_map{ {"RSS", RSS }, { "CRAWL", CRAWL } };
    unsigned processed_section_count(0);
    for (const auto &section : ini_file) {
        if (not section_name_to_found_flag_map.empty()) {
            const auto section_name_and_found_flag(section_name_to_found_flag_map.find(section.first));
            if (section_name_and_found_flag == section_name_to_found_flag_map.end())
                continue;
            section_name_and_found_flag->second = true;
        }
        ++processed_section_count;

        LOG_INFO("Processing section \"" + section.first + "\".");
        const Type type(static_cast<Type>(section.second.getEnum("type", string_to_value_map)));
        if (type == RSS)
            ProcessRSS(section.second);
        else
            ProcessCrawl(section.second, MARC_OUTPUT_FILE, augment_params);
    }

    if (section_name_to_found_flag_map.size() > processed_section_count) {
        std::cerr << "The following sections were specified but not processed:\n";
        for (const auto &section_name_and_found_flag : section_name_to_found_flag_map)
            std::cerr << '\t' << section_name_and_found_flag.first << '\n';
    }

    return EXIT_SUCCESS;
}
