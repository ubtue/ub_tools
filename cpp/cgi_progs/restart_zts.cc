/** \file    restart_zts
 *  \brief   Restart the docker container with the Zotero Translation Server
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2020,2021, Library of the University of Tübingen

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
#include <functional>
#include <iostream>
#include <sstream>
#include "ExecUtil.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "WebUtil.h"
#include "util.h"

namespace {

const std::string ZTS_RESTART_CONFIG("/usr/local/var/lib/tuelib/restart_zts.conf");
// Make sure to match this directory in /etc/sudoers.d/99-zts-restart otherwise symbolic linking will fail
const std::string ZTS_TRANSLATORS_DIR("/usr/local/zotero-translators");
const std::string ZOTERO_ENHANCEMENT_MAPS_DIR("/usr/local/var/lib/tuelib/zotero-enhancement-maps");


void SendHeaders() {
    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n"
              << "<html>\n";
}


void SendTrailer() {
    std::cout << "</html>\n";
}

struct TranslatorsLocationConfig {
    std::string name_;
    std::string url_;
    std::string local_path_;
    std::string branch_;
    std::string zotero_enhancement_maps_local_path_;
    std::string zotero_enhancement_maps_branch_;
    TranslatorsLocationConfig(std::string name = "", std::string url = "", std::string local_path = "", std::string branch = "",
                              std::string zotero_enhancement_maps_local_path = "", std::string zotero_enhancement_maps_branch = "")
        : name_(name), url_(url), local_path_(local_path), branch_(branch),
          zotero_enhancement_maps_local_path_(zotero_enhancement_maps_local_path),
          zotero_enhancement_maps_branch_(zotero_enhancement_maps_branch) { }
};


void GetTranslatorLocationConfigs(const IniFile &ini_file, std::vector<TranslatorsLocationConfig> *translators_location_configs) {
    translators_location_configs->clear();
    const std::string location_prefix("Repo_");
    for (const auto &section : ini_file) {
        if (StringUtil::StartsWith(section.getSectionName(), location_prefix)) {
            TranslatorsLocationConfig translators_location_config;
            translators_location_config.name_ = section.getSectionName().substr(location_prefix.length());
            translators_location_config.url_ = section.getString("url", "");
            translators_location_config.local_path_ = section.getString("local_path");
            translators_location_config.branch_ = section.getString("branch");
            translators_location_config.zotero_enhancement_maps_local_path_ = section.getString("zotero_enhancement_maps_local_path");
            translators_location_config.zotero_enhancement_maps_branch_ = section.getString("zotero_enhancement_maps_branch");
            translators_location_configs->emplace_back(translators_location_config);
        }
    }
}

bool IsRestartActionPresent(const std::multimap<std::string, std::string> &cgi_args) {
    const auto key_and_value(cgi_args.find("action"));
    return key_and_value != cgi_args.cend() and key_and_value->second == "Restart";
}


void ExecuteAndDumpMessages(const std::string &command, const std::vector<std::string> &args) {
    auto auto_temp_file((FileUtil::AutoTempFile()));
    const std::string tmp_output(auto_temp_file.getFilePath());
    ExecUtil::ExecOrDie(command, args, "" /*stdin*/, tmp_output, "/dev/stdout");
    std::ifstream output_file(tmp_output);
    if (not output_file)
        LOG_ERROR("Could not open " + tmp_output + " for reading\n");
    std::stringstream output_istream;
    output_istream << output_file.rdbuf();
    std::cout << StringUtil::ReplaceString("\n", "<br/>", output_istream.str());
}


template <typename Function>
void ExecuteAndDisplayStatus(const std::string &header_msg, Function function, std::string footer_msg = "") {
    std::cout << header_msg << std::endl;
    bool log_no_decorations_old(logger->getLogNoDecorations());
    bool log_strip_call_site_old(logger->getLogStripCallSite());
    logger->setLogNoDecorations(true);
    logger->setLogStripCallSite(true);
    logger->redirectOutput(STDOUT_FILENO);
    try {
        function();
    } catch (const std::runtime_error &error) {
        std::cerr << error.what();
    }
    std::cout << footer_msg << std::endl;
    logger->redirectOutput(STDERR_FILENO);
    logger->setLogNoDecorations(log_no_decorations_old);
    logger->setLogStripCallSite(log_strip_call_site_old);
}


template <typename Function>
void ExecuteAndSendStatus(const std::string &message, Function function) {
    SendHeaders();
    ExecuteAndDisplayStatus(message, function);
    SendTrailer();
}


