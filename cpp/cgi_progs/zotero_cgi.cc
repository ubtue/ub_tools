/** \file    zotero_cgi.cc
 *  \brief   A CGI-tool to execute Zotero RSS & Crawling mechanisms
 *  \author  Mario Trojan
 */
/*
    Copyright (C) 2016-2021, Library of the University of Tübingen

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
#include <map>
#include <string>
#include <unordered_set>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include "DbConnection.h"
#include "DnsUtil.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "HtmlUtil.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "Template.h"
#include "UBTools.h"
#include "UrlUtil.h"
#include "WallClockTimer.h"
#include "WebUtil.h"
#include "Zeder.h"
#include "ZoteroHarvesterConfig.h"
#include "ZoteroHarvesterUtil.h"
#include "util.h"


namespace {


std::string zts_client_maps_directory;
const std::string ZTS_HARVESTER_CONF_FILE(UBTools::GetTuelibPath() + "zotero-enhancement-maps/zotero_harvester.conf");


static Template::Map names_to_values_map;


std::string GetMinElementOrDefault(const std::vector<std::string> &elements, const std::string &default_value = "") {
    const auto min_element(std::min_element(elements.begin(), elements.end()));
    if (unlikely(min_element == elements.end()))
        return default_value;

    return *min_element;
}


struct Journal {
    unsigned id_;
    unsigned zeder_id_;
    Zeder::Flavour zeder_flavour_;
    std::string name_;
};


Journal GetJournalById(const unsigned id, DbConnection * const db_connection) {
    db_connection->queryOrDie("SELECT id, zeder_id, zeder_instance, journal_name FROM zeder_journals WHERE id="
                              + db_connection->escapeAndQuoteString(std::to_string(id)));
    auto result_set(db_connection->getLastResultSet());
    while (const auto row = result_set.getNextRow()) {
        Journal journal;
        journal.id_ = id;
        journal.zeder_id_ = StringUtil::ToUnsigned(row["zeder_id"]);
        journal.zeder_flavour_ = Zeder::GetFlavourByString(row["zeder_instance"]);
        journal.name_ = row["journal_name"];
        return journal;
    }

    LOG_ERROR("Journal with ID \"" + std::to_string(id) + "\" does not exist in database!");
}


std::unordered_map<std::string, unsigned> GetZederIdAndInstanceToZederJournalIdMap(DbConnection * const db_connection) {
    std::unordered_map<std::string, unsigned> zeder_id_and_instance_to_zeder_journal_id_map;

    db_connection->queryOrDie("SELECT id, zeder_id, zeder_instance FROM zeder_journals");
    auto result_set(db_connection->getLastResultSet());
    while (const auto row = result_set.getNextRow())
        zeder_id_and_instance_to_zeder_journal_id_map[row["zeder_id"] + "#" + row["zeder_instance"]] = StringUtil::ToUnsigned(row["id"]);

    return zeder_id_and_instance_to_zeder_journal_id_map;
}


unsigned GetZederJournalId(const unsigned zeder_id, const std::string &zeder_instance, DbConnection * const db_connection) {
    static const auto zeder_id_and_instance_to_zeder_journal_id_map(GetZederIdAndInstanceToZederJournalIdMap(db_connection));
    return zeder_id_and_instance_to_zeder_journal_id_map.at(std::to_string(zeder_id) + "#" + zeder_instance);
}


std::unordered_map<unsigned, time_t> GetMaxDeliveredDatetimePerJournal(DbConnection * const db_connection) {
    std::unordered_map<unsigned, time_t> journal_id_to_delivered_datetime_map;

    db_connection->queryOrDie(
        "SELECT zeder_journal_id, MAX(delivered_at) AS max_delivered_at FROM delivered_marc_records GROUP BY zeder_journal_id");
    auto result_set(db_connection->getLastResultSet());
    while (const auto row = result_set.getNextRow())
        journal_id_to_delivered_datetime_map[StringUtil::ToUnsigned(row["zeder_journal_id"])] =
            SqlUtil::DatetimeToTimeT(row["max_delivered_at"]);

    return journal_id_to_delivered_datetime_map;
}


time_t GetJournalMaxDeliveredDatetime(const unsigned zeder_journal_id, DbConnection * const db_connection) {
    static const auto journal_id_to_delivered_datetime_map(GetMaxDeliveredDatetimePerJournal(db_connection));
    const auto match(journal_id_to_delivered_datetime_map.find(zeder_journal_id));
    if (match == journal_id_to_delivered_datetime_map.end())
        return TimeUtil::BAD_TIME_T;
    return match->second;
}


std::unordered_set<unsigned> GetJournalIdsWithErrors(DbConnection * const db_connection) {
    std::unordered_set<unsigned> zeder_journal_ids_with_errors;

    db_connection->queryOrDie(
        "SELECT DISTINCT zeder_journals.id FROM zeder_journals "
        "RIGHT JOIN delivered_marc_records ON zeder_journals.id=delivered_marc_records.zeder_journal_id "
        "WHERE delivered_marc_records.delivery_state="
        + db_connection->escapeAndQuoteString(
            ZoteroHarvester::Util::UploadTracker::DELIVERY_STATE_TO_STRING_MAP.at(ZoteroHarvester::Util::UploadTracker::ERROR)));
    auto result_set(db_connection->getLastResultSet());
    while (const auto row = result_set.getNextRow())
        zeder_journal_ids_with_errors.emplace(StringUtil::ToUnsigned(row["id"]));

    return zeder_journal_ids_with_errors;
}


bool GetJournalErrorsDetected(const unsigned zeder_journal_id, DbConnection * const db_connection) {
    static const auto zeder_journal_ids_with_errors(GetJournalIdsWithErrors(db_connection));
    return zeder_journal_ids_with_errors.find(zeder_journal_id) != zeder_journal_ids_with_errors.end();
}


void RegisterMissingJournals(const std::vector<std::unique_ptr<ZoteroHarvester::Config::JournalParams>> &journal_params,
                             DbConnection * const db_connection, ZoteroHarvester::Util::UploadTracker * const upload_tracker) {
    const auto zeder_id_and_instance_to_zeder_journal_id_map(GetZederIdAndInstanceToZederJournalIdMap(db_connection));
    for (const auto &journal : journal_params) {
        const std::string key(std::to_string(journal->zeder_id_) + "#" + upload_tracker->GetZederInstanceString(journal->group_));
        if (zeder_id_and_instance_to_zeder_journal_id_map.find(key) == zeder_id_and_instance_to_zeder_journal_id_map.end())
            upload_tracker->registerZederJournal(journal->zeder_id_, StringUtil::ASCIIToLower(journal->group_), journal->name_);
    }
}


bool isTestEnvironment() {
    return (DnsUtil::GetHostname() != "ub28.uni-tuebingen.de");
}


std::string GetJournalHarvestStatus(const unsigned zeder_journal_id, const ZoteroHarvester::Config::JournalParams &journal_params,
                                    DbConnection * const db_connection) {
    std::string harvest_status("NONE");
    if ((journal_params.upload_operation_ == ZoteroHarvester::Config::TEST and isTestEnvironment())
        or (journal_params.upload_operation_ == ZoteroHarvester::Config::LIVE and not isTestEnvironment()))
    {
        const auto max_delivered_datetime(GetJournalMaxDeliveredDatetime(zeder_journal_id, db_connection));
        if (max_delivered_datetime != TimeUtil::BAD_TIME_T) {
            if (journal_params.update_window_ != 0 and max_delivered_datetime < ::time(nullptr) - journal_params.update_window_ * 86400)
                harvest_status = "ERROR";
            else if (GetJournalErrorsDetected(zeder_journal_id, db_connection))
                harvest_status = "WARNING";
            else
                harvest_status = "SUCCESS";
        }
    }
    return harvest_status;
}


void ParseConfigFile(const std::multimap<std::string, std::string> &cgi_args,
                     std::unordered_map<std::string, ZoteroHarvester::Config::GroupParams> * const group_name_to_params_map,
                     std::unordered_map<std::string, ZoteroHarvester::Config::SubgroupParams> * const subgroup_name_to_params_map,
                     std::unordered_map<std::string, std::string> * const journal_name_to_group_name_map,
                     DbConnection * const db_connection, ZoteroHarvester::Util::UploadTracker * const upload_tracker) {
    std::unique_ptr<ZoteroHarvester::Config::GlobalParams> global_params;
    std::vector<std::unique_ptr<ZoteroHarvester::Config::GroupParams>> group_params;
    std::vector<std::unique_ptr<ZoteroHarvester::Config::SubgroupParams>> subgroup_params;
    std::vector<std::unique_ptr<ZoteroHarvester::Config::JournalParams>> journal_params;

    ZoteroHarvester::Config::LoadHarvesterConfigFile(ZTS_HARVESTER_CONF_FILE, &global_params, &group_params, &subgroup_params,
                                                     &journal_params);
    RegisterMissingJournals(journal_params, db_connection, upload_tracker);

    std::vector<std::string> all_journal_titles;
    std::vector<std::string> all_journal_print_issns;
    std::vector<std::string> all_journal_online_issns;
    std::vector<std::string> all_journal_print_ppns;
    std::vector<std::string> all_journal_online_ppns;
    std::vector<std::string> all_journal_methods;
    std::vector<std::string> all_journal_groups;
    std::vector<std::string> all_journal_delivery_modes;
    std::vector<std::string> all_journal_ids;
    std::vector<std::string> all_journal_zeder_ids;
    std::vector<std::string> all_journal_zeder_urls;
    std::vector<std::string> all_personalized_authors;
    std::vector<std::string> all_journal_harvest_statuses;
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

    for (const auto &subgroup : subgroup_params)
        subgroup_name_to_params_map->emplace(subgroup->name_, *subgroup);

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
        const auto &personalized_authors(journal_param->personalized_authors_);

        std::string zeder_instance;
        std::string zeder_instance_for_url;
        if (group == "IxTheo" or group == "RelBib") {
            zeder_instance = "ixtheo";
            zeder_instance_for_url = "ixtheo";
        } else if (group == "KrimDok") {
            zeder_instance = "krimdok";
            zeder_instance_for_url = "krim";
        }
        const std::string zeder_url("http://www-ub.ub.uni-tuebingen.de/zeder/?instanz=" + zeder_instance_for_url + "#suche=Z%3D"
                                    + std::to_string(zeder_id));
        const auto zeder_journal_id(GetZederJournalId(zeder_id, zeder_instance, db_connection));
        all_journal_harvest_statuses.emplace_back(GetJournalHarvestStatus(zeder_journal_id, *journal_param, db_connection));

        journal_name_to_group_name_map->insert(std::make_pair(title, group));

        all_journal_titles.emplace_back(title);
        all_journal_print_issns.emplace_back(issn_print);
        all_journal_online_issns.emplace_back(issn_online);
        all_journal_print_ppns.emplace_back(ppn_print);
        all_journal_online_ppns.emplace_back(ppn_online);
        all_journal_groups.emplace_back(group);
        all_journal_methods.emplace_back(harvest_type_raw);
        all_journal_ids.emplace_back(std::to_string(zeder_journal_id));
        all_journal_zeder_ids.emplace_back(std::to_string(zeder_id));
        all_journal_zeder_urls.emplace_back(zeder_url);
        all_journal_delivery_modes.emplace_back(ZoteroHarvester::Config::UPLOAD_OPERATION_TO_STRING_MAP.at(delivery_mode));
        all_urls.emplace_back(url);
        std::string personalized_authors_transf("-");
        if (personalized_authors == "1" or StringUtil::ASCIIToUpper(personalized_authors) == "N")
            personalized_authors_transf = "N";
        if (personalized_authors == "2" or StringUtil::ASCIIToUpper(personalized_authors) == "J")
            personalized_authors_transf = "J";
        all_personalized_authors.emplace_back(personalized_authors_transf);

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

    names_to_values_map.insertScalar("zotero_translation_server_url", global_params->translation_server_url_);

    names_to_values_map.insertArray("all_journal_titles", all_journal_titles);
    names_to_values_map.insertArray("all_journal_print_issns", all_journal_print_issns);
    names_to_values_map.insertArray("all_journal_online_issns", all_journal_online_issns);
    names_to_values_map.insertArray("all_journal_print_ppns", all_journal_print_ppns);
    names_to_values_map.insertArray("all_journal_online_ppns", all_journal_online_ppns);
    names_to_values_map.insertArray("all_journal_methods", all_journal_methods);
    names_to_values_map.insertArray("all_journal_groups", all_journal_groups);
    names_to_values_map.insertArray("all_journal_delivery_modes", all_journal_delivery_modes);
    names_to_values_map.insertArray("all_journal_ids", all_journal_ids);
    names_to_values_map.insertArray("all_journal_zeder_ids", all_journal_zeder_ids);
    names_to_values_map.insertArray("all_journal_zeder_urls", all_journal_zeder_urls);
    names_to_values_map.insertArray("all_personalized_authors", all_personalized_authors);
    names_to_values_map.insertArray("all_journal_harvest_statuses", all_journal_harvest_statuses);
    names_to_values_map.insertArray("all_urls", all_urls);

    names_to_values_map.insertArray("rss_journal_titles", rss_journal_titles);
    names_to_values_map.insertArray("rss_journal_print_issns", rss_journal_print_issns);
    names_to_values_map.insertArray("rss_journal_online_issns", rss_journal_online_issns);
    names_to_values_map.insertArray("rss_journal_print_ppns", rss_journal_print_ppns);
    names_to_values_map.insertArray("rss_journal_online_ppns", rss_journal_online_ppns);
    names_to_values_map.insertArray("rss_feed_urls", rss_feed_urls);
    names_to_values_map.insertArray("rss_strptime_formats", rss_strptime_formats);

    names_to_values_map.insertArray("crawling_journal_titles", crawling_journal_titles);
    names_to_values_map.insertArray("crawling_journal_print_issns", crawling_journal_print_issns);
    names_to_values_map.insertArray("crawling_journal_online_issns", crawling_journal_online_issns);
    names_to_values_map.insertArray("crawling_journal_print_ppns", crawling_journal_print_ppns);
    names_to_values_map.insertArray("crawling_journal_online_ppns", crawling_journal_online_ppns);
    names_to_values_map.insertArray("crawling_base_urls", crawling_base_urls);
    names_to_values_map.insertArray("crawling_extraction_regexes", crawling_extraction_regexes);
    names_to_values_map.insertArray("crawling_depths", crawling_depths);
    names_to_values_map.insertArray("crawling_strptime_formats", crawling_strptime_formats);

    const std::string first_crawling_journal_title(GetMinElementOrDefault(crawling_journal_titles));
    names_to_values_map.insertScalar("selected_crawling_journal_title",
                                     WebUtil::GetCGIParameterOrDefault(cgi_args, "crawling_journal_title", first_crawling_journal_title));

    const std::string first_rss_journal_title(GetMinElementOrDefault(rss_journal_titles));
    names_to_values_map.insertScalar("selected_rss_journal_title",
                                     WebUtil::GetCGIParameterOrDefault(cgi_args, "rss_journal_title", first_rss_journal_title));

    names_to_values_map.insertScalar("selected_url_journal_title", WebUtil::GetCGIParameterOrDefault(cgi_args, "url_journal_title"));
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
    HarvestTask(const std::string &title, const std::string &bsz_upload_group, const std::string &url = "",
                const std::string &config_overrides = "");

    /** \brief get shell command including args (for debug output) */
    inline const std::string &getCommand() const { return command_; }
    inline int getExitCode() const { return exit_code_; }
    inline int getPid() const { return pid_; }
    inline const std::string &getLogPath() const { return log_path_.getFilePath(); }

    /** \brief get path to out file with harvested records */
    inline const std::string &getOutPath() const { return out_path_->getFilePath(); }
};


