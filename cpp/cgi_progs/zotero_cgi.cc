/** \file    zotero_cgi.cc
 *  \brief   A CGI-tool to execute Zotero RSS & Crawling mechanisms
 *  \author  Mario Trojan
 */
/*
    Copyright (C) 2016-2020, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "Template.h"
#include "WallClockTimer.h"
#include "WebUtil.h"
#include "UBTools.h"
#include "util.h"
#include "ZoteroHarvesterConfig.h"


namespace {


std::string zts_client_maps_directory;
const std::string ZTS_HARVESTER_CONF_FILE("/usr/local/ub_tools/cpp/data/zotero_harvester.conf");
const std::vector<std::pair<std::string,std::string>> OUTPUT_FORMAT_IDS_AND_EXTENSIONS {
    // custom formats
    { "marc-xml", "xml" }
};


std::vector<std::string> GetOutputFormatIds() {
    std::vector<std::string> output_formats;
    for (const auto &output_format_id_and_extension : OUTPUT_FORMAT_IDS_AND_EXTENSIONS)
        output_formats.push_back(output_format_id_and_extension.first);

    return output_formats;
}


std::string GetOutputFormatExtension(const std::string &output_format_id) {
    for (const auto &output_format_id_and_extension : OUTPUT_FORMAT_IDS_AND_EXTENSIONS) {
        if (output_format_id_and_extension.first == output_format_id)
            return output_format_id_and_extension.second;
    }

    LOG_ERROR("no extension defined for output format " + output_format_id);
    __builtin_unreachable();
}


std::string GetCGIParameterOrDefault(const std::multimap<std::string, std::string> &cgi_args,
                                     const std::string &parameter_name,
                                     const std::string &default_value = "")
{
    const auto key_and_value(cgi_args.find(parameter_name));
    if (key_and_value == cgi_args.cend())
        return default_value;

    return key_and_value->second;
}


std::string GetMinElementOrDefault(const std::vector<std::string> &elements, const std::string &default_value = "") {
    const auto min_element(std::min_element(elements.begin(), elements.end()));
    if (unlikely(min_element == elements.end()))
        return default_value;

    return *min_element;
}


void ParseConfigFile(const std::multimap<std::string, std::string> &cgi_args, Template::Map * const names_to_values_map,
                     std::unordered_map<std::string, ZoteroHarvester::Config::GroupParams> * const group_name_to_params_map,
                     std::unordered_map<std::string, std::string> * const journal_name_to_group_name_map)
{
    std::unique_ptr<ZoteroHarvester::Config::GlobalParams> global_params;
    std::vector<std::unique_ptr<ZoteroHarvester::Config::GroupParams>> group_params;
    std::vector<std::unique_ptr<ZoteroHarvester::Config::JournalParams>> journal_params;

    ZoteroHarvester::Config::LoadHarvesterConfigFile(ZTS_HARVESTER_CONF_FILE, &global_params, &group_params, &journal_params);

    std::vector<std::string> all_journal_titles;
    std::vector<std::string> all_journal_print_issns;
    std::vector<std::string> all_journal_online_issns;
    std::vector<std::string> all_journal_print_ppns;
    std::vector<std::string> all_journal_online_ppns;
    std::vector<std::string> all_journal_methods;
    std::vector<std::string> all_journal_groups;
    std::vector<std::string> all_journal_delivery_modes;
    std::vector<std::string> all_journal_zeder_ids;
    std::vector<std::string> all_journal_zeder_urls;
    std::vector<std::string> all_urls;

    std::vector<std::string> rss_journal_titles;
    std::vector<std::string> rss_journal_print_issns;
    std::vector<std::string> rss_journal_online_issns;
    std::vector<std::string> rss_journal_print_ppns;
    std::vector<std::string> rss_journal_online_ppns;
    std::vector<std::string> rss_feed_urls;
    std::vector<std::string> rss_strptime_formats;

    std::vector<std::string> direct_journal_titles;
    std::vector<std::string> direct_journal_print_issns;
    std::vector<std::string> direct_journal_online_issns;
    std::vector<std::string> direct_journal_print_ppns;
    std::vector<std::string> direct_journal_online_ppns;
    std::vector<std::string> direct_urls;
    std::vector<std::string> direct_strptime_formats;

    std::vector<std::string> crawling_journal_titles;
    std::vector<std::string> crawling_journal_print_issns;
    std::vector<std::string> crawling_journal_online_issns;
    std::vector<std::string> crawling_journal_print_ppns;
    std::vector<std::string> crawling_journal_online_ppns;
    std::vector<std::string> crawling_base_urls;
    std::vector<std::string> crawling_extraction_regexes;
    std::vector<std::string> crawling_depths;
    std::vector<std::string> crawling_strptime_formats;

    std::unordered_set<std::string> group_names;

    zts_client_maps_directory = global_params->enhancement_maps_directory_;
    for (const auto &group : group_params)
        group_name_to_params_map->emplace(group->name_, *group);

    for (const auto &journal_param : journal_params) {
        const auto &title(journal_param->name_);
        const auto &harvest_type_raw(ZoteroHarvester::Config::HARVESTER_OPERATION_TO_STRING_MAP.at(journal_param->harvester_operation_));
        const auto &harvest_type(journal_param->harvester_operation_);
        const auto &delivery_mode(journal_param->upload_operation_);
        const auto &issn_print(journal_param->issn_.print_);
        const auto &ppn_print(journal_param->ppn_.print_);
        const auto &issn_online(journal_param->issn_.online_);
        const auto &ppn_online(journal_param->ppn_.online_);
        const auto &group(journal_param->group_);
        const auto &url(journal_param->entry_point_url_);
        const auto &strptime_format(journal_param->strptime_format_string_);
        const auto &zeder_id(journal_param->zeder_id_);

        std::string zeder_url;
        if (group == "IxTheo" or group == "RelBib")
            zeder_url = "http://www-ub.ub.uni-tuebingen.de/zeder/?instanz=ixtheo#suche=Z%3D" + std::to_string(zeder_id);
        else if (group == "KrimDok")
            zeder_url = "http://www-ub.ub.uni-tuebingen.de/zeder/?instanz=krim#suche=Z%3D" + std::to_string(zeder_id);

        all_journal_titles.emplace_back(title);
        all_journal_print_issns.emplace_back(issn_print);
        all_journal_online_issns.emplace_back(issn_online);
        all_journal_print_ppns.emplace_back(ppn_print);
        all_journal_online_ppns.emplace_back(ppn_online);
        all_journal_groups.emplace_back(group);
        all_journal_methods.emplace_back(harvest_type_raw);
        all_journal_zeder_ids.emplace_back(std::to_string(zeder_id));
        all_journal_zeder_urls.emplace_back(zeder_url);

        journal_name_to_group_name_map->insert(std::make_pair(title, group));

        all_journal_delivery_modes.emplace_back(ZoteroHarvester::Config::UPLOAD_OPERATION_TO_STRING_MAP.at(delivery_mode));
        all_urls.emplace_back(url);

        if (harvest_type == ZoteroHarvester::Config::HarvesterOperation::RSS) {
            rss_journal_titles.emplace_back(title);
            rss_journal_print_issns.emplace_back(issn_print);
            rss_journal_online_issns.emplace_back(issn_online);
            rss_journal_print_ppns.emplace_back(ppn_print);
            rss_journal_online_ppns.emplace_back(ppn_online);
            rss_feed_urls.emplace_back(url);
            rss_strptime_formats.emplace_back(strptime_format);
        } else if (harvest_type == ZoteroHarvester::Config::HarvesterOperation::DIRECT) {
            direct_journal_titles.emplace_back(title);
            direct_journal_print_issns.emplace_back(issn_print);
            direct_journal_online_issns.emplace_back(issn_online);
            direct_journal_print_ppns.emplace_back(ppn_print);
            direct_journal_online_ppns.emplace_back(ppn_online);
            direct_urls.emplace_back(url);
            direct_strptime_formats.emplace_back(strptime_format);
        } else if (harvest_type == ZoteroHarvester::Config::HarvesterOperation::CRAWL) {
            crawling_journal_titles.emplace_back(title);
            crawling_journal_print_issns.emplace_back(issn_print);
            crawling_journal_online_issns.emplace_back(issn_online);
            crawling_journal_print_ppns.emplace_back(ppn_print);
            crawling_journal_online_ppns.emplace_back(ppn_online);
            crawling_base_urls.emplace_back(url);
            crawling_extraction_regexes.emplace_back(journal_param->crawl_params_.extraction_regex_->getPattern());
            crawling_depths.emplace_back(std::to_string(journal_param->crawl_params_.max_crawl_depth_));
            crawling_strptime_formats.emplace_back(strptime_format);
        }
    }

    names_to_values_map->insertScalar("zotero_translation_server_url", global_params->translation_server_url_);

    names_to_values_map->insertArray("all_journal_titles", all_journal_titles);
    names_to_values_map->insertArray("all_journal_print_issns", all_journal_print_issns);
    names_to_values_map->insertArray("all_journal_online_issns", all_journal_online_issns);
    names_to_values_map->insertArray("all_journal_print_ppns", all_journal_print_ppns);
    names_to_values_map->insertArray("all_journal_online_ppns", all_journal_online_ppns);
    names_to_values_map->insertArray("all_journal_methods", all_journal_methods);
    names_to_values_map->insertArray("all_journal_groups", all_journal_groups);
    names_to_values_map->insertArray("all_journal_delivery_modes", all_journal_delivery_modes);
    names_to_values_map->insertArray("all_journal_zeder_ids", all_journal_zeder_ids);
    names_to_values_map->insertArray("all_journal_zeder_urls", all_journal_zeder_urls);
    names_to_values_map->insertArray("all_urls", all_urls);

    names_to_values_map->insertArray("rss_journal_titles", rss_journal_titles);
    names_to_values_map->insertArray("rss_journal_print_issns", rss_journal_print_issns);
    names_to_values_map->insertArray("rss_journal_online_issns", rss_journal_online_issns);
    names_to_values_map->insertArray("rss_journal_print_ppns", rss_journal_print_ppns);
    names_to_values_map->insertArray("rss_journal_online_ppns", rss_journal_online_ppns);
    names_to_values_map->insertArray("rss_feed_urls", rss_feed_urls);
    names_to_values_map->insertArray("rss_strptime_formats", rss_strptime_formats);

    names_to_values_map->insertArray("direct_journal_titles", direct_journal_titles);
    names_to_values_map->insertArray("direct_journal_print_issns", direct_journal_print_issns);
    names_to_values_map->insertArray("direct_journal_online_issns", direct_journal_online_issns);
    names_to_values_map->insertArray("direct_journal_print_ppns", direct_journal_print_ppns);
    names_to_values_map->insertArray("direct_journal_online_ppns", direct_journal_online_ppns);
    names_to_values_map->insertArray("direct_urls", direct_urls);
    names_to_values_map->insertArray("direct_strptime_formats", direct_strptime_formats);

    names_to_values_map->insertArray("crawling_journal_titles", crawling_journal_titles);
    names_to_values_map->insertArray("crawling_journal_print_issns", crawling_journal_print_issns);
    names_to_values_map->insertArray("crawling_journal_online_issns", crawling_journal_online_issns);
    names_to_values_map->insertArray("crawling_journal_print_ppns", crawling_journal_print_ppns);
    names_to_values_map->insertArray("crawling_journal_online_ppns", crawling_journal_online_ppns);
    names_to_values_map->insertArray("crawling_base_urls", crawling_base_urls);
    names_to_values_map->insertArray("crawling_extraction_regexes", crawling_extraction_regexes);
    names_to_values_map->insertArray("crawling_depths", crawling_depths);
    names_to_values_map->insertArray("crawling_strptime_formats", crawling_strptime_formats);

    const std::string first_crawling_journal_title(GetMinElementOrDefault(crawling_journal_titles));
    names_to_values_map->insertScalar("selected_crawling_journal_title", GetCGIParameterOrDefault(cgi_args, "crawling_journal_title",
                                                                                                  first_crawling_journal_title));

    const std::string first_direct_journal_title(GetMinElementOrDefault(crawling_journal_titles));
    names_to_values_map->insertScalar("selected_direct_journal_title", GetCGIParameterOrDefault(cgi_args, "direct_journal_title",
                                                                                                first_direct_journal_title));

    const std::string first_rss_journal_title(GetMinElementOrDefault(rss_journal_titles));
    names_to_values_map->insertScalar("selected_rss_journal_title", GetCGIParameterOrDefault(cgi_args, "rss_journal_title",
                                                                                             first_rss_journal_title));
}


std::string BuildCommandString(const std::string &command, const std::vector<std::string> &args) {
    std::string command_string(command);

    for (const std::string &arg : args)
        command_string += " \"" + arg + "\"";

    return command_string;
}


void UpdateRuntime(unsigned seconds) {
    std::cout << "<script type=\"text/javascript\">UpdateRuntime(" + std::to_string(seconds) + ");</script>\r\n";
    std::cout << std::flush;
}


/** \brief class for executing zts_harvester & access its results */
class HarvestTask {
    FileUtil::AutoTempDirectory auto_temp_dir_;
    std::string executable_;
    std::string command_;
    int pid_;
    int exit_code_;
    FileUtil::AutoTempFile log_path_;
    std::unique_ptr<FileUtil::AutoTempFile> out_path_;
public:
    HarvestTask(const std::string &section, const std::string &output_format_id, const std::string &bsz_upload_group);

