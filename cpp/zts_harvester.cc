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
#include "Compiler.h"
#include "DbConnection.h"
#include "IniFile.h"
#include "MARC.h"
#include "StlHelpers.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"
#include "Zotero.h"


namespace {


const std::unordered_map<std::string, std::string> group_to_user_agent_map = {
    // system-specific groups
    {"IxTheo", "ub_tools/ixtheo (see https://ixtheo.de/crawler)"},
    {"RelBib", "ub_tools/relbib (see https://relbib.de/crawler)"},
    {"KrimDok", "ub_tools/krimdok (see https://krimdok.uni-tuebingen.de/crawler)"},
    // user-specific groups
    {"Braun", "ub_tools/test"},
    {"Kellmeyer", "ub_tools/ixtheo (see https://ixtheo.de/crawler)"},
    {"Kim", "ub_tools/ixtheo (see https://ixtheo.de/crawler)"},
    {"Stelzel", "ub_tools/krimdok (see https://krimdok.uni-tuebingen.de/crawler)"},
};


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [options] config_file_path [section1 section2 .. sectionN]\n"
              << "\n"
              << "\tOptions:\n"
              << "\t[--min-log-level=log_level]    Possible log levels are ERROR, WARNING, INFO, and DEBUG with the default being WARNING.\n"
              << "\t[--test]                       No download information will be stored for further downloads.\n"
              << "\t[--live-only]                  Only sections that have \"delivery_mode=test|live\" set will be processed.\n"
              << "\t[--groups=my_groups            Where groups are a comma-separated list of goups.\n"
              << "\t[--ignore-robots-dot-txt]\n"
              << "\t[--map-directory=map_directory]\n"
              << "\t[--output-file=output_file]\n"
              << "\n"
              << "\tIf any section names have been provided, only those will be processed o/w all sections will be processed.\n\n";
    std::exit(EXIT_FAILURE);
}


void LoadMARCEditInstructions(const IniFile::Section &section, std::vector<MARC::EditInstruction> * edit_instructions) {
    edit_instructions->clear();

    for (const auto &entry : section) {
        if (StringUtil::StartsWith(entry.name_, "insert_field_")) {
            const std::string tag_candidate(entry.name_.substr(__builtin_strlen("insert_field_")));
            if (tag_candidate.length() != MARC::Record::TAG_LENGTH)
                LOG_ERROR("bad entry in section \"" + section.getSectionName() + "\" \"" + entry.name_ + "\"!");
            edit_instructions->emplace_back(MARC::EditInstruction::CreateInsertFieldInstruction(tag_candidate, entry.value_));
        } else if (StringUtil::StartsWith(entry.name_, "insert_subfield_")) {
            const std::string tag_and_subfield_code_candidate(entry.name_.substr(__builtin_strlen("insert_subfield_")));
            if (tag_and_subfield_code_candidate.length() != MARC::Record::TAG_LENGTH + 1)
                LOG_ERROR("bad entry in section \"" + section.getSectionName() + "\" \"" + entry.name_ + "\"!");
            edit_instructions->emplace_back(MARC::EditInstruction::CreateInsertSubfieldInstruction(
                tag_and_subfield_code_candidate.substr(0, MARC::Record::TAG_LENGTH),
                tag_and_subfield_code_candidate[MARC::Record::TAG_LENGTH], entry.value_));
        } else if (StringUtil::StartsWith(entry.name_, "add_subfield_")) {
            const std::string tag_and_subfield_code_candidate(entry.name_.substr(__builtin_strlen("add_subfield_")));
            if (tag_and_subfield_code_candidate.length() != MARC::Record::TAG_LENGTH + 1)
                LOG_ERROR("bad entry in section \"" + section.getSectionName() + "\" \"" + entry.name_ + "\"!");
            edit_instructions->emplace_back(MARC::EditInstruction::CreateAddSubfieldInstruction(
                tag_and_subfield_code_candidate.substr(0, MARC::Record::TAG_LENGTH),
                tag_and_subfield_code_candidate[MARC::Record::TAG_LENGTH], entry.value_));
        }
    }
}


void ReadGenericSiteAugmentParams(const IniFile::Section &section, Zotero::SiteAugmentParams * const augment_params) {
    augment_params->parent_journal_name_ = section.getSectionName();
    augment_params->parent_ISSN_print_ = section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_ISSN_PRINT), "");
    augment_params->parent_ISSN_online_ = section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_ISSN_ONLINE), "");
    augment_params->strptime_format_ = section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::STRPTIME_FORMAT), "");
    augment_params->parent_PPN_ = section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_PPN), "");
}