HarvestTask::HarvestTask(const std::string &title, const std::string &bsz_upload_group, const std::string &url,
                         const std::string &config_overrides)
    : auto_temp_dir_("/tmp/ZtsMaps_", /*cleanup_if_exception_is_active*/ false, /*remove_when_out_of_scope*/ false),
      executable_(ExecUtil::LocateOrDie("zotero_harvester")),
      log_path_(auto_temp_dir_.getDirectoryPath() + "/log", "", /*automatically_remove*/ false) {
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

    std::unordered_map<std::string, std::string> envs{
        { "LOGGER_FORMAT", "no_decorations,strip_call_site" },
        { "UTIL_LOG_DEBUG", "true" },
        { "BACKTRACE", "1" },
    };

    command_ = BuildCommandString(executable_, args);
    const std::string log_path(auto_temp_dir_.getDirectoryPath() + "/log");
    pid_ = ExecUtil::Spawn(executable_, args, "", log_path_.getFilePath(), log_path_.getFilePath(), envs);
}


void ExecuteHarvestAction(const std::string &title, const std::string &group_name, const std::string url = "",
                          const std::string &config_overrides = "") {
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
    if WIFEXITED (status)
        exit_code = WEXITSTATUS(status);

    std::string output;
    if (not FileUtil::ReadString(task.getLogPath(), &output))
        output = "could not read log file!";

    if (exit_code == 0)
        std::cout << "<tr><td>Download</td><td><a target=\"_blank\" href=\"?action=download&id=" + task.getOutPath()
                         + "\">Result file</a></td></tr>\r\n";
    else
        std::cout << "<tr><td>ERROR</td><td>Exitcode: " + std::to_string(exit_code) + "</td></tr>\r\n";

    // use <pre> instead of nl2br + htmlspecialchars
    std::cout << "<tr><td>CLI output:</td><td><pre>" + output + "</pre></td></tr>\r\n";
    std::cout << "<tr><td>Server logs:</td><td><a target=\"_blank\" href=\"?action=show_logs\">click here</a></td></tr>\r\n";
    std::cout << "</table>\r\n";
}


