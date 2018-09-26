/** \file    zotero_cgi.cc
 *  \brief   A CGI-tool to execute Zotero RSS & Crawling mechanisms
 *  \author  Mario Trojan
 */
/*
    Copyright (C) 2016-2018, Library of the University of Tübingen

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
#include "IniFile.h"
#include "StringUtil.h"
#include "Template.h"
#include "TextUtil.h"
#include "WallClockTimer.h"
#include "WebUtil.h"
#include "Zotero.h"
#include "util.h"


namespace {


std::string zts_client_maps_directory;
std::string zts_url(Zotero::TranslationServer::GetUrl());
const std::string ZTS_HARVESTER_CONF_FILE("/usr/local/ub_tools/cpp/data/zts_harvester.conf");
const std::vector<std::pair<std::string,std::string>> OUTPUT_FORMAT_IDS_AND_EXTENSIONS {
    // custom formats
    { "marcxml", "xml" },
    { "marc21", "mrc" },
    { "json", "json" },

    // native zotero formats, see https://github.com/zotero/translation-server/blob/master/src/server_translation.js#L31-43
    { "bibtex", "bibtex" },
    { "biblatex", "biblatex" },
    { "bookmarks", "bookmarks" },
    { "coins", "coins" },
    { "csljson", "csljson" },
    { "mods", "mods" },
    { "refer", "refer" },
    { "rdf_bibliontology", "rdf_bib" },
    { "rdf_dc", "rdf_dc" },
    { "rdf_zotero", "rdf_zotero" },
    { "ris", "ris" },
    { "tei", "tei" },
    { "wikipedia", "wikipedia" }
};


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


void ParseConfigFile(const std::multimap<std::string, std::string> &cgi_args, Template::Map * const names_to_values_map) {
    IniFile ini(ZTS_HARVESTER_CONF_FILE);

    std::vector<std::string> all_journal_titles;
    std::vector<std::string> all_journal_print_issns;
    std::vector<std::string> all_journal_online_issns;
    std::vector<std::string> all_journal_print_ppns;
    std::vector<std::string> all_journal_online_ppns;
    std::vector<std::string> all_journal_methods;
    std::vector<std::string> all_journal_groups;
    std::vector<std::string> all_journal_delivery_modes;
    std::vector<std::string> all_journal_zeder_ids;
    std::vector<std::string> all_journal_zeder_comments;
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

    std::set<std::string> group_names;
    for (const auto &section : ini) {
        const std::string &title(section.getSectionName());
        if (title.empty()) {
            zts_client_maps_directory = section.getString("map_directory_path");
            StringUtil::SplitThenTrimWhite(section.getString("groups"), ',', &group_names);
        } else {
            if (group_names.find(title) != group_names.cend())
                continue;

            const Zotero::HarvesterType harvest_type(static_cast<Zotero::HarvesterType>(section.getEnum(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::TYPE), Zotero::STRING_TO_HARVEST_TYPE_MAP)));
            const std::string harvest_type_raw(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::TYPE)));
            const BSZUpload::DeliveryMode delivery_mode(static_cast<BSZUpload::DeliveryMode>(section.getEnum("zotero_delivery_mode", BSZUpload::STRING_TO_DELIVERY_MODE_MAP, BSZUpload::DeliveryMode::NONE)));
            const std::string issn_print(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_ISSN_PRINT), ""));
            const std::string issn_online(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_ISSN_ONLINE), ""));
            const std::string ppn_print(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_PPN), ""));
            const std::string ppn_online(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::PARENT_PPN), ""));
            const std::string group(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::GROUP)));
            const std::string zeder_id(section.getString("zeder_id", ""));
            const std::string zeder_comment(section.getString("zeder_comment", ""));

            std::string zeder_url;
            if (not zeder_id.empty()) {
                if (group == "KrimDok")
                    zeder_url = "http://www-ub.ub.uni-tuebingen.de/zeder/?instanz=ixtheo#suche=Z%3D" + zeder_id;
                else if (group == "IxTheo")
                    zeder_url = "http://www-ub.ub.uni-tuebingen.de/zeder/?instanz=krim#suche=Z%3D" + zeder_id;
            }

            all_journal_titles.emplace_back(title);
            all_journal_print_issns.emplace_back(issn_print);
            all_journal_online_issns.emplace_back(issn_online);
            all_journal_print_ppns.emplace_back(ppn_print);
            all_journal_online_ppns.emplace_back(ppn_online);
            all_journal_groups.emplace_back(group);
            all_journal_methods.emplace_back(harvest_type_raw);
            all_journal_zeder_ids.emplace_back(zeder_id);
            all_journal_zeder_comments.emplace_back(zeder_comment);
            all_journal_zeder_urls.emplace_back(zeder_url);

            const auto delivery_mode_string(std::find_if(BSZUpload::STRING_TO_DELIVERY_MODE_MAP.begin(), BSZUpload::STRING_TO_DELIVERY_MODE_MAP.end(), [delivery_mode](const std::pair<std::string, int> &map_entry) {return map_entry.second == delivery_mode; })->first);
            all_journal_delivery_modes.emplace_back(delivery_mode_string);
            all_urls.emplace_back(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::URL)));

            if (harvest_type == Zotero::HarvesterType::RSS) {

                rss_journal_titles.emplace_back(title);
                rss_journal_print_issns.emplace_back(issn_print);
                rss_journal_online_issns.emplace_back(issn_online);
                rss_journal_print_ppns.emplace_back(ppn_print);
                rss_journal_online_ppns.emplace_back(ppn_online);
                rss_feed_urls.emplace_back(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::FEED)));
                rss_strptime_formats.emplace_back(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::STRPTIME_FORMAT), ""));
            } else if (harvest_type == Zotero::HarvesterType::DIRECT) {
                direct_journal_titles.emplace_back(title);
                direct_journal_print_issns.emplace_back(issn_print);
                direct_journal_online_issns.emplace_back(issn_online);
                direct_journal_print_ppns.emplace_back(ppn_print);
                direct_journal_online_ppns.emplace_back(ppn_online);
                direct_urls.emplace_back(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::URL)));
                direct_strptime_formats.emplace_back(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::STRPTIME_FORMAT), ""));
            } else if (harvest_type == Zotero::HarvesterType::CRAWL) {
                crawling_journal_titles.emplace_back(title);
                crawling_journal_print_issns.emplace_back(issn_print);
                crawling_journal_online_issns.emplace_back(issn_online);
                crawling_journal_print_ppns.emplace_back(ppn_print);
                crawling_journal_online_ppns.emplace_back(ppn_online);
                crawling_base_urls.emplace_back(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::BASE_URL)));
                crawling_extraction_regexes.emplace_back(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::EXTRACTION_REGEX)));
                crawling_depths.emplace_back(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::MAX_CRAWL_DEPTH)));
                crawling_strptime_formats.emplace_back(section.getString(Zotero::HARVESTER_CONFIG_ENTRY_TO_STRING_MAP.at(Zotero::HarvesterConfigEntry::STRPTIME_FORMAT), ""));
            }
        }
    }

    if (zts_url.empty())
        LOG_ERROR("Zotero Translation Server Url not defined in config file!");
    if (zts_client_maps_directory.empty())
        LOG_ERROR("Zotero Mapping Directory not defined in config file!");
    names_to_values_map->insertScalar("zotero_translation_server_url", zts_url);

    names_to_values_map->insertArray("all_journal_titles", all_journal_titles);
    names_to_values_map->insertArray("all_journal_print_issns", all_journal_print_issns);
    names_to_values_map->insertArray("all_journal_online_issns", all_journal_online_issns);
    names_to_values_map->insertArray("all_journal_print_ppns", all_journal_print_ppns);
    names_to_values_map->insertArray("all_journal_online_ppns", all_journal_online_ppns);
    names_to_values_map->insertArray("all_journal_methods", all_journal_methods);
    names_to_values_map->insertArray("all_journal_groups", all_journal_groups);
    names_to_values_map->insertArray("all_journal_delivery_modes", all_journal_delivery_modes);
    names_to_values_map->insertArray("all_journal_zeder_ids", all_journal_zeder_ids);
    names_to_values_map->insertArray("all_journal_zeder_comments", all_journal_zeder_comments);
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
}


std::string BuildCommandString(const std::string &command, const std::vector<std::string> &args) {
    std::string command_string(command);

    for (const std::string &arg : args)
        command_string += " \"" + arg + "\"";

    return command_string;
}


/** \brief create local copy of template map directory and delete previously downloaded cache for clean test results */
std::string PrepareMapsDirectory(const std::string &orig_directory, const std::string &tmp_directory) {
    ExecUtil::ExecOrDie(ExecUtil::LocateOrDie("cp"), { "-r", orig_directory, tmp_directory });
    const std::string local_maps_directory(tmp_directory + "/zotero-enhancement-maps");
    return local_maps_directory;
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
    std::string log_path_;
    std::string out_path_;
public:
    HarvestTask(const std::string &section, const std::string &output_format_id);

    /** \brief get shell command including args (for debug output) */
    inline const std::string &getCommand() const { return command_; }
    inline int getExitCode() const { return exit_code_; }
    inline int getPid() const { return pid_; }
    inline const std::string &getLogPath() const { return log_path_; }

    /** \brief get path to out file with harvested records */
    inline const std::string &getOutPath() const { return out_path_; }
};


