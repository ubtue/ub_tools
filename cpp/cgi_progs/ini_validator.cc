/** \file    ini_validator.cc
 *  \brief   A CGI-tool to validate INI files
 *  \author  Mario Trojan
 */
/*
    Copyright (C) 2020, Tübingen University Library

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
#include "FileUtil.h"
#include "HtmlUtil.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "util.h"
#include "WebUtil.h"


namespace {


void ShowUploadForm() {
    std::cout << "<h1>INI validator</h1>\n"
              << "<p>Please paste the contents of the INI file here:</p>\n"
              << "<form id=\"upload_form\" method=\"post\">\n"
              << "\t<textarea name=\"ini_content\" rows=\"20\" cols=\"100\"></textarea>\n"
              << "\t<input type=\"hidden\" name=\"action\" value=\"validate\">\n"
              << "\t<br><br>\n"
              << "\t<button onclick=\"document.getElementById(\"upload_form\").submit()\">Validate</button>\n"
              << "</form>\n";
}


void Validate(const std::multimap<std::string, std::string> &cgi_args) {

    std::cout << "<h1>Validate</h1>\n";

    FileUtil::AutoTempFile temp_file;
    std::string ini_content(WebUtil::GetCGIParameterOrDefault(cgi_args, "ini_content", ""));
    StringUtil::RemoveChars("\r", &ini_content);
    FileUtil::WriteStringOrDie(temp_file.getFilePath(), ini_content);

    // Redirect stderr to stdout
    // flush is important, else we have invalid script headers!
    std::cout << "<font color=\"red\">";
    std::cout.flush();
    bool log_no_decorations_old(logger->getLogNoDecorations());
    bool log_strip_call_site_old(logger->getLogStripCallSite());
    logger->setLogNoDecorations(true);
    logger->setLogStripCallSite(true);
    logger->redirectOutput(STDOUT_FILENO);
    try {
        // this will either exit directly (LOG_ERROR) or throw a std::runtime_error
        // so we need to cover both cases
        IniFile ini_file(temp_file.getFilePath());
        std::cout << "</font>\n";
        std::cout << "<font color=\"green\">Validation successful</font>\n";
    }
    catch(const std::runtime_error &e) {
        std::cout << e.what();
    }

    logger->redirectOutput(STDERR_FILENO);
    logger->setLogNoDecorations(log_no_decorations_old);
    logger->setLogStripCallSite(log_strip_call_site_old);

}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    std::multimap<std::string, std::string> cgi_args;
    WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);
    const std::string action(WebUtil::GetCGIParameterOrDefault(cgi_args, "action", ""));

    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";
    std::cout << "<html>";
    if (action == "validate")
        Validate(cgi_args);
    else
        ShowUploadForm();
    std::cout << "</html>";

    return EXIT_SUCCESS;
}
