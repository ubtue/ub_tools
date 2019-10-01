/** \brief Handles crawling as well as RSS feeds.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018,2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "FileUtil.h"
#include "IniFile.h"
#include "JournalConfig.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"
#include "Zotero.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [options] config_file_path [section1 section2 .. sectionN]\n"
              << "\n"
              << "\tOptions:\n"
              << "\t[--min-log-level=log_level]         Possible log levels are ERROR, WARNING, INFO, and DEBUG with the default being WARNING.\n"
              << "\t[--delivery-mode=mode]              Only sections that have the specific delivery mode (either LIVE or TEST) set will be processed. When this parameter is not specified, tracking is automatically disabled.\n"
              << "\t[--groups=my_groups                 Where groups are a comma-separated list of groups.\n"
              << "\t[--zeder-ids=my_zeder_ids           Where IDs are a comma-separated list of Zeder IDs.\n"
              << "\t[--force-downloads]                 Download all records regardless of their 'delivered' status.\n"
              << "\t[--ignore-robots-dot-txt]\n"
              << "\t[--map-directory=map_directory]\n"
              << "\t[--output-directory=output_directory]\n"
              << "\t[--output-filename=output_filename] Overrides the automatically-generated filename based on the current date/time.\n"
              << "\t[--output-format=output_format]     Either \"marc-21\" or \"marc-xml\" or \"json\", with the default being \"marc-xml\"\n"
              << "\t[--harvest-url-regex=regex]         For testing purposes. When set, only those URLs that match this regex will be harvested\n"
              << "\t[--harvest-single-url=url]          For testing purposes. When set, only this URL will be harvested. \n"
              << "\n"
              << "\tIf any section names have been provided, only those will be processed o/w all sections will be processed.\n\n";
    std::exit(EXIT_FAILURE);
}


void ReadGenericSiteAugmentParams(const IniFile &ini_file, const IniFile::Section &section, const JournalConfig::Reader &bundle_reader,
                                  Zotero::SiteParams * const site_params)
{
    const auto section_name(section.getSectionName());
    site_params->journal_name_ = section_name;
    site_params->ISSN_print_ = bundle_reader.print(section_name).value(JournalConfig::Print::ISSN, "");
    site_params->ISSN_online_ = bundle_reader.online(section_name).value(JournalConfig::Online::ISSN, "");
    site_params->PPN_print_ = bundle_reader.print(section_name).value(JournalConfig::Print::PPN, "");
    site_params->PPN_online_ = bundle_reader.online(section_name).value(JournalConfig::Online::PPN, "");

    const auto extraction_regex(bundle_reader.zotero(section_name).value(JournalConfig::Zotero::EXTRACTION_REGEX, ""));
    if (not extraction_regex.empty())
        site_params->extraction_regex_.reset(RegexMatcher::RegexMatcherFactoryOrDie(extraction_regex));

    const auto review_regex(bundle_reader.zotero(section_name).value(JournalConfig::Zotero::REVIEW_REGEX, ""));
    if (not review_regex.empty())
        site_params->review_regex_.reset(RegexMatcher::RegexMatcherFactoryOrDie(review_regex));

    // Append the common time format string to the site-specific override
    site_params->strptime_format_ = bundle_reader.zotero(section_name).value(JournalConfig::Zotero::STRPTIME_FORMAT, "");

    const auto common_strptime_format(ini_file.getString("", "common_strptime_format"));
    if (not common_strptime_format.empty()) {
        if (common_strptime_format[0] == '(')
            LOG_ERROR("Cannot specify locale in common_strptime_format");

        if (not site_params->strptime_format_.empty())
            site_params->strptime_format_ += '|';
        site_params->strptime_format_ += common_strptime_format;
    }

    auto expected_languages(bundle_reader.zotero(section_name).value(JournalConfig::Zotero::EXPECTED_LANGUAGES, ""));
    if (not expected_languages.empty() and expected_languages[0] == '*') {
        site_params->force_automatic_language_detection_ = true;
        expected_languages = expected_languages.substr(1);
    }
    const auto field_separator_pos(expected_languages.find(':'));
    if (field_separator_pos != std::string::npos) {
        site_params->expected_languages_text_fields_ = expected_languages.substr(0, field_separator_pos);
        expected_languages = expected_languages.substr(field_separator_pos + 1);
    }
    StringUtil::Split(expected_languages, ',', &site_params->expected_languages_, /* suppress_empty_components = */true);

    for (const auto &entry : section) {
        if (StringUtil::StartsWith(entry.name_, "override_metadata_")) {
            const auto field_name(entry.name_.substr(__builtin_strlen("override_metadata_")));
            site_params->metadata_overrides_.insert(std::make_pair(field_name, entry.value_));
        } else if (StringUtil::StartsWith(entry.name_, "suppress_metadata_")) {
            const auto field_name(entry.name_.substr(__builtin_strlen("suppress_metadata_")));
            site_params->metadata_suppression_filters_.insert(std::make_pair(field_name,
                                                              std::unique_ptr<RegexMatcher>(RegexMatcher::RegexMatcherFactoryOrDie(entry.value_))));
        } else if (StringUtil::StartsWith(entry.name_, "add_field"))
            site_params->additional_fields_.emplace_back(entry.value_);
        else if (StringUtil::StartsWith(entry.name_, "non_standard_metadata_field"))
            site_params->non_standard_metadata_fields_.emplace_back(entry.value_);
        else if (StringUtil::StartsWith(entry.name_, "exclude_if_field_")) {
            const auto field_name(entry.name_.substr(__builtin_strlen("exclude_if_field_")));
            if (field_name.length() != MARC::Record::TAG_LENGTH and field_name.length() != MARC::Record::TAG_LENGTH + 1)
                LOG_ERROR("invalid exclusion field name '" + field_name + "'! expected format: <tag> or <tag><subfield_code>");

            site_params->field_exclusion_filters_.insert(std::make_pair(field_name,
                                                         std::unique_ptr<RegexMatcher>(RegexMatcher::RegexMatcherFactoryOrDie(entry.value_))));
        } else if (StringUtil::StartsWith(entry.name_, "exclude_if_metadata_")) {
            const auto metadata_name(entry.name_.substr(__builtin_strlen("exclude_if_metadata_")));
            site_params->metadata_exclusion_filters_.insert(std::make_pair(metadata_name,
                                                                std::unique_ptr<RegexMatcher>(RegexMatcher::RegexMatcherFactoryOrDie(entry.value_))));
        } else if (StringUtil::StartsWith(entry.name_, "remove_field_")) {
            const auto field_name(entry.name_.substr(__builtin_strlen("remove_field_")));
            if (field_name.length() != MARC::Record::TAG_LENGTH + 1)
                LOG_ERROR("invalid removal filter name '" + field_name + "'! expected format: <tag><subfield_code>");

            site_params->field_removal_filters_.insert(std::make_pair(field_name,
                                                       std::unique_ptr<RegexMatcher>(RegexMatcher::RegexMatcherFactoryOrDie(entry.value_))));
        }
    }

    site_params->zeder_id_ = bundle_reader.zeder(section_name).value(JournalConfig::Zeder::ID);
    StringUtil::ToUnsigned(bundle_reader.zotero(section_name).value(JournalConfig::Zotero::UPDATE_WINDOW, "0"),
                           &site_params->journal_update_window_);
    site_params->ssgn_ = BSZTransform::GetSSGNTypeFromString(bundle_reader.zotero(section_name).value(JournalConfig::Zotero::SSGN, ""));
}