UnsignedPair ProcessRSSFeed(const IniFile::Section &section, const std::shared_ptr<Zotero::HarvestParams> &harvest_params,
                            const Zotero::SiteAugmentParams &augment_params, DbConnection * const db_connection, const bool &test)
{
    const std::string feed_url(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::FEED)));
    LOG_DEBUG("feed_url: " + feed_url);
    Zotero::RSSHarvestMode rss_harvest_mode(Zotero::RSSHarvestMode::NORMAL);
    if (test)
        rss_harvest_mode = Zotero::RSSHarvestMode::TEST;
    return Zotero::HarvestSyndicationURL(rss_harvest_mode, feed_url, harvest_params, augment_params, db_connection);
}


void ReadCrawlerSiteDesc(const IniFile::Section &section, SimpleCrawler::SiteDesc * const site_desc) {
    site_desc->start_url_ = section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::BASE_URL));
    site_desc->max_crawl_depth_ = section.getUnsigned(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::MAX_CRAWL_DEPTH));
    site_desc->url_regex_matcher_.reset(RegexMatcher::RegexMatcherFactoryOrDie(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::EXTRACTION_REGEX))));
}


UnsignedPair ProcessCrawl(const IniFile::Section &section, const std::shared_ptr<Zotero::HarvestParams> &harvest_params,
                          const Zotero::SiteAugmentParams &augment_params, const SimpleCrawler::Params &crawler_params,
                          const std::shared_ptr<RegexMatcher> &supported_urls_regex)
{
    SimpleCrawler::SiteDesc site_desc;
    ReadCrawlerSiteDesc(section, &site_desc);
    return Zotero::HarvestSite(site_desc, crawler_params, supported_urls_regex, harvest_params, augment_params);
}


UnsignedPair ProcessDirectHarvest(const IniFile::Section &section, const std::shared_ptr<Zotero::HarvestParams> &harvest_params,
                                  const Zotero::SiteAugmentParams &augment_params)
{
    return Zotero::HarvestURL(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::URL)), harvest_params, augment_params);
}


