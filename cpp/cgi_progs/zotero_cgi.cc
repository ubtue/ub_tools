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
#include "DbConnection.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "HtmlUtil.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "Template.h"
#include "WallClockTimer.h"
#include "WebUtil.h"
#include "UBTools.h"
#include "util.h"
#include "Zeder.h"
#include "ZoteroHarvesterConfig.h"
#include "ZoteroHarvesterUtil.h"


namespace {


std::string zts_client_maps_directory;
const std::string ZTS_HARVESTER_CONF_FILE(UBTools::GetTuelibPath() + "zotero-enhancement-maps/zotero_harvester.conf");


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
        } else if (harvest_type == ZoteroHarvester::Config::HarvesterOperation::CRAWL) {
            crawling_journal_titles.emplace_back(title);
            crawling_journal_print_issns.emplace_back(issn_print);
            crawling_journal_online_issns.emplace_back(issn_online);
            crawling_journal_print_ppns.emplace_back(ppn_print);
            crawling_journal_online_ppns.emplace_back(ppn_online);
            crawling_base_urls.emplace_back(url);
            if (journal_param->crawl_params_.extraction_regex_ != nullptr)
                crawling_extraction_regexes.emplace_back(journal_param->crawl_params_.extraction_regex_->getPattern());
            else
                crawling_extraction_regexes.emplace_back("");
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

    const std::string first_rss_journal_title(GetMinElementOrDefault(rss_journal_titles));
    names_to_values_map->insertScalar("selected_rss_journal_title", GetCGIParameterOrDefault(cgi_args, "rss_journal_title",
                                                                                             first_rss_journal_title));

    names_to_values_map->insertScalar("selected_url_journal_title", GetCGIParameterOrDefault(cgi_args, "url_journal_title"));
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
    HarvestTask(const std::string &title, const std::string &bsz_upload_group,
                const std::string &url = "", const std::string &config_overrides = "");

    /** \brief get shell command including args (for debug output) */
    inline const std::string &getCommand() const { return command_; }
    inline int getExitCode() const { return exit_code_; }
    inline int getPid() const { return pid_; }
    inline const std::string &getLogPath() const { return log_path_.getFilePath(); }

    /** \brief get path to out file with harvested records */
    inline const std::string &getOutPath() const { return out_path_->getFilePath(); }
};


HarvestTask::HarvestTask(const std::string &title, const std::string &bsz_upload_group,
                         const std::string &url, const std::string &config_overrides)
    : auto_temp_dir_("/tmp/ZtsMaps_", /*cleanup_if_exception_is_active*/ false, /*remove_when_out_of_scope*/ false),
      executable_(ExecUtil::LocateOrDie("zotero_harvester")),
      log_path_(auto_temp_dir_.getDirectoryPath() + "/log", "", /*automatically_remove*/ false)
{
    const auto output_directory(auto_temp_dir_.getDirectoryPath() + "/" + StringUtil::ASCIIToLower(bsz_upload_group) + "/");
    FileUtil::MakeDirectory(output_directory, true);
    out_path_.reset(new FileUtil::AutoTempFile(output_directory, ".xml",
                    /*automatically_remove*/ false));

    std::string dir_name, basename;
    FileUtil::DirnameAndBasename(out_path_->getFilePath(), &dir_name, &basename);

    std::vector<std::string> args;
    args.emplace_back("--min-log-level=DEBUG");
    args.emplace_back("--force-downloads");
    args.emplace_back("--output-directory=" + auto_temp_dir_.getDirectoryPath());
    args.emplace_back("--output-filename=" + basename);
    if (not config_overrides.empty())
        args.emplace_back(StringUtil::RemoveChars("\r", "--config-overrides=" + config_overrides));
    args.emplace_back(ZTS_HARVESTER_CONF_FILE);

    if (not url.empty()) {
        args.emplace_back("URL");
        args.emplace_back(url);
        if (not title.empty())
            args.emplace_back(title);
    } else {
        args.emplace_back("JOURNAL");
        args.emplace_back(title);
    }

    std::unordered_map<std::string, std::string> envs {
        { "LOGGER_FORMAT",  "no_decorations,strip_call_site" },
        { "UTIL_LOG_DEBUG", "true"                           },
        { "BACKTRACE",      "1"                              },
    };

    command_ = BuildCommandString(executable_, args);
    const std::string log_path(auto_temp_dir_.getDirectoryPath() + "/log");
    pid_ = ExecUtil::Spawn(executable_, args, "", log_path_.getFilePath(), log_path_.getFilePath(), envs);
}