UnsignedPair ProcessRSSFeed(const IniFile::Section &section, const JournalConfig::Reader &bundle_reader,
                            const std::shared_ptr<Zotero::HarvestParams> &harvest_params, const Zotero::SiteParams &site_params,
                            Zotero::HarvesterErrorLogger * const error_logger)
{
    const std::string feed_url(bundle_reader.zotero(section.getSectionName()).value(JournalConfig::Zotero::URL));
    LOG_DEBUG("feed_url: " + feed_url);

    return Zotero::HarvestSyndicationURL(feed_url, harvest_params, site_params, error_logger);
}


UnsignedPair ProcessCrawl(const IniFile::Section &section, const JournalConfig::Reader &bundle_reader,
                          const std::shared_ptr<Zotero::HarvestParams> &harvest_params, const Zotero::SiteParams &site_params,
                          const SimpleCrawler::Params &crawler_params, const std::shared_ptr<RegexMatcher> &supported_urls_regex,
                          Zotero::HarvesterErrorLogger * const error_logger)
{
    SimpleCrawler::SiteDesc site_desc;
    site_desc.start_url_ = bundle_reader.zotero(section.getSectionName()).value(JournalConfig::Zotero::URL);
    site_desc.max_crawl_depth_ =
        StringUtil::ToUnsigned(bundle_reader.zotero(section.getSectionName()).value(JournalConfig::Zotero::MAX_CRAWL_DEPTH));


    auto crawl_url_regex_str(bundle_reader.zotero(section.getSectionName()).value(JournalConfig::Zotero::CRAWL_URL_REGEX, ""));
    if (not crawl_url_regex_str.empty()) {
        // the crawl URL regex needs to be combined with the extraction URL regex if they aren't the same
        // we combine the two here to prevent unnecessary duplication in the config file
        const auto extraction_url_regex_pattern(site_params.extraction_regex_ != nullptr ? site_params.extraction_regex_->getPattern() : "");
        if (not extraction_url_regex_pattern.empty() and extraction_url_regex_pattern != crawl_url_regex_str)
            crawl_url_regex_str = "((" + crawl_url_regex_str + ")|(" + extraction_url_regex_pattern + "))";

        site_desc.url_regex_matcher_.reset(RegexMatcher::RegexMatcherFactoryOrDie(crawl_url_regex_str));
    }

    return Zotero::HarvestSite(site_desc, crawler_params, supported_urls_regex, harvest_params, site_params, error_logger);
}