    /** \brief get shell command including args (for debug output) */
    inline const std::string &getCommand() const { return command_; }
    inline int getExitCode() const { return exit_code_; }
    inline int getPid() const { return pid_; }
    inline const std::string &getLogPath() const { return log_path_.getFilePath(); }

    /** \brief get path to out file with harvested records */
    inline const std::string &getOutPath() const { return out_path_->getFilePath(); }
};


HarvestTask::HarvestTask(const std::string &section, const std::string &output_format_id, const std::string &bsz_upload_group)
    : auto_temp_dir_("/tmp/ZtsMaps_", /*cleanup_if_exception_is_active*/ false, /*remove_when_out_of_scope*/ false),
      executable_(ExecUtil::LocateOrDie("zotero_harvester")),
      log_path_(auto_temp_dir_.getDirectoryPath() + "/log", "", /*automatically_remove*/ false)
{
    const auto output_directory(auto_temp_dir_.getDirectoryPath() + "/" + bsz_upload_group + "/");
    FileUtil::MakeDirectory(output_directory, true);
    out_path_.reset(new FileUtil::AutoTempFile(output_directory, "." + GetOutputFormatExtension(output_format_id),
                    /*automatically_remove*/ false));

    std::string dir_name, basename;
    FileUtil::DirnameAndBasename(out_path_->getFilePath(), &dir_name, &basename);

    std::vector<std::string> args;
    args.emplace_back("--min-log-level=DEBUG");
    args.emplace_back("--force-downloads");
    args.emplace_back("--output-directory=" + auto_temp_dir_.getDirectoryPath());
    args.emplace_back("--output-filename=" + basename);
    args.emplace_back(ZTS_HARVESTER_CONF_FILE);
    args.emplace_back("JOURNAL");
    args.emplace_back(section);

    std::unordered_map<std::string, std::string> envs {
        { "LOGGER_FORMAT",  "no_decorations,strip_call_site" },
        { "UTIL_LOG_DEBUG", "true"                           },
        { "BACKTRACE",      "1"                              },
    };

    command_ = BuildCommandString(executable_, args);
    const std::string log_path(auto_temp_dir_.getDirectoryPath() + "/log");
    pid_ = ExecUtil::Spawn(executable_, args, "", log_path_.getFilePath(), log_path_.getFilePath(), envs);
}