void ExecuteHarvestAction(const std::string &title, const std::string &group_name,
                          const std::string url = "", const std::string &config_overrides = "")
{
    std::cout << "<h2>Result</h2>\r\n";
    std::cout << "<table>\r\n";

    const HarvestTask task(title, group_name, url, config_overrides);

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
    std::cout << "<tr><td>CLI output:</td><td><pre>" + output + "</pre></td></tr>\r\n";
    std::cout << "<tr><td>Server logs:</td><td><a target=\"_blank\" href=\"?action=show_logs\">click here</a></td></tr>\r\n";
    std::cout << "</table>\r\n";
}


const std::string TEMPLATE_DIRECTORY(UBTools::GetTuelibPath() + "zotero_cgi/");


void RenderHtmlTemplate(const std::string &template_filename, const Template::Map &names_to_values_map) {
    const std::string template_path(TEMPLATE_DIRECTORY + template_filename);
    std::string error_message;
    if (not FileUtil::IsReadable(template_path, &error_message))
        LOG_ERROR(error_message);

    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";

    std::ifstream template_html(template_path);
    Template::ExpandTemplate(template_html, std::cout, names_to_values_map);
    std::cout << std::flush;
}


void ProcessDownloadAction(const std::multimap<std::string, std::string> &cgi_args) {
    const std::string path(GetCGIParameterOrDefault(cgi_args, "id"));

    if (StringUtil::EndsWith(path, ".xml", /*ignore_case*/ true))
        std::cout << "Content-Type: application/xml; charset=utf-8\r\n\r\n";
    else
        std::cout << "Content-Type: text/plain; charset=utf-8\r\n\r\n";

    std::cout << FileUtil::ReadStringOrDie(path);
}


void ProcessShowDownloadedAction(const std::multimap<std::string, std::string> &cgi_args) {
    const std::string zeder_id(GetCGIParameterOrDefault(cgi_args, "zeder_id"));
    const std::string group(GetCGIParameterOrDefault(cgi_args, "group"));
    const Zeder::Flavour zeder_flavour(group == "IxTheo" or group == "RelBib" ? Zeder::Flavour::IXTHEO : Zeder::Flavour::KRIMDOK);
    const std::string zeder_instance(group == "IxTheo" or group == "RelBib" ? "ixtheo" : "krimdok");

    Template::Map names_to_values_map;
    names_to_values_map.insertScalar("zeder_id", zeder_id);
    names_to_values_map.insertScalar("zeder_instance", zeder_instance);

    std::vector<std::string> delivered_datetimes;
    std::vector<std::string> titles;
    std::vector<std::string> hashes;
    std::vector<std::string> urls;

    ZoteroHarvester::Util::UploadTracker upload_tracker;
    const auto entries(upload_tracker.getEntriesByZederIdAndFlavour(StringUtil::ToUnsigned(zeder_id), zeder_flavour));
    for (const auto &entry : entries) {
        delivered_datetimes.emplace_back(HtmlUtil::HtmlEscape(entry.delivered_at_str_));
        titles.emplace_back(HtmlUtil::HtmlEscape(entry.main_title_));
        hashes.emplace_back(HtmlUtil::HtmlEscape(entry.hash_));
        urls.emplace_back(entry.url_);
    }

    names_to_values_map.insertArray("delivered_datetimes", delivered_datetimes);
    names_to_values_map.insertArray("titles", titles);
    names_to_values_map.insertArray("hashes", hashes);
    names_to_values_map.insertArray("urls", urls);

    RenderHtmlTemplate("delivered.html", names_to_values_map);
}