UnsignedPair ProcessDirectHarvest(const IniFile::Section &section, const JournalConfig::Reader &bundle_reader,
                                  const std::shared_ptr<Zotero::HarvestParams> &harvest_params, const Zotero::SiteParams &site_params,
                                  Zotero::HarvesterErrorLogger * const error_logger)
{
    return Zotero::HarvestURL(bundle_reader.zotero(section.getSectionName()).value(JournalConfig::Zotero::URL),
                              harvest_params, site_params, error_logger);
}


std::string GetOutputFormatString(const std::string &output_filename) {
    const auto extension(FileUtil::GetExtension(output_filename, /*to_lower_case = */ true));
    if (extension == "xml")
        return "marc-xml";
    else if (extension == "mrc")
        return "marc-21";
    else if (extension == "json")
        return "json";

    LOG_ERROR("couldn't determine output format from filename '" + output_filename + "'");
}


std::string GetOutputFormatExtension(const std::string &output_format_string) {
    if (output_format_string == "marc-xml")
        return "xml";
    else if (output_format_string == "marc-21")
        return "mrc";
    else if (output_format_string == "json")
        return "json";

    LOG_ERROR("couldn't determine output extension from format string '" + output_format_string + "'");
}


struct ZoteroFormatHandlerParams {
    DbConnection * const db_connection_;
    std::string output_format_string_;
    std::string output_file_path_;
    std::shared_ptr<Zotero::HarvestParams> harvester_params_;
};


void InitializeFormatHandlerParams(DbConnection * const db_connection, const std::shared_ptr<Zotero::HarvestParams> &harvester_params,
                                   std::string output_format_string, const std::string &output_directory, const std::string &output_filename,
                                   const std::unordered_map<std::string, Zotero::GroupParams> &group_name_to_params_map,
                                   std::unordered_map<std::string, ZoteroFormatHandlerParams> * const group_name_to_format_handler_params_map)
{
    static const std::string time_format_string("%Y-%m-%d %T");

    for (const auto &group_param_entry : group_name_to_params_map) {
        const auto &group_name(group_param_entry.first);
        const auto &bsz_upload_group(group_param_entry.second.bsz_upload_group_);

        char time_buffer[100]{};
        const auto current_time_gmt(TimeUtil::GetCurrentTimeGMT());
        std::strftime(time_buffer, sizeof(time_buffer), time_format_string.c_str(), &current_time_gmt);

        std::string output_file_path(output_directory + "/" + bsz_upload_group);
        std::string output_format;

        if (not output_filename.empty()) {
            output_file_path += "/" + output_filename;
            output_format_string = GetOutputFormatString(output_filename);
        } else
            output_file_path += "/zts_harvester_" + std::string(time_buffer) + "." + GetOutputFormatExtension(output_format_string);

        ZoteroFormatHandlerParams format_handler_params{
            db_connection, output_format_string, output_file_path, harvester_params
        };
        group_name_to_format_handler_params_map->insert(std::make_pair(group_name, format_handler_params));
    }
}