void ExecuteHarvestAction(const std::string &title, const std::string &output_format,
                          const ZoteroHarvester::Config::GroupParams &group_params) {
    std::cout << "<h2>Result</h2>\r\n";
    std::cout << "<table>\r\n";

    const HarvestTask task(title, output_format, group_params.output_folder_);

    std::cout << "<tr><td>Command</td><td>" + task.getCommand() + "</td></tr>\r\n";
    std::cout << "<tr><td>Runtime</td><td id=\"runtime\"></td></tr>\r\n";
    std::cout << std::flush;

    // start status monitoring
    WallClockTimer timer(WallClockTimer::CUMULATIVE_WITH_AUTO_START);
    int status;

    do {
        ::sleep(1);
        timer.stop();
        UpdateRuntime(static_cast<unsigned>(timer.getTime()));
        timer.start();
    } while (::waitpid(task.getPid(), &status, WNOHANG) >= 0);
    timer.stop();

    int exit_code(-2);
    if WIFEXITED(status)
        exit_code = WEXITSTATUS(status);

    std::string output;
    if (not FileUtil::ReadString(task.getLogPath(), &output))
        output = "could not read log file!";

    if (exit_code == 0)
        std::cout << "<tr><td>Download</td><td><a target=\"_blank\" href=\"?action=download&id=" + task.getOutPath() + "\">Result file</a></td></tr>\r\n";
    else
        std::cout << "<tr><td>ERROR</td><td>Exitcode: " + std::to_string(exit_code) + "</td></tr>\r\n";

    // use <pre> instead of nl2br + htmlspecialchars
    std::cout << "<tr><td>CLI output:</td><td><pre>" + output + "</td></tr>\r\n";
    std::cout << "</table>\r\n";
}