const std::string TEMPLATE_DIRECTORY(UBTools::GetTuelibPath() + "zotero_cgi");


void RenderHtmlTemplate(const std::string &template_filename) {
    const std::string template_path(TEMPLATE_DIRECTORY + "/" + template_filename);
    std::string error_message;
    if (not FileUtil::IsReadable(template_path, &error_message))
        LOG_ERROR(error_message);

    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";

    std::ifstream template_html(template_path);
    Template::ExpandTemplate(template_html, std::cout, names_to_values_map);
    std::cout << std::flush;
}


void AddStyleCSS(Template::Map * const template_map) {
    std::string style_css;
    FileUtil::ReadString(TEMPLATE_DIRECTORY + "/" + "style.css", &style_css);
    template_map->insertScalar("style_css", style_css);
}


void ProcessDownloadAction(const std::multimap<std::string, std::string> &cgi_args) {
    const std::string path(WebUtil::GetCGIParameterOrDefault(cgi_args, "id"));

    if (StringUtil::EndsWith(path, ".xml", /*ignore_case*/ true))
        std::cout << "Content-Type: application/xml; charset=utf-8\r\n\r\n";
    else
        std::cout << "Content-Type: text/plain; charset=utf-8\r\n\r\n";

    std::cout << FileUtil::ReadStringOrDie(path);
}


