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
    std::cerr << "Usage: " << ::progname << " [--verbosity=log_level] [--ignore-robots-dot-txt] config_file_path [section1 section2 .. sectionN]\n"
              << "       Possible log levels are ERROR, WARNING, INFO, and DEBUG with the default being WARNING.\n"
              << "       If any section names have been provided, only those will be processed o/w all sections will be processed.\n\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRSS(const IniFile::Section &section) {
    const std::string feed_url(section.getString("feed"));
    LOG_DEBUG("feed_url: " + feed_url);
}


void InitSiteDescFromIniFileSection(const IniFile::Section &section, SimpleCrawler::SiteDesc * const site_desc) {
    site_desc->start_url_ = section.getString("base_url");
    site_desc->max_crawl_depth_ = section.getUnsigned("max_crawl_depth");
    site_desc->url_regex_matcher_.reset(RegexMatcher::FactoryOrDie(section.getString("extraction_regex")));
}
    
    
void ProcessCrawl(const IniFile::Section &section, const std::string &marc_output_file,
                  std::shared_ptr<Zotero::HarvestMaps> harvest_maps, const SimpleCrawler::Params &crawler_params,
                  const std::shared_ptr<RegexMatcher> &supported_urls_regex)
{
    const std::string issn(section.getString("issn", ""));
    const std::string optional_strptime_format(section.getString("strptime_format", ""));

    std::shared_ptr<Zotero::HarvestParams> harvest_params;
    harvest_params->optional_strptime_format_ = optional_strptime_format;
    harvest_params->format_handler_.reset(new Zotero::MarcFormatHandler(marc_output_file, harvest_maps, harvest_params));

    SimpleCrawler::SiteDesc site_desc;
    InitSiteDescFromIniFileSection(section, &site_desc);
    Zotero::HarvestSite(site_desc, crawler_params, supported_urls_regex, harvest_params, harvest_maps);
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

    bool ignore_robots_dot_txt(false);
    if (std::strcmp(argv[1], "--ignore-robots-dot-txt") == 0) {
        ignore_robots_dot_txt = true;
        ++argc, --argv;
        if (argc < 2)
            Usage();
    }
    
    IniFile ini_file(argv[1]);

    std::shared_ptr<Zotero::HarvestParams> harvest_params(new Zotero::HarvestParams);
    harvest_params->zts_server_url_ = Url(ini_file.getString("", "zts_server_url"));

    std::string map_directory_path(ini_file.getString("", "map_directory_path"));
    if (not StringUtil::EndsWith(map_directory_path, '/'))
        map_directory_path += '/';

    std::shared_ptr<Zotero::HarvestMaps> harvest_maps(Zotero::LoadMapFilesFromDirectory(map_directory_path));
    const std::shared_ptr<RegexMatcher> supported_urls_regex(Zotero::LoadSupportedURLsRegex(map_directory_path));

    const std::string PREVIOUSLY_DOWNLOADED_HASHES_PATH(map_directory_path + "previously_downloaded.hashes");
    Zotero::PreviouslyDownloadedHashesManager previously_downloaded_hashes_manager(PREVIOUSLY_DOWNLOADED_HASHES_PATH,
                                                                                   &harvest_maps->previously_downloaded_);
    const std::string MARC_OUTPUT_FILE(ini_file.getString("", "marc_output_file"));
    harvest_params->format_handler_ = Zotero::FormatHandler::Factory(GetMarcFormat(MARC_OUTPUT_FILE), MARC_OUTPUT_FILE,
                                                                     harvest_maps, harvest_params);

    SimpleCrawler::Params crawler_params;
    crawler_params.ignore_robots_dot_txt_ = ignore_robots_dot_txt;
    crawler_params.min_url_processing_time_ = Zotero::DEFAULT_MIN_URL_PROCESSING_TIME;
    crawler_params.timeout_ = Zotero::DEFAULT_TIMEOUT;

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
            ProcessCrawl(section.second, MARC_OUTPUT_FILE, harvest_maps, crawler_params, supported_urls_regex);
    }

    if (section_name_to_found_flag_map.size() > processed_section_count) {
        std::cerr << "The following sections were specified but not processed:\n";
        for (const auto &section_name_and_found_flag : section_name_to_found_flag_map)
            std::cerr << '\t' << section_name_and_found_flag.first << '\n';
    }

    return EXIT_SUCCESS;
}