bool ProcessShowQASubActionAdd(const std::multimap<std::string, std::string> &cgi_args, DbConnection * const db_connection,
                               const std::string &zeder_journal_id)
{
    // sub-action "add"
    const std::string add_type(GetCGIParameterOrDefault(cgi_args, "add_type"));
    const std::string add_tag(GetCGIParameterOrDefault(cgi_args, "add_tag"));
    const std::string add_presence(GetCGIParameterOrDefault(cgi_args, "add_presence"));
    if (add_type.empty() or add_tag.empty() or add_presence.empty())
        return false;

    std::string journal_id_to_insert = db_connection->escapeAndQuoteString(zeder_journal_id);
    if (add_type == "global")
        journal_id_to_insert = "NULL";

    db_connection->queryOrDie("INSERT INTO metadata_presence_tracer (zeder_journal_id, metadata_field_name, field_presence) "
                              " VALUES (" + journal_id_to_insert + ", " + db_connection->escapeAndQuoteString(add_tag) + ", "
                              " " + db_connection->escapeAndQuoteString(add_presence) + ")");
    return true;
}


bool ProcessShowQASubActionDelete(const std::multimap<std::string, std::string> &cgi_args, DbConnection * const db_connection,
                                  const std::string &zeder_journal_id)
{
    const std::string delete_type(GetCGIParameterOrDefault(cgi_args, "delete_type"));
    const std::string delete_tag(GetCGIParameterOrDefault(cgi_args, "delete_tag"));
    if (delete_type.empty() or delete_tag.empty())
        return false;

    std::string journal_id_to_delete(" = " + db_connection->escapeAndQuoteString(zeder_journal_id));
    if (delete_type == "global")
        journal_id_to_delete = "IS NULL";

    db_connection->queryOrDie("DELETE FROM metadata_presence_tracer "
                             "WHERE zeder_journal_id " + journal_id_to_delete + " "
                             "AND metadata_field_name = " + db_connection->escapeAndQuoteString(delete_tag));

    return true;
}


void ProcessShowQAAction(const std::multimap<std::string, std::string> &cgi_args) {
    const std::string zeder_id(GetCGIParameterOrDefault(cgi_args, "zeder_id"));
    const std::string zeder_instance(GetCGIParameterOrDefault(cgi_args, "zeder_instance"));
    std::string zeder_journal_id;
    std::string journal_name;
    std::string submitted("false");

    // try to get more details for given journal
    DbConnection db_connection;
    {
        db_connection.queryOrDie("SELECT id,journal_name FROM zeder_journals WHERE zeder_id=" + db_connection.escapeAndQuoteString(zeder_id) +
                                 " AND zeder_instance=" + db_connection.escapeAndQuoteString(zeder_instance));
        auto result_set(db_connection.getLastResultSet());
        while (const auto row = result_set.getNextRow()) {
            zeder_journal_id = row["id"];
            journal_name = row["journal_name"];
        }
    }

    if (ProcessShowQASubActionDelete(cgi_args, &db_connection, zeder_journal_id)
        or ProcessShowQASubActionAdd(cgi_args, &db_connection, zeder_journal_id))
            submitted = "true";

    // display current settings
    db_connection.queryOrDie("SELECT * FROM metadata_presence_tracer WHERE zeder_journal_id IS NULL "
                             "OR zeder_journal_id IN (SELECT id FROM zeder_journals WHERE zeder_id=" + db_connection.escapeAndQuoteString(zeder_id) +
                             " AND zeder_instance=" + db_connection.escapeAndQuoteString(zeder_instance) + ")"
                             " ORDER BY metadata_field_name ASC, zeder_journal_id ASC");

    auto result_set(db_connection.getLastResultSet());
    std::map<std::string, std::pair<std::string, std::string>> tag_to_settings_map;

    while (const auto row = result_set.getNextRow()) {
        const auto iter(tag_to_settings_map.find(row["metadata_field_name"]));
        if (iter == tag_to_settings_map.end()) {
            if (row["zeder_journal_id"].empty())
                tag_to_settings_map[row["metadata_field_name"]] = { row["field_presence"], "" };
            else
                tag_to_settings_map[row["metadata_field_name"]] = { "", row["field_presence"] };
        } else {
            if (row["zeder_journal_id"].empty())
                iter->second.first = row["field_presence"];
            else
                iter->second.second = row["field_presence"];
        }
    }

    std::vector<std::string> tags;
    std::vector<std::string> global_settings;
    std::vector<std::string> journal_settings;
    for (const auto &tag_and_settings : tag_to_settings_map) {
        tags.emplace_back(tag_and_settings.first);
        global_settings.emplace_back(tag_and_settings.second.first);
        journal_settings.emplace_back(tag_and_settings.second.second);
    }

    Template::Map names_to_values_map;
    names_to_values_map.insertScalar("submitted", submitted);
    names_to_values_map.insertScalar("zeder_id", zeder_id);
    names_to_values_map.insertScalar("zeder_instance", zeder_instance);
    names_to_values_map.insertScalar("journal_name", journal_name);
    names_to_values_map.insertArray("tags", tags);
    names_to_values_map.insertArray("global_settings", global_settings);
    names_to_values_map.insertArray("journal_settings", journal_settings);
    RenderHtmlTemplate("qa.html", names_to_values_map);
}