void UpdateRecordDeliveryStateAndTimestamp(const std::string &record_id,
                                           const ZoteroHarvester::Util::UploadTracker::DeliveryState &delivery_state,
                                           DbConnection * const db_connection) {
    db_connection->queryOrDie(
        "UPDATE delivered_marc_records SET delivery_state="
        + db_connection->escapeAndQuoteString(ZoteroHarvester::Util::UploadTracker::DELIVERY_STATE_TO_STRING_MAP.at(delivery_state))
        + ",delivered_at=NOW() WHERE id=" + db_connection->escapeAndQuoteString(record_id));
}


void ResetDeliveredRecordsForJournal(const unsigned journal_id, DbConnection * const db_connection) {
    db_connection->queryOrDie(
        "UPDATE delivered_marc_records SET delivery_state="
        + db_connection->escapeAndQuoteString(
            ZoteroHarvester::Util::UploadTracker::DELIVERY_STATE_TO_STRING_MAP.at(ZoteroHarvester::Util::UploadTracker::RESET))
        + " WHERE zeder_journal_id=" + db_connection->escapeAndQuoteString(std::to_string(journal_id)) + " AND delivery_state="
        + db_connection->escapeAndQuoteString(
            ZoteroHarvester::Util::UploadTracker::DELIVERY_STATE_TO_STRING_MAP.at(ZoteroHarvester::Util::UploadTracker::AUTOMATIC)));
}