Zotero::FormatHandler *GetFormatHandlerForGroup(
    const std::string &group_name, const std::unordered_map<std::string, ZoteroFormatHandlerParams> &group_name_to_format_handler_params_map)
{
    // lazy-initialize format handlers to prevent file spam in the output directory
    static std::unordered_map<std::string, std::unique_ptr<Zotero::FormatHandler>> group_name_to_format_handler_map;

    const auto match(group_name_to_format_handler_map.find(group_name));
    if (match != group_name_to_format_handler_map.end())
        return match->second.get();

    const auto format_handler_param(group_name_to_format_handler_params_map.at(group_name));
    std::string file_name, directory;
    FileUtil::DirnameAndBasename(format_handler_param.output_file_path_, &directory, &file_name);
    FileUtil::MakeDirectory(directory, true);

    auto handler(Zotero::FormatHandler::Factory(format_handler_param.db_connection_, format_handler_param.output_format_string_,
                    format_handler_param.output_file_path_, format_handler_param.harvester_params_));
    group_name_to_format_handler_map.insert(std::make_pair(group_name, std::move(handler)));
    return group_name_to_format_handler_map.at(group_name).get();
}


// Parses the command-line arguments.
void ProcessArgs(int * const argc, char *** const argv, BSZUpload::DeliveryMode * const delivery_mode_to_process,
                 std::unordered_set<std::string> * const groups_filter, std::unordered_set<std::string> * const zeder_ids_filter,
                 bool * const force_downloads, bool * const ignore_robots_dot_txt,
                 std::string * const map_directory_path, std::string * const output_directory, std::string * const output_filename,
                 std::string * const output_format_string, std::string * const harvest_url_regex, std::string * const harvest_single_url)
{
    while (StringUtil::StartsWith((*argv)[1], "--")) {
        if (StringUtil::StartsWith((*argv)[1], "--delivery-mode=")) {
            const auto mode_string((*argv)[1] + __builtin_strlen("--delivery-mode="));
            const auto match(BSZUpload::STRING_TO_DELIVERY_MODE_MAP.find(mode_string));

            if (match == BSZUpload::STRING_TO_DELIVERY_MODE_MAP.end())
                LOG_ERROR("Unknown delivery mode '" + std::string(mode_string) + "'");
            else
                *delivery_mode_to_process = static_cast<BSZUpload::DeliveryMode>(match->second);

            --*argc, ++*argv;
            continue;
        }

        if (StringUtil::StartsWith((*argv)[1], "--groups=")) {
            StringUtil::SplitThenTrimWhite((*argv)[1] + __builtin_strlen("--groups="), ',', groups_filter);
            --*argc, ++*argv;
            continue;
        }

        if (StringUtil::StartsWith((*argv)[1], "--zeder-ids=")) {
            StringUtil::SplitThenTrimWhite((*argv)[1] + __builtin_strlen("--zeder-ids="), ',', zeder_ids_filter);
            --*argc, ++*argv;
            continue;
        }

        if (std::strcmp((*argv)[1], "--force-downloads") == 0) {
            *force_downloads = true;
            --*argc, ++*argv;
            continue;
        }

        if (std::strcmp((*argv)[1], "--ignore-robots-dot-txt") == 0) {
            *ignore_robots_dot_txt = true;
            --*argc, ++*argv;
            continue;
        }

        const std::string MAP_DIRECTORY_FLAG_PREFIX("--map-directory=");
        if (StringUtil::StartsWith((*argv)[1], MAP_DIRECTORY_FLAG_PREFIX)) {
            *map_directory_path = (*argv)[1] + MAP_DIRECTORY_FLAG_PREFIX.length();
            --*argc, ++*argv;
            continue;
        }

        const std::string OUTPUT_DIRECTORY_FLAG_PREFIX("--output-directory=");
        if (StringUtil::StartsWith((*argv)[1], OUTPUT_DIRECTORY_FLAG_PREFIX)) {
            *output_directory = (*argv)[1] + OUTPUT_DIRECTORY_FLAG_PREFIX.length();
            --*argc, ++*argv;
            continue;
        }

        const std::string OUTPUT_FILENAME_FLAG_PREFIX("--output-filename=");
        if (StringUtil::StartsWith((*argv)[1], OUTPUT_FILENAME_FLAG_PREFIX)) {
            *output_filename = (*argv)[1] + OUTPUT_FILENAME_FLAG_PREFIX.length();
            --*argc, ++*argv;
            continue;
        }

        const std::string OUTPUT_FORMAT_FLAG_PREFIX("--output-format=");
        if (StringUtil::StartsWith((*argv)[1], OUTPUT_FORMAT_FLAG_PREFIX)) {
            *output_format_string = (*argv)[1] + OUTPUT_FORMAT_FLAG_PREFIX.length();
            --*argc, ++*argv;
            continue;
        }

        const std::string HARVEST_URL_REGEX_PREFIX("--harvest-url-regex=");
        if (StringUtil::StartsWith((*argv)[1], HARVEST_URL_REGEX_PREFIX)) {
            *harvest_url_regex = (*argv)[1] + HARVEST_URL_REGEX_PREFIX.length();
            --*argc, ++*argv;
            continue;
        }

        const std::string HARVEST_SINGLE_URL_PREFIX("--harvest-single-url=");
        if (StringUtil::StartsWith((*argv)[1], HARVEST_SINGLE_URL_PREFIX)) {
            *harvest_single_url = (*argv)[1] + HARVEST_SINGLE_URL_PREFIX.length();
            --*argc, ++*argv;
            continue;
        }

        Usage();
    }
}


