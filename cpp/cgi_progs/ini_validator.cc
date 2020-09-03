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


std::string GetCGIParameterOrDefault(const std::multimap<std::string, std::string> &cgi_args,
                                     const std::string &parameter_name,
                                     const std::string &default_value = "")
{
    const auto key_and_value(cgi_args.find(parameter_name));
    if (key_and_value == cgi_args.cend())
        return default_value;

    return key_and_value->second;
}


void ShowUploadForm() {
    std::cout << "<h1>INI validator</h1>\n";
    std::cout << "<p>Please paste the content of the INI file here:</p>\n";
    std::cout << "<form id=\"upload_form\" method=\"post\">\n";
    std::cout << "\t<textarea name=\"ini_content\" rows=\"20\" cols=\"100\"></textarea>\n";
    std::cout << "\t<input type=\"hidden\" name=\"action\" value=\"validate\">\n";
    std::cout << "\t<br><br>\n";
    std::cout << "\t<button onclick=\"document.getElementById(\"upload_form\").submit()\">Validate</button>\n";
    std::cout << "</form>\n";
}


void Validate(const std::multimap<std::string, std::string> &cgi_args) {

    std::cout << "<h1>Validate</h1>";

    FileUtil::AutoTempFile temp_file("/tmp/ATF", "", false);
    std::string ini_content(GetCGIParameterOrDefault(cgi_args, "ini_content", ""));
    StringUtil::RemoveChars("\r", &ini_content);
    FileUtil::WriteStringOrDie(temp_file.getFilePath(), ini_content);

    // Redirect stderr to stdout
    // flush is important, else we have invalid script headers!
    logger->redirectOutput(STDOUT_FILENO);
    std::cout << "<font color=\"red\">";
    std::cout.flush();
    IniFile ini_file(temp_file.getFilePath());
    std::cout << "</font>";
    logger->redirectOutput(STDERR_FILENO);

    // This message will only be shown if IniFile has been successfully parsed
    std::cout << "<p>Validation successful</p>";
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    std::multimap<std::string, std::string> cgi_args;
    WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);
    const std::string action(GetCGIParameterOrDefault(cgi_args, "action", ""));

    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";
    std::cout << "<html>";
    if (action == "validate")
        Validate(cgi_args);
    else
        ShowUploadForm();
    std::cout << "</html>";

    return EXIT_SUCCESS;
}