void ProcessShowDownloadedAction(const std::multimap<std::string, std::string> &cgi_args,
                                 ZoteroHarvester::Util::UploadTracker * const upload_tracker, DbConnection * const db_connection) {
    const std::string journal_id(WebUtil::GetCGIParameterOrDefault(cgi_args, "id"));
    const Journal journal(GetJournalById(StringUtil::ToUnsigned(journal_id), db_connection));
    std::string at_least_one_action_done("false");

    const std::string id_to_deliver_manually(WebUtil::GetCGIParameterOrDefault(cgi_args, "set_manually_delivered"));
    if (not id_to_deliver_manually.empty()) {
        UpdateRecordDeliveryStateAndTimestamp(id_to_deliver_manually, ZoteroHarvester::Util::UploadTracker::DeliveryState::MANUAL,
                                              db_connection);
        at_least_one_action_done = "true";
    }

    const std::string id_to_reset(WebUtil::GetCGIParameterOrDefault(cgi_args, "reset"));
    if (not id_to_reset.empty()) {
        if (id_to_reset == "all")
            ResetDeliveredRecordsForJournal(journal.id_, db_connection);
        else
            UpdateRecordDeliveryStateAndTimestamp(id_to_reset, ZoteroHarvester::Util::UploadTracker::DeliveryState::RESET, db_connection);
        at_least_one_action_done = "true";
    }

    names_to_values_map.insertScalar("id", journal.id_);
    names_to_values_map.insertScalar("zeder_id", journal.zeder_id_);
    names_to_values_map.insertScalar("zeder_instance", Zeder::FLAVOUR_TO_STRING_MAP.at(journal.zeder_flavour_));
    names_to_values_map.insertScalar("journal_name", journal.name_);
    names_to_values_map.insertScalar("at_least_one_action_done", at_least_one_action_done);

    std::vector<std::string> ids;
    std::vector<std::string> delivered_datetimes;
    std::vector<std::string> titles;
    std::vector<std::string> hashes;
    std::vector<std::string> links;
    std::vector<std::string> delivery_states;
    std::vector<std::string> error_messages;

    const auto entries(upload_tracker->getEntriesByZederIdAndFlavour(journal.zeder_id_, journal.zeder_flavour_));
    for (const auto &entry : entries) {
        const std::string escaped_id(HtmlUtil::HtmlEscape(std::to_string(entry.id_)));
        const std::string link("<a href=\"" + entry.url_ + "\" target=\"_blank\">" + entry.url_ + "</a>");
        if (not ids.empty() and ids.back() == escaped_id)
            links.back() += "<br>" + link;
        else {
            ids.emplace_back(escaped_id);
            delivered_datetimes.emplace_back(HtmlUtil::HtmlEscape(entry.delivered_at_str_));
            titles.emplace_back(HtmlUtil::HtmlEscape(entry.main_title_));
            hashes.emplace_back(HtmlUtil::HtmlEscape(entry.hash_));
            links.emplace_back(link);
            delivery_states.emplace_back(
                HtmlUtil::HtmlEscape(ZoteroHarvester::Util::UploadTracker::DELIVERY_STATE_TO_STRING_MAP.at(entry.delivery_state_)));
            error_messages.emplace_back(HtmlUtil::HtmlEscape(entry.error_message_));
        }
    }

    names_to_values_map.insertArray("ids", ids);
    names_to_values_map.insertArray("delivered_datetimes", delivered_datetimes);
    names_to_values_map.insertArray("titles", titles);
    names_to_values_map.insertArray("hashes", hashes);
    names_to_values_map.insertArray("links", links);
    names_to_values_map.insertArray("delivery_states", delivery_states);
    names_to_values_map.insertArray("error_messages", error_messages);

    RenderHtmlTemplate("delivered.html");
}