HarvestTask::HarvestTask(const std::string &section, const std::string &output_format_id)
    : auto_temp_dir_("/tmp/ZtsMaps_", /*cleanup_if_exception_is_active*/ false, /*remove_when_out_of_scope*/ false),
      executable_(ExecUtil::LocateOrDie("zts_harvester"))
{
    const std::string local_maps_directory(PrepareMapsDirectory(zts_client_maps_directory, auto_temp_dir_.getDirectoryPath()));
    const std::string file_extension(GetOutputFormatExtension(output_format_id));
    log_path_ = auto_temp_dir_.getDirectoryPath() + "/log";
    out_path_ = auto_temp_dir_.getDirectoryPath() + "/output." + file_extension;

    std::vector<std::string> args;
    args.emplace_back("--min-log-level=DEBUG");
    args.emplace_back("--map-directory=" + local_maps_directory);
    args.emplace_back("--output-file=" + out_path_);
    args.emplace_back(ZTS_HARVESTER_CONF_FILE);
    args.emplace_back(section);

    command_ = BuildCommandString(executable_, args);
    const std::string log_path(auto_temp_dir_.getDirectoryPath() + "/log");
    pid_ = ExecUtil::Spawn(executable_, args, "", log_path_, log_path_);
}


void ExecuteHarvestAction(const std::string &title, const std::string &output_format) {
    std::cout << "<h2>Result</h2>\r\n";
    std::cout << "<table>\r\n";

    const HarvestTask task(title, output_format);

    std::cout << "<tr><td>Command</td><td>" + task.getCommand() + "</td></tr>\r\n";
    std::cout << "<tr><td>Runtime</td><td id=\"runtime\"></td></tr>\r\n";
    //std::cout << "<tr><td>Progress</td><td><div id=\"progress\">Harvesting...</div></td></tr>\r\n";
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


const std::string TEMPLATE_DIRECTORY("/usr/local/var/lib/tuelib/zotero_cgi/");


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

        names_to_values_map.insertScalar("running_processes_count", std::to_string(ExecUtil::FindActivePrograms("zts_harvester").size()));

        std::ifstream template_html(TEMPLATE_FILENAME);
        ParseConfigFile(cgi_args, &names_to_values_map);
        Template::ExpandTemplate(template_html, std::cout, names_to_values_map);
        std::cout << std::flush;

        if (action == "rss")
            ExecuteHarvestAction(GetCGIParameterOrDefault(cgi_args, "rss_journal_title"), GetCGIParameterOrDefault(cgi_args, "rss_output_format"));
        else if (action == "direct")
            ExecuteHarvestAction(GetCGIParameterOrDefault(cgi_args, "direct_journal_title"), GetCGIParameterOrDefault(cgi_args, "direct_output_format"));
        else if (action == "crawling")
            ExecuteHarvestAction(GetCGIParameterOrDefault(cgi_args, "crawling_journal_title"), GetCGIParameterOrDefault(cgi_args, "crawling_output_format"));
        else if (action != default_action)
            LOG_ERROR("invalid action: \"" + action + '"');

        std::cout << "</body></html>";
    }

    return EXIT_SUCCESS;
}