std::string GetMarcFormat(const std::string &output_filename) {
    switch (MARC::GuessFileType(output_filename)) {
    case MARC::FileType::BINARY:
        return "marc21";
    case MARC::FileType::XML:
        return "marcxml";
    default:
        LOG_ERROR("can't determine output format from MARC output filename \"" + output_filename + "\"!");
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage();

    bool test(false);
    if (std::strcmp(argv[1], "--test") == 0) {
        test = true;
        --argc, ++argv;
    }

    bool live_only(false);
    if (std::strcmp(argv[1], "--live-only") == 0) {
        live_only = true;
        --argc, ++argv;
    }

    std::set<std::string> groups_filter;
    if (StringUtil::StartsWith(argv[1], "--groups=")) {
        StringUtil::SplitThenTrimWhite(argv[1] + __builtin_strlen("--groups="), ',', &groups_filter);
        --argc, ++argv;
    }

    bool ignore_robots_dot_txt(false);
    if (std::strcmp(argv[1], "--ignore-robots-dot-txt") == 0) {
        ignore_robots_dot_txt = true;
        --argc, ++argv;
    }

    std::string map_directory_path;
    const std::string MAP_DIRECTORY_FLAG_PREFIX("--map-directory=");
    if (StringUtil::StartsWith(argv[1], MAP_DIRECTORY_FLAG_PREFIX)) {
        map_directory_path = argv[1] + MAP_DIRECTORY_FLAG_PREFIX.length();
        --argc, ++argv;
    }

    std::string output_file;
    const std::string OUTPUT_FILE_FLAG_PREFIX("--output-file=");
    if (StringUtil::StartsWith(argv[1], OUTPUT_FILE_FLAG_PREFIX)) {
        output_file = argv[1] + OUTPUT_FILE_FLAG_PREFIX.length();
        --argc, ++argv;
    }

    if (argc < 2)
        Usage();

    IniFile ini_file(argv[1]);

    std::shared_ptr<Zotero::HarvestParams> harvest_params(new Zotero::HarvestParams);
    harvest_params->zts_server_url_ = Url(ini_file.getString("", "zts_server_url"));

    if (map_directory_path.empty())
        map_directory_path = ini_file.getString("", "map_directory_path");

    // ZoteroFormatHandler expects a directory path with a trailing /
    if (not StringUtil::EndsWith(map_directory_path, '/'))
        map_directory_path += "/";

    Zotero::AugmentMaps augment_maps(map_directory_path);
    const std::shared_ptr<RegexMatcher> supported_urls_regex(Zotero::LoadSupportedURLsRegex(map_directory_path));

    std::unique_ptr<DbConnection> db_connection;
    const IniFile rss_ini_file(DbConnection::DEFAULT_CONFIG_FILE_PATH);
    db_connection.reset(new DbConnection(rss_ini_file));

    if (output_file.empty())
        output_file = ini_file.getString("", "marc_output_file");
    harvest_params->format_handler_ = Zotero::FormatHandler::Factory(db_connection.get(), GetMarcFormat(output_file),
                                                                     output_file, harvest_params);

    std::unordered_map<std::string, bool> section_name_to_found_flag_map;
    for (int arg_no(2); arg_no < argc; ++arg_no)
        section_name_to_found_flag_map.emplace(argv[arg_no], false);

    std::map<std::string, int> type_string_to_value_map;
    for (const auto &type : Zotero::HARVESTER_TYPE_TO_STRING_MAP)
        type_string_to_value_map[type.second] = type.first;
    unsigned processed_section_count(0);
    UnsignedPair total_record_count_and_previously_downloaded_record_count;

    std::set<std::string> group_names;
    std::map<std::string, Zotero::GroupParams> group_name_to_params_map;
    for (const auto &section : ini_file) {
        if (section.getSectionName().empty()) {
            StringUtil::SplitThenTrimWhite(section.getString("groups"), ',', &group_names);
            continue;
        }

        // Group processing:
        if (group_names.find(section.getSectionName()) != group_names.cend()) {
            Zotero::LoadGroup(section, &group_name_to_params_map);
            continue;
        }

        const Zotero::DeliveryMode delivery_mode(static_cast<Zotero::DeliveryMode>(section.getEnum("delivery_mode", Zotero::STRING_TO_DELIVERY_MODE_MAP, Zotero::DeliveryMode::NONE)));
        if (live_only and delivery_mode == Zotero::DeliveryMode::NONE)
            continue;

        const std::string group_name(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::GROUP)));
        const auto group_name_and_params(group_name_to_params_map.find(group_name));
        if (group_name_and_params == group_name_to_params_map.cend())
            LOG_ERROR("unknown or undefined group \"" + group_name + "\" in section \"" + section.getSectionName() + "\"!");

        std::vector<MARC::EditInstruction> edit_instructions;
        LoadMARCEditInstructions(section, &edit_instructions);

        Zotero::GobalAugmentParams global_augment_params(&augment_maps);

        Zotero::SiteAugmentParams site_augment_params;
        site_augment_params.global_params_          = &global_augment_params;
        site_augment_params.group_params_           = &group_name_and_params->second;
        site_augment_params.marc_edit_instructions_ = edit_instructions;
        ReadGenericSiteAugmentParams(section, &site_augment_params);

        harvest_params->format_handler_->setAugmentParams(&site_augment_params);

        if (not section_name_to_found_flag_map.empty()) {
            const auto section_name_and_found_flag(section_name_to_found_flag_map.find(section.getSectionName()));
            if (section_name_and_found_flag == section_name_to_found_flag_map.end())
                continue;
            section_name_and_found_flag->second = true;
        }

        harvest_params->user_agent_ = group_name_and_params->second.user_agent_;

        LOG_INFO("Processing section \"" + section.getSectionName() + "\".");
        ++processed_section_count;

        const Zotero::HarvesterType type(static_cast<Zotero::HarvesterType>(section.getEnum(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::TYPE),
                                                                                            type_string_to_value_map)));
        if (type == Zotero::HarvesterType::RSS)
            total_record_count_and_previously_downloaded_record_count += ProcessRSSFeed(section, harvest_params, site_augment_params,
                                                                                        db_connection.get(), test);
        else if (type == Zotero::HarvesterType::CRAWL) {
            SimpleCrawler::Params crawler_params;
            crawler_params.ignore_robots_dot_txt_ = ignore_robots_dot_txt;
            crawler_params.min_url_processing_time_ = Zotero::DEFAULT_MIN_URL_PROCESSING_TIME;
            crawler_params.timeout_ = Zotero::DEFAULT_TIMEOUT;
            crawler_params.user_agent_ = harvest_params->user_agent_;

            total_record_count_and_previously_downloaded_record_count +=
                ProcessCrawl(section, harvest_params, site_augment_params, crawler_params, supported_urls_regex);
        } else
            total_record_count_and_previously_downloaded_record_count +=
                ProcessDirectHarvest(section, harvest_params, site_augment_params);
    }

    LOG_INFO("Extracted metadata from "
             + std::to_string(total_record_count_and_previously_downloaded_record_count.first
                              - total_record_count_and_previously_downloaded_record_count.second) + " page(s).");

    if (section_name_to_found_flag_map.size() > processed_section_count) {
        std::cerr << "The following sections were specified but not processed:\n";
        for (const auto &section_name_and_found_flag : section_name_to_found_flag_map)
            std::cerr << '\t' << section_name_and_found_flag.first << '\n';
    }

    return EXIT_SUCCESS;
}