void HarvestSingleURLWithDummyData(const std::string &url, const IniFile &ini_file,
                                   const std::shared_ptr<Zotero::HarvestParams> &harvest_params,
                                   Zotero::SiteParams * const site_params, Zotero::HarvesterErrorLogger * const harvester_error_logger)
{
    site_params->journal_name_ = "Single URL Test";
    site_params->ISSN_online_ = "2167-2040";
    site_params->PPN_online_ = "696793393";

    const auto common_strptime_format(ini_file.getString("", "common_strptime_format"));
    if (not common_strptime_format.empty()) {
        if (common_strptime_format[0] == '(')
            LOG_ERROR("Cannot specify locale in common_strptime_format");
        site_params->strptime_format_ = common_strptime_format;
    }

    site_params->zeder_id_ = "0";
    site_params->journal_update_window_ = 0;

    Zotero::HarvestURL(url, harvest_params, *site_params, harvester_error_logger);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 2)
        Usage();

    // Handle options independent of the order
    BSZUpload::DeliveryMode delivery_mode_to_process(BSZUpload::DeliveryMode::NONE);
    std::unordered_set<std::string> groups_filter, zeder_ids_filter;
    bool force_downloads(false);
    bool ignore_robots_dot_txt(false);
    std::string map_directory_path;
    std::string output_directory;
    std::string output_filename;
    std::string output_format_string("marc-xml");
    std::string harvest_url_regex;
    std::string harvest_single_url;
    ProcessArgs(&argc, &argv, &delivery_mode_to_process, &groups_filter, &zeder_ids_filter, &force_downloads, &ignore_robots_dot_txt,
                &map_directory_path, &output_directory, &output_filename, &output_format_string, &harvest_url_regex, &harvest_single_url);

    if (argc < 2)
        Usage();

    IniFile ini_file(argv[1]);
    JournalConfig::Reader bundle_reader(ini_file);
    Zotero::HarvesterErrorLogger harvester_error_logger;

    std::shared_ptr<Zotero::HarvestParams> harvest_params(new Zotero::HarvestParams);
    harvest_params->zts_server_url_ = Zotero::TranslationServer::GetUrl();
    harvest_params->force_downloads_ = force_downloads;
    harvest_params->journal_harvest_interval_ = ini_file.getUnsigned("", "journal_harvest_interval");
    harvest_params->force_process_feeds_with_no_pub_dates_ = ini_file.getBool("", "force_process_feeds_with_no_pub_dates");
    harvest_params->default_crawl_delay_time_ = ini_file.getUnsigned("", "default_crawl_delay_time");
    harvest_params->skip_online_first_articles_unconditionally_ = ini_file.getBool("", "skip_online_first_articles_unconditionally");
    if (force_downloads)
        harvest_params->skip_online_first_articles_unconditionally_ = false;
    if (not harvest_url_regex.empty())
        harvest_params->harvest_url_regex_.reset(RegexMatcher::RegexMatcherFactoryOrDie(harvest_url_regex));

    if (map_directory_path.empty())
        map_directory_path = ini_file.getString("", "map_directory_path");

    // ZoteroFormatHandler expects a directory path with a trailing /
    if (not StringUtil::EndsWith(map_directory_path, '/'))
        map_directory_path += "/";

    BSZTransform::AugmentMaps augment_maps(map_directory_path);
    const std::shared_ptr<RegexMatcher> supported_urls_regex(Zotero::LoadSupportedURLsRegex(map_directory_path));

    std::unique_ptr<DbConnection> db_connection(new DbConnection);

    if (output_directory.empty())
        output_directory = ini_file.getString("", "marc_output_directory");

    std::unordered_map<std::string, bool> section_name_to_found_flag_map;
    for (int arg_no(2); arg_no < argc; ++arg_no)
        section_name_to_found_flag_map.emplace(argv[arg_no], false);

    std::map<std::string, int> type_string_to_value_map;
    for (const auto &type : Zotero::HARVESTER_TYPE_TO_STRING_MAP)
        type_string_to_value_map[type.second] = type.first;
    unsigned processed_section_count(0);
    UnsignedPair total_record_count_and_previously_downloaded_record_count;

    std::unordered_set<std::string> group_names;
    std::unordered_map<std::string, Zotero::GroupParams> group_name_to_params_map;
    std::unordered_map<std::string, ZoteroFormatHandlerParams> group_name_to_format_handler_params_map;

    // process groups in advance
    StringUtil::SplitThenTrimWhite(ini_file.getString("", "groups"), ',', &group_names);
    for (const auto &group_name : group_names)
        Zotero::LoadGroup(*ini_file.getSection(group_name), &group_name_to_params_map);
    InitializeFormatHandlerParams(db_connection.get(), harvest_params, output_format_string, output_directory, output_filename,
                                  group_name_to_params_map, &group_name_to_format_handler_params_map);


    if (not harvest_single_url.empty()) {
        const auto group_name_and_params(group_name_to_params_map.find("IxTheo"));
        Zotero::GlobalAugmentParams global_augment_params(&augment_maps);

        Zotero::SiteParams site_params;
        site_params.global_params_ = &global_augment_params;
        site_params.group_params_ = &group_name_and_params->second;
        site_params.delivery_mode_ = BSZUpload::DeliveryMode::NONE;
        harvest_params->format_handler_ = GetFormatHandlerForGroup(site_params.group_params_->name_,
                                                                   group_name_to_format_handler_params_map);
        harvest_params->format_handler_->setAugmentParams(&site_params);
        harvest_params->user_agent_ = group_name_and_params->second.user_agent_;
        HarvestSingleURLWithDummyData(harvest_single_url, ini_file, harvest_params, &site_params, &harvester_error_logger);
        return EXIT_SUCCESS;
    }

    for (const auto &section : ini_file) {
        const auto section_name(section.getSectionName());
        if (section_name.empty() or group_names.find(section_name) != group_names.end())
            continue;

        const BSZUpload::DeliveryMode delivery_mode(static_cast<BSZUpload::DeliveryMode>(BSZUpload::STRING_TO_DELIVERY_MODE_MAP.at(
                                                    bundle_reader.zotero(section_name).value(JournalConfig::Zotero::DELIVERY_MODE,
                                                    BSZUpload::DELIVERY_MODE_TO_STRING_MAP.at(BSZUpload::DeliveryMode::NONE)))));
        if (delivery_mode_to_process != BSZUpload::DeliveryMode::NONE and delivery_mode != delivery_mode_to_process)
            continue;

        const std::string group_name(bundle_reader.zotero(section_name).value(JournalConfig::Zotero::GROUP));
        const auto group_name_and_params(group_name_to_params_map.find(group_name));
        if (group_name_and_params == group_name_to_params_map.cend())
            LOG_ERROR("unknown or undefined group \"" + group_name + "\" in section \"" + section_name + "\"!");
        else if (not groups_filter.empty() and groups_filter.find(group_name) == groups_filter.end())
            continue;

        if (not section_name_to_found_flag_map.empty()) {
            const auto section_name_and_found_flag(section_name_to_found_flag_map.find(section_name));
            if (section_name_and_found_flag == section_name_to_found_flag_map.end())
                continue;
            section_name_and_found_flag->second = true;
        }

        const auto zeder_id(bundle_reader.zeder(section_name).value(JournalConfig::Zeder::ID, ""));
        if (not zeder_ids_filter.empty() and zeder_ids_filter.find(zeder_id) == zeder_ids_filter.end())
            continue;

        LOG_INFO("\n\nProcessing section \"" + section_name + "\".");
        ++processed_section_count;

        Zotero::GlobalAugmentParams global_augment_params(&augment_maps);

        Zotero::SiteParams site_params;
        site_params.global_params_          = &global_augment_params;
        site_params.group_params_           = &group_name_and_params->second;
        site_params.delivery_mode_          = delivery_mode;
        ReadGenericSiteAugmentParams(ini_file, section, bundle_reader, &site_params);

        harvest_params->format_handler_ = GetFormatHandlerForGroup(site_params.group_params_->name_,
                                                                   group_name_to_format_handler_params_map);
        harvest_params->format_handler_->setAugmentParams(&site_params);
        harvest_params->user_agent_ = group_name_and_params->second.user_agent_;

        const Zotero::HarvesterType type(static_cast<Zotero::HarvesterType>(Zotero::STRING_TO_HARVEST_TYPE_MAP.at(bundle_reader
                                                                            .zotero(section_name)
                                                                            .value (JournalConfig::Zotero::TYPE))));
        if (type == Zotero::HarvesterType::RSS) {
            total_record_count_and_previously_downloaded_record_count += ProcessRSSFeed(section, bundle_reader,harvest_params,
                                                                                        site_params, &harvester_error_logger);
        } else if (type == Zotero::HarvesterType::CRAWL) {
            SimpleCrawler::Params crawler_params;
            crawler_params.ignore_robots_dot_txt_ = ignore_robots_dot_txt;
            crawler_params.min_url_processing_time_ = Zotero::DEFAULT_MIN_URL_PROCESSING_TIME;
            crawler_params.timeout_ = Zotero::DEFAULT_TIMEOUT;
            crawler_params.user_agent_ = harvest_params->user_agent_;

            total_record_count_and_previously_downloaded_record_count +=
                ProcessCrawl(section, bundle_reader, harvest_params, site_params, crawler_params, supported_urls_regex,
                             &harvester_error_logger);
        } else {
            total_record_count_and_previously_downloaded_record_count += ProcessDirectHarvest(section, bundle_reader, harvest_params,
                                                                                              site_params, &harvester_error_logger);
        }
    }

    LOG_INFO("Extracted metadata from "
             + std::to_string(total_record_count_and_previously_downloaded_record_count.first
                              - total_record_count_and_previously_downloaded_record_count.second) + " page(s).");

    if (section_name_to_found_flag_map.size() > processed_section_count) {
        std::cerr << "The following sections were specified but not processed:\n";
        for (const auto &section_name_and_found_flag : section_name_to_found_flag_map)
            std::cerr << '\t' << section_name_and_found_flag.first << '\n';
    }

    if (harvester_error_logger.hasErrors())
        LOG_WARNING("Unexpected errors were encountered during the harvesting process");

    return EXIT_SUCCESS;
}