void ProcessDownloadAction(const std::multimap<std::string, std::string> &cgi_args) {
    const std::string path(GetCGIParameterOrDefault(cgi_args, "id"));

    if (StringUtil::EndsWith(path, ".xml", /*ignore_case*/ true))
        std::cout << "Content-Type: application/xml; charset=utf-8\r\n\r\n";
    else
        std::cout << "Content-Type: text/plain; charset=utf-8\r\n\r\n";

    std::cout << FileUtil::ReadStringOrDie(path);
}


const std::string TEMPLATE_DIRECTORY(UBTools::GetTuelibPath() + "zotero_cgi/");


} // unnamed namespace


int Main(int argc, char *argv[]) {
    ::progname = argv[0];

    std::multimap<std::string, std::string> cgi_args;
    WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);

    const std::string default_action("list");
    const std::string action(GetCGIParameterOrDefault(cgi_args, "action", default_action));

    if (action == "download")
        ProcessDownloadAction(cgi_args);
    else {
        std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";

        Template::Map names_to_values_map;
        names_to_values_map.insertScalar("action", action);

        std::string style_css;
        FileUtil::ReadString(TEMPLATE_DIRECTORY + "style.css", &style_css);
        names_to_values_map.insertScalar("style_css", style_css);

        std::string scripts_js;
        FileUtil::ReadString(TEMPLATE_DIRECTORY + "scripts.js", &scripts_js);
        names_to_values_map.insertScalar("scripts_js", scripts_js);

        const std::string depth(GetCGIParameterOrDefault(cgi_args, "depth", "1"));
        names_to_values_map.insertScalar("depth", depth);

        const std::string selected_output_format_id(GetCGIParameterOrDefault(cgi_args, "output_format_id"));
        names_to_values_map.insertScalar("selected_output_format_id", selected_output_format_id);
        names_to_values_map.insertArray("output_format_ids", GetOutputFormatIds());

        const std::string TEMPLATE_FILENAME(TEMPLATE_DIRECTORY + "index.html");
        std::string error_message;
        if (not FileUtil::IsReadable(TEMPLATE_FILENAME, &error_message))
            LOG_ERROR(error_message);

        names_to_values_map.insertScalar("running_processes_count", std::to_string(ExecUtil::FindActivePrograms("zotero_harvester").size()));

        std::ifstream template_html(TEMPLATE_FILENAME);
        std::unordered_map<std::string, ZoteroHarvester::Config::GroupParams> group_name_to_params_map;
        std::unordered_map<std::string, std::string>journal_name_to_group_name_map;
        ParseConfigFile(cgi_args, &names_to_values_map, &group_name_to_params_map, &journal_name_to_group_name_map);
        Template::ExpandTemplate(template_html, std::cout, names_to_values_map);
        std::cout << std::flush;

        std::string journal_title, output_format;
        if (action == "rss") {
            journal_title = GetCGIParameterOrDefault(cgi_args, "rss_journal_title");
            output_format = GetCGIParameterOrDefault(cgi_args, "rss_output_format");
        } else if (action == "direct") {
            journal_title = GetCGIParameterOrDefault(cgi_args, "direct_journal_title");
            output_format = GetCGIParameterOrDefault(cgi_args, "direct_output_format");
        } else if (action == "crawling") {
            journal_title = GetCGIParameterOrDefault(cgi_args, "crawling_journal_title");
            output_format = GetCGIParameterOrDefault(cgi_args, "crawling_output_format");
        } else if (action != default_action)
            LOG_ERROR("invalid action: \"" + action + '"');

        ExecuteHarvestAction(journal_title, output_format, group_name_to_params_map.at(journal_name_to_group_name_map.at(journal_title)));
        std::cout << "</body></html>";
    }

    return EXIT_SUCCESS;
}