bool ProcessShowQASubActionAdd(const std::multimap<std::string, std::string> &cgi_args, DbConnection * const db_connection,
                               const std::string &journal_id) {
    // sub-action "add"
    const std::string add_type(WebUtil::GetCGIParameterOrDefault(cgi_args, "add_type"));
    const std::string add_tag(WebUtil::GetCGIParameterOrDefault(cgi_args, "add_tag"));
    const std::string add_subfield_code(WebUtil::GetCGIParameterOrDefault(cgi_args, "add_subfield_code"));
    const std::string add_record_type(WebUtil::GetCGIParameterOrDefault(cgi_args, "add_record_type"));
    const std::string add_regex(WebUtil::GetCGIParameterOrDefault(cgi_args, "add_regex"));
    const std::string add_presence(WebUtil::GetCGIParameterOrDefault(cgi_args, "add_presence"));
    if (add_type.empty() or add_tag.empty() or add_presence.empty())
        return false;

    const std::string regex_to_insert(add_regex.empty() ? "NULL" : db_connection->escapeAndQuoteString(add_regex));
    const std::string journal_id_to_insert(add_type == "global" ? "NULL" : db_connection->escapeAndQuoteString(journal_id));

    db_connection->queryOrDie(
        "INSERT INTO metadata_presence_tracer (journal_id, marc_field_tag, marc_subfield_code,"
        " record_type, regex, field_presence) VALUES ("
        + journal_id_to_insert + ", " + db_connection->escapeAndQuoteString(add_tag) + ", '" + add_subfield_code + "', '" + add_record_type
        + "', " + regex_to_insert + ", " + db_connection->escapeAndQuoteString(add_presence) + ")");
    return true;
}


bool ProcessShowQASubActionDelete(const std::multimap<std::string, std::string> &cgi_args, DbConnection * const db_connection,
                                  const std::string &journal_id) {
    const std::string delete_tag(WebUtil::GetCGIParameterOrDefault(cgi_args, "delete_tag"));
    const std::string delete_type(WebUtil::GetCGIParameterOrDefault(cgi_args, "delete_type"));
    if (delete_type.empty() or delete_tag.empty())
        return false;

    const std::string delete_subfield_code(WebUtil::GetCGIParameterOrDefault(cgi_args, "delete_subfield_code"));
    const std::string delete_record_type(WebUtil::GetCGIParameterOrDefault(cgi_args, "delete_record_type"));

    std::string journal_id_to_delete(" = " + db_connection->escapeAndQuoteString(journal_id));
    if (delete_type == "global")
        journal_id_to_delete = "IS NULL";

    db_connection->queryOrDie(
        "DELETE FROM metadata_presence_tracer"
        " WHERE journal_id "
        + journal_id_to_delete + " AND marc_field_tag = " + db_connection->escapeAndQuoteString(delete_tag)
        + " AND marc_subfield_code = " + db_connection->escapeAndQuoteString(delete_subfield_code)
        + " AND record_type = " + db_connection->escapeAndQuoteString(delete_record_type));

    return true;
}


struct QASubfieldProperties {
    std::string field_presence_;
    std::string regex_;

    QASubfieldProperties() = default;
    QASubfieldProperties(const std::string &field_presence, const std::string &regex): field_presence_(field_presence), regex_(regex) { }
};


struct QAFieldProperties {
    std::string tag_;
    std::map<char, QASubfieldProperties> global_regular_articles_;
    std::map<char, QASubfieldProperties> global_review_articles_;
    std::map<char, QASubfieldProperties> global_non_articles_;
    std::map<char, QASubfieldProperties> journal_regular_articles_;
    std::map<char, QASubfieldProperties> journal_review_articles_;
    std::map<char, QASubfieldProperties> journal_non_articles_;

    QAFieldProperties() = default;
    QAFieldProperties(const std::string &tag): tag_(tag){};

    std::string generateHtmlForMap(const std::map<char, QASubfieldProperties> &map, const std::string &record_type,
                                   const bool overridden = false, const std::string &delete_type = "",
                                   const std::string &base_url = "") const;
};


std::string QAFieldProperties::generateHtmlForMap(const std::map<char, QASubfieldProperties> &map, const std::string &record_type,
                                                  const bool overridden, const std::string &delete_type,
                                                  const std::string &base_url) const {
    std::string html;
    if (overridden)
        html += "<div class=\"qa_rule_overridden\">Overridden (journal-specific):<br>";
    for (const auto &subfield_and_properties : map) {
        const auto subfield_code(std::string(1, subfield_and_properties.first));
        html += subfield_code + ": " + subfield_and_properties.second.field_presence_;
        if (not delete_type.empty()) {
            const std::string deletion_url = base_url + "&delete_tag=" + tag_ + "&delete_subfield_code=" + subfield_code
                                             + "&delete_record_type=" + record_type + "&delete_type=" + delete_type;
            html += "<a href=" + deletion_url
                    + " title=\"Delete this rule\" onclick=\"return confirm('Do you really want to delete this rule?')\"><sup>x</sup></a>";
        }
        if (not subfield_and_properties.second.regex_.empty()) {
            html += ", pattern: <a href=\"https://regex101.com/?regex=" + UrlUtil::UrlEncode(subfield_and_properties.second.regex_)
                    + "\" target=\"_blank\">" + HtmlUtil::HtmlEscape(subfield_and_properties.second.regex_) + "</a>";
        }
        html += "<br>";
    }
    if (overridden)
        html += "</div>";
    return html;
}