void ProcessShowLogsAction() {
    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";
    std::cout << "<html>";
    std::cout << "<body>";
    std::cout << "<h1>Zotero Translation Server Logs</h1>";
    std::cout << "<pre>";
    std::cout << std::flush;
    // The httpd user must be in docker group for this to work
    ExecUtil::ExecOrDie("/usr/local/ub_tools/docker/zts/logs.sh");
    std::cout << "</pre>";
    std::cout << "</body>";
    std::cout << "</html>";
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    std::multimap<std::string, std::string> cgi_args;
    WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);

    const std::string default_action("list");
    const std::string action(GetCGIParameterOrDefault(cgi_args, "action", default_action));
    const std::string config_overrides(GetCGIParameterOrDefault(cgi_args, "config_overrides"));
    const std::string url(GetCGIParameterOrDefault(cgi_args, "url"));

    if (action == "download")
        ProcessDownloadAction(cgi_args);
    else if (action == "show_downloaded")
        ProcessShowDownloadedAction(cgi_args);
    else if (action == "show_qa")
        ProcessShowQAAction(cgi_args);
    else if (action == "show_logs")
        ProcessShowLogsAction();
    else {
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

        names_to_values_map.insertScalar("running_processes_count", std::to_string(ExecUtil::FindActivePrograms("zotero_harvester").size()));
        names_to_values_map.insertScalar("config_overrides", config_overrides);
        names_to_values_map.insertScalar("url", url);

        std::unordered_map<std::string, ZoteroHarvester::Config::GroupParams> group_name_to_params_map;
        std::unordered_map<std::string, std::string>journal_name_to_group_name_map;
        ParseConfigFile(cgi_args, &names_to_values_map, &group_name_to_params_map, &journal_name_to_group_name_map);
        RenderHtmlTemplate("index.html", names_to_values_map);

        std::string title, group_name;

        if (action != default_action) {
            if (action == "rss") {
                title = GetCGIParameterOrDefault(cgi_args, "rss_journal_title");
                group_name = journal_name_to_group_name_map.at(title);
            } else if (action == "crawling") {
                title = GetCGIParameterOrDefault(cgi_args, "crawling_journal_title");
                group_name = journal_name_to_group_name_map.at(title);
            } else if (action == "url") {
                title = GetCGIParameterOrDefault(cgi_args, "url_journal_title");
                if (title.empty())
                    group_name = "ixtheo";
                else
                    group_name = journal_name_to_group_name_map.at(title);
            } else if (action != default_action)
                LOG_ERROR("invalid action: \"" + action + '"');

            ExecuteHarvestAction(title, group_name, url, config_overrides);
        }
        std::cout << "</body></html>";
    }

    return EXIT_SUCCESS;
}
