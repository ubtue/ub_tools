/** \file    translator_ajax.cc
 *  \brief   Interface for updating translations by ajax requests
 *  \author  Johannes Riedl
 */

/*
    Copyright (C) 2017, Library of the University of Tübingen

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
#include <fstream>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include "Compiler.h"
#include "ExecUtil.h"
#include "HtmlUtil.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "WebUtil.h"
#include "util.h"

const std::string getTranslatorOrEmptryString() {
    return (std::getenv("REMOTE_USER") != nullptr) ? std::getenv("REMOTE_USER") : "";
}


void DumpCgiArgs(const std::multimap<std::string, std::string> &cgi_args) {
    for (const auto &key_and_values : cgi_args)
        std::cout << key_and_values.first << " = " << key_and_values.second << '\n';
}


std::string GetCGIParameterOrDie(const std::multimap<std::string, std::string> &cgi_args,
                                 const std::string &parameter_name)
{
    const auto key_and_value(cgi_args.find(parameter_name));
    if (key_and_value == cgi_args.cend())
        Error("expected a(n) \"" + parameter_name + "\" parameter!");

    return key_and_value->second;
}


std::string GetCGIParameterOrEmptyString(const std::multimap<std::string, std::string> &cgi_args,
                                         const std::string &parameter_name)
{
    const auto key_and_value(cgi_args.find(parameter_name));
    if (key_and_value == cgi_args.cend())
        return "";

    return key_and_value->second;
}


std::string GetEnvParameterOrEmptyString(const std::multimap<std::string, std::string> &env_args,
                                         const std::string &parameter_name)
{
    const auto key_and_value(env_args.find(parameter_name));
    if (key_and_value == env_args.cend())
        return "";

    return key_and_value->second;
}



void Update(const std::multimap<std::string, std::string> &cgi_args, const std::multimap<std::string, std::string> &env_args) {
    const std::string language_code(GetCGIParameterOrDie(cgi_args, "language_code"));
    const std::string translation(GetCGIParameterOrDie(cgi_args, "translation"));
    const std::string index(GetCGIParameterOrDie(cgi_args, "index"));
    const std::string gnd_code(GetCGIParameterOrEmptyString(cgi_args, "gnd_code"));
    const std::string translator(GetEnvParameterOrEmptyString(env_args, "REMOTE_USER"));

    std::string update_command("/usr/local/bin/translation_db_tool update '" + index);
    if (not gnd_code.empty())
        update_command += "' '" + gnd_code;
    update_command += "' " + language_code + " \"" + translation + "\" '" + translator + "'";

    std::string output;
    if (not ExecUtil::ExecSubcommandAndCaptureStdout(update_command, &output))
        Error("failed to execute \"" + update_command + "\" or it returned a non-zero exit code!");
}


void Insert(const std::multimap<std::string, std::string> &cgi_args, const std::multimap<std::string, std::string> &env_args) {
    const std::string language_code(GetCGIParameterOrDie(cgi_args, "language_code"));
    const std::string translation(GetCGIParameterOrDie(cgi_args, "translation"));
    const std::string index(GetCGIParameterOrDie(cgi_args, "index"));
    const std::string gnd_code(GetCGIParameterOrEmptyString(cgi_args, "gnd_code"));
    const std::string translator(GetEnvParameterOrEmptyString(env_args, "REMOTE_USER"));

    if (translation.empty())
        return;

    std::string insert_command("/usr/local/bin/translation_db_tool insert '" + index);
    if (not gnd_code.empty())
        insert_command += "' '" + gnd_code;
    insert_command += "' " + language_code + " \"" + translation + "\" '" + translator + "'";

    std::string output;
    if (not ExecUtil::ExecSubcommandAndCaptureStdout(insert_command, &output))
        Error("failed to execute \"" + insert_command + "\" or it returned a non-zero exit code!");
}

int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        std::multimap<std::string, std::string> cgi_args;
        WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);

        std::multimap<std::string, std::string> env_args;
        env_args.insert(std::make_pair("REMOTE_USER", getTranslatorOrEmptryString()));

        if (cgi_args.size() == 5 or cgi_args.size() == 6) {
            const std::string action(GetCGIParameterOrDie(cgi_args, "action"));
            std::string status = "Status: 501 Not Implemented";
            if (action == "insert") {
                Insert(cgi_args, env_args);
                status = "Status: 201 Created\r\n";
            }
            else if (action == "update") {
                Update(cgi_args, env_args);
                status = "Status: 200 OK\r\n";
            }
            else
                Error("Unknown action: " + action + "! Expecting 'insert' or 'update'.");

            std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";
            const std::string language_code(GetCGIParameterOrDie(cgi_args, "language_code"));
            std::cout << status;
        } else
            Error("we should be called w/ either or 5 or 6 CGI arguments!");
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}