std::map<std::string, QAFieldProperties> GetQASettings(const std::string &journal_id, DbConnection * const db_connection) {
    db_connection->queryOrDie(
        "SELECT * FROM metadata_presence_tracer WHERE journal_id IS NULL"
        " OR journal_id = "
        + db_connection->escapeAndQuoteString(journal_id) + " ORDER BY marc_field_tag ASC, marc_subfield_code ASC, journal_id ASC");

    auto result_set(db_connection->getLastResultSet());
    std::map<std::string, QAFieldProperties> tags_to_settings_map;
    while (const auto row = result_set.getNextRow()) {
        const auto tag(row["marc_field_tag"]);
        const char subfield(row["marc_subfield_code"].at(0));
        const QASubfieldProperties subfield_properties(row["field_presence"], row["regex"]);
        if (tags_to_settings_map.find(tag) == tags_to_settings_map.end())
            tags_to_settings_map[tag] = QAFieldProperties(tag);

        if (row["journal_id"].empty()) {
            if (row["record_type"] == "regular_article")
                tags_to_settings_map[tag].global_regular_articles_[subfield] = subfield_properties;
            else if (row["record_type"] == "review")
                tags_to_settings_map[tag].global_review_articles_[subfield] = subfield_properties;
            else if (row["record_type"] == "non_article")
                tags_to_settings_map[tag].global_non_articles_[subfield] = subfield_properties;
            else
                LOG_ERROR("Invalid record type: " + row["record_type"]);
        } else {
            if (row["record_type"] == "regular_article")
                tags_to_settings_map[tag].journal_regular_articles_[subfield] = subfield_properties;
            else if (row["record_type"] == "review")
                tags_to_settings_map[tag].journal_review_articles_[subfield] = subfield_properties;
            else if (row["record_type"] == "non_article")
                tags_to_settings_map[tag].journal_non_articles_[subfield] = subfield_properties;
            else
                LOG_ERROR("Invalid record type: " + row["record_type"]);
        }
    }
    return tags_to_settings_map;
}


void ProcessShowQAAction(const std::multimap<std::string, std::string> &cgi_args, DbConnection * const db_connection) {
    const std::string journal_id(WebUtil::GetCGIParameterOrDefault(cgi_args, "id"));
    const Journal journal(GetJournalById(StringUtil::ToUnsigned(journal_id), db_connection));
    std::string submitted("false");

    if (ProcessShowQASubActionDelete(cgi_args, db_connection, journal_id) or ProcessShowQASubActionAdd(cgi_args, db_connection, journal_id))
        submitted = "true";

    const auto tags_to_settings_map(GetQASettings(journal_id, db_connection));
    std::vector<std::string> tags, global_regular_articles, global_review_articles, global_non_articles,
                             journal_regular_articles, journal_review_articles, journal_non_articles;
    const std::string base_url("?action=show_qa&id=" + journal_id);
    for (const auto &tag_and_settings : tags_to_settings_map) {
        tags.emplace_back(tag_and_settings.first);

        const bool global_regular_articles_overridden(not tag_and_settings.second.journal_regular_articles_.empty());
        const bool global_review_articles_overridden(not tag_and_settings.second.journal_review_articles_.empty());
        const bool global_non_articles_overridden(not tag_and_settings.second.journal_non_articles_.empty());
        global_regular_articles.emplace_back(tag_and_settings.second.generateHtmlForMap(
            tag_and_settings.second.global_regular_articles_, "regular_article", global_regular_articles_overridden));
        global_review_articles.emplace_back(tag_and_settings.second.generateHtmlForMap(tag_and_settings.second.global_review_articles_,
                                                                                       "review", global_review_articles_overridden));
        global_non_articles.emplace_back(tag_and_settings.second.generateHtmlForMap(tag_and_settings.second.global_non_articles_,
                                                                                       "non_article", global_non_articles_overridden));

        journal_regular_articles.emplace_back(tag_and_settings.second.generateHtmlForMap(
            tag_and_settings.second.journal_regular_articles_, "regular_article", /* overridden = */ false, "local", base_url));
        journal_review_articles.emplace_back(tag_and_settings.second.generateHtmlForMap(
            tag_and_settings.second.journal_review_articles_, "review", /* overridden = */ false, "local", base_url));
        journal_non_articles.emplace_back(tag_and_settings.second.generateHtmlForMap(
            tag_and_settings.second.journal_non_articles_, "non_article", /* overridden = */ false, "local", base_url));
    }

    names_to_values_map.insertScalar("submitted", submitted);
    names_to_values_map.insertScalar("id", journal_id);
    names_to_values_map.insertScalar("journal_name", journal.name_);
    names_to_values_map.insertArray("tags", tags);
    names_to_values_map.insertArray("global_regular_articles", global_regular_articles);
    names_to_values_map.insertArray("global_review_articles", global_review_articles);
    names_to_values_map.insertArray("global_non_articles", global_non_articles);
    names_to_values_map.insertArray("journal_regular_articles", journal_regular_articles);
    names_to_values_map.insertArray("journal_review_articles", journal_review_articles);
    names_to_values_map.insertArray("journal_non_articles", journal_non_articles);
    RenderHtmlTemplate("qa.html");
}