void GetCurrentRepoAndBranch() {
    const std::string chdir_to_translators_dir("cd " + ZTS_TRANSLATORS_DIR + "/translators");
    auto closure = [&] {
        ExecuteAndDumpMessages("/usr/bin/sudo", { "/bin/bash", "-c", "/usr/local/bin/restart_zts_show_current_gitrepo.sh" });
    };
    ExecuteAndDisplayStatus("<h4>Current repo and branch </h4>", closure, "<p>");
}


void DisplayRestartAndSelectButtons(const std::vector<TranslatorsLocationConfig> &translators_location_configs) {
    SendHeaders();
    std::cout << "<h2>Restart Zotero Translation Server Service</h2>\n";
    GetCurrentRepoAndBranch();
    std::cout << "<form action=\"\" method=\"post\">\n";
    for (const auto &translators_location_config : translators_location_configs)
        std::cout << "\t<input type=\"submit\" name=\"action\" value=\"" + translators_location_config.name_ + "\">\n";
    std::cout << "<p/><hr/><p/>" << std::endl;
    std::cout << "\t<input type=\"submit\" name=\"action\" value=\"Restart\">\n"
              << "</form>\n";
    SendTrailer();
}


void RestartZTS() {
    auto closure = [] {
        ExecUtil::ExecOrDie("/usr/bin/sudo", { "systemctl", "restart", "zts" });
        ExecuteAndDumpMessages("/usr/bin/sudo", { "systemctl", "status", "zts" });
    };
    ExecuteAndSendStatus("<h2>Trying to restart ZTS Server</h2>", closure);
}


void RelinkTranslatorAndEnhancemenMapsDirectory(const TranslatorsLocationConfig &translators_location_config) {
    auto closure = [&] {
        ExecuteAndDumpMessages("/usr/bin/sudo", { "ln", "--symbolic", "--force", "--no-dereference",
                                                  translators_location_config.local_path_, ZTS_TRANSLATORS_DIR });
        std::cout << "Linking " << ZTS_TRANSLATORS_DIR << " to " << translators_location_config.local_path_ << "<br/>";
        ExecuteAndDumpMessages("/usr/bin/sudo",
                               { "ln", "--symbolic", "--force", "--no-dereference",
                                 translators_location_config.zotero_enhancement_maps_local_path_, ZOTERO_ENHANCEMENT_MAPS_DIR });
        std::cout << "Linking " << ZOTERO_ENHANCEMENT_MAPS_DIR << " to " << translators_location_config.zotero_enhancement_maps_local_path_
                  << "<br/>";
        RestartZTS();
    };
    ExecuteAndSendStatus("<h2>Switching to branch " + translators_location_config.name_ + "</h2>", closure);
}


bool GetSwitchBranch(const std::multimap<std::string, std::string> &cgi_args,
                     std::vector<TranslatorsLocationConfig> translators_location_configs,
                     TranslatorsLocationConfig * const translator_location_config) {
    const auto key_and_value(cgi_args.find("action"));
    if (key_and_value == cgi_args.end())
        return false;
    const std::string target(key_and_value->second);
    auto match(std::find_if(translators_location_configs.begin(), translators_location_configs.end(),
                            [&target](const TranslatorsLocationConfig &target_obj) { return target_obj.name_ == target; }));
    if (match == translators_location_configs.end()) {
        std::cout << "NO MATCH";
        return false;
    }
    *translator_location_config = *match;
    return true;
}


} // end unnamed namespace


int Main(int /*argc*/, char * /*argv*/[]) {
    std::multimap<std::string, std::string> cgi_args;
    WebUtil::GetAllCgiArgs(&cgi_args);
    IniFile ini_file(ZTS_RESTART_CONFIG);
    std::vector<TranslatorsLocationConfig> translators_location_configs;
    GetTranslatorLocationConfigs(ini_file, &translators_location_configs);
    if (IsRestartActionPresent(cgi_args)) {
        RestartZTS();
        return EXIT_SUCCESS;
    }

    TranslatorsLocationConfig translators_location_config;
    if (GetSwitchBranch(cgi_args, translators_location_configs, &translators_location_config)) {
        RelinkTranslatorAndEnhancemenMapsDirectory(translators_location_config);
        return EXIT_SUCCESS;
    }

    DisplayRestartAndSelectButtons(translators_location_configs);
    return EXIT_SUCCESS;
}
