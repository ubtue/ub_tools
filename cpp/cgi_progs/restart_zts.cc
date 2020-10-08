/** \file    restart_zts
 *  \brief   Restart the docker container with the Zotero Translation Server
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2020, Library of the University of Tübingen

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

#include <iostream>
#include <sstream>
#include "ExecUtil.h"
#include "StringUtil.h"
#include "WebUtil.h"
#include "util.h"

namespace {

void SendHeaders() {
    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n"
              << "<html>\n";
}


void SendTrailer() {
    std::cout << "</html>\n";

}


void DisplayRestartButton() {
    SendHeaders();
    std::cout << "<h2>Restart Zotero Translation Server Service</h2>\n"
              << "<form action=\"\" method=\"post\">\n"
              << "\t<input type=\"submit\" name=\"action\" value=\"Restart\">\n"
              << "</form>\n";
    SendTrailer();
}


bool IsRestartActionPresent(std::multimap<std::string, std::string> *cgi_args) {
    WebUtil::GetAllCgiArgs(cgi_args);
    const auto key_and_value(cgi_args->find("action"));
    return key_and_value != cgi_args->cend() and key_and_value->second == "Restart";
}


void OutputZTSStatus() {
    const std::string tmp_output(FileUtil::AutoTempFile().getFilePath());
    ExecUtil::ExecOrDie("/usr/bin/sudo", { "systemctl", "status", "zts" }, "" /*stdin*/,
                        tmp_output, tmp_output);

    std::ifstream zts_output_file(tmp_output);
    if (not zts_output_file)
        LOG_ERROR("Could not open " + tmp_output + " for reading\n");
    std::stringstream zts_output_istream;
    zts_output_istream << zts_output_file.rdbuf();
    std::cout << StringUtil::ReplaceString("\n", "<br/>", zts_output_istream.str());
}


void RestartZTS() {
    SendHeaders();
    std::cout << "<h2>Attempting restart of ZTS...</h2>\n" << std::endl;
    bool log_no_decorations_old(logger->getLogNoDecorations());
    bool log_strip_call_site_old(logger->getLogStripCallSite());
    logger->setLogNoDecorations(true);
    logger->setLogStripCallSite(true);
    logger->redirectOutput(STDOUT_FILENO);
    try {
        ExecUtil::ExecOrDie("/usr/bin/sudo", { "systemctl", "restart", "zts" });
        OutputZTSStatus();
    } catch (const std::runtime_error &error) { 
        std::cerr << error.what();
    }
    std::cout << "<h2>Done...</h>\n";
    SendTrailer();
    logger->redirectOutput(STDERR_FILENO);
    logger->setLogNoDecorations(log_no_decorations_old);
    logger->setLogStripCallSite(log_strip_call_site_old);
}


} // end unnamed namespace

int Main(int /*argc*/, char */*argv*/[]) {
    std::multimap<std::string, std::string> cgi_args;
    if (not IsRestartActionPresent(&cgi_args)) {
        DisplayRestartButton();
        return EXIT_SUCCESS;
    } 
    RestartZTS();
    return EXIT_SUCCESS;
}