void ProcessShowLogsAction() {
    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n"
              << "<html>"
              << "<body>"
              << "<h1>Zotero Translation Server Logs</h1>" << std::flush;
    const std::string ZTS_LOG(UBTools::GetTueFindLogPath() + "zts.log");
    std::string tail_output, tail_error;
    if (not FileUtil::IsReadable(ZTS_LOG))
        std::cout << "<p>The log file does not exist!</p>";
    else if (not ExecUtil::ExecSubcommandAndCaptureStdoutAndStderr(ExecUtil::LocateOrDie("tail"), { "--lines=1000", ZTS_LOG }, &tail_output,
                                                                   &tail_error))
        std::cout << "<p>The log file could not be parsed!</p>";
    else if (tail_output.empty())
        std::cout << "<p>The log file is empty!</p>" << ExecUtil::LocateOrDie("tail");
    else {
        std::cout
            << "<p>This view contains the last 1000 lines of the log,<br>"
            << "even if they don't belong to your run!</p>"
            << "<p>Need help? <a href=\"//github.com/ubtue/zotero-translation-server/wiki/Logging\" target=\"_blank\">See Wiki</a></p>"
            << "<pre>" << tail_output << "</pre>";
    }
    std::cout << "</body>"
              << "</html>";
}


} // unnamed namespace


const std::string SKIP_ONLINE_FIRST_TRUE_DIRECTIVE("skip_online_first_articles_unconditionally=true");


int Main(int argc, char *argv[]) {
    std::multimap<std::string, std::string> cgi_args;
    WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);
    AddStyleCSS(&names_to_values_map);
    if (isTestEnvironment())
        names_to_values_map.insertScalar("test", "true");
    else
        names_to_values_map.insertScalar("test", "false");

    DbConnection db_connection(DbConnection::UBToolsFactory());
    ZoteroHarvester::Util::UploadTracker upload_tracker;
    const std::string default_action("list");
    const std::string action(WebUtil::GetCGIParameterOrDefault(cgi_args, "action", default_action));
    const std::string include_online_first(WebUtil::GetCGIParameterOrDefault(cgi_args, "include_online_first", ""));
    std::string config_overrides(WebUtil::GetCGIParameterOrDefault(cgi_args, "config_overrides"));
    if (include_online_first.empty())
        config_overrides.append((config_overrides.empty() ? "" : "\n") + SKIP_ONLINE_FIRST_TRUE_DIRECTIVE);
    const std::string url(WebUtil::GetCGIParameterOrDefault(cgi_args, "url"));

    if (action == "download")
        ProcessDownloadAction(cgi_args);
    else if (action == "show_downloaded")
        ProcessShowDownloadedAction(cgi_args, &upload_tracker, &db_connection);
    else if (action == "show_qa")
        ProcessShowQAAction(cgi_args, &db_connection);
    else if (action == "show_logs")
        ProcessShowLogsAction();
    else {
        names_to_values_map.insertScalar("action", action);

        std::string scripts_js;
        FileUtil::ReadString(TEMPLATE_DIRECTORY + "/" + "scripts.js", &scripts_js);
        names_to_values_map.insertScalar("scripts_js", scripts_js);

        const std::string depth(WebUtil::GetCGIParameterOrDefault(cgi_args, "depth", "1"));
        names_to_values_map.insertScalar("depth", depth);

        names_to_values_map.insertScalar("running_processes_count",
                                         std::to_string(ExecUtil::FindActivePrograms("zotero_harvester").size()));
        names_to_values_map.insertScalar("url", url);
        names_to_values_map.insertScalar("include_online_first", include_online_first);
        names_to_values_map.insertScalar("config_overrides", include_online_first.empty() ? StringUtil::ReplaceString(
                                                                 SKIP_ONLINE_FIRST_TRUE_DIRECTIVE, "", config_overrides)
                                                                                          : config_overrides);

        std::unordered_map<std::string, ZoteroHarvester::Config::GroupParams> group_name_to_params_map;
        std::unordered_map<std::string, ZoteroHarvester::Config::SubgroupParams> subgroup_name_to_params_map;
        std::unordered_map<std::string, std::string> journal_name_to_group_name_map;
        ParseConfigFile(cgi_args, &group_name_to_params_map, &subgroup_name_to_params_map, &journal_name_to_group_name_map, &db_connection,
                        &upload_tracker);
        RenderHtmlTemplate("index.html");

        std::string title, group_name;

        if (action != default_action) {
            if (action == "rss") {
                title = WebUtil::GetCGIParameterOrDefault(cgi_args, "rss_journal_title");
                group_name = journal_name_to_group_name_map.at(title);
            } else if (action == "crawling") {
                title = WebUtil::GetCGIParameterOrDefault(cgi_args, "crawling_journal_title");
                group_name = journal_name_to_group_name_map.at(title);
            } else if (action == "url") {
                title = WebUtil::GetCGIParameterOrDefault(cgi_args, "url_journal_title");
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
