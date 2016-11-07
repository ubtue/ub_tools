/** \file    translate_chainer.cc
 *  \brief   Simple tool for generating a sequence of Web pages for translations.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016, Library of the University of Tübingen

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
#include "MiscUtil.h"
#include "StringUtil.h"
#include "UrlUtil.h"
#include "WebUtil.h"
#include "util.h"


void DumpCgiArgs(const std::multimap<std::string, std::string> &cgi_args) {
    for (const auto &key_and_values : cgi_args)
        std::cout << key_and_values.first << " = " << key_and_values.second << '\n';
}


void ParseEscapedCommaSeparatedList(const std::string &escaped_text, std::vector<std::string> * const list) {
    list->clear();

    std::string unescaped_text;
    bool last_char_was_backslash(false);

    for (const auto ch : escaped_text) {
        if (last_char_was_backslash) {
            last_char_was_backslash = false;
            unescaped_text += ch;
        } else if (ch == '\\')
            last_char_was_backslash = true;
        else if (ch == ',') {
            list->emplace_back(unescaped_text);
            unescaped_text.clear();
        } else
            unescaped_text += ch;
    }

    if (unlikely(last_char_was_backslash))
        Error("weird escaped string ends in backslash \"" + escaped_text + "\"!");

    list->emplace_back(StringUtil::RightTrim(&unescaped_text, '\n'));
}


struct Translation {
    std::string index_, remaining_count_, language_code_, text_, category_, gnd_code_;
};


const std::string NO_GND_CODE("-1");


void ParseGetMissingLine(const std::string &line, Translation * const translation) {
    std::vector<std::string> parts;
    ParseEscapedCommaSeparatedList(line, &parts);
    if (unlikely(parts.size() != 5 and parts.size() != 6))
        Error("expected 5 or 6 parts, found \"" + line + "\"!");

    translation->index_           = parts[0];
    translation->remaining_count_ = parts[1];
    translation->language_code_   = parts[2];
    translation->text_            = parts[3];
    translation->category_        = parts[4];
    translation->gnd_code_        = (parts.size() == 5) ? NO_GND_CODE : parts[5];
}


void ParseTranslationsDbToolOutput(const std::string &output, std::vector<Translation> * const translations) {
    std::vector<std::string> lines;
    StringUtil::Split(output, '\n', &lines);

    for (const auto &line : lines) {
        Translation new_translation;
        ParseGetMissingLine(line, &new_translation);
        translations->emplace_back(new_translation);
    }
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


void ParseTranslationsDbToolOutputAndGenerateNewDisplay(const std::string &output, const std::string &language_code,
                                                        const std::string &ixtheo_base_url)
{
    std::vector<Translation> translations;
    ParseTranslationsDbToolOutput(output, &translations);
    if (translations.empty()) {
        std::ifstream done_html("/var/lib/tuelib/translate_chainer/done_translating.html", std::ios::binary);
        std::cout << done_html.rdbuf();
    } else {
        std::map<std::string, std::vector<std::string>> names_to_values_map;
        names_to_values_map.emplace(std::make_pair(std::string("index"),
                                                   std::vector<std::string>{ translations.front().index_ }));
        names_to_values_map.emplace(std::make_pair(std::string("remaining_count"),
                                                   std::vector<std::string>{ translations.front().remaining_count_ }));
        names_to_values_map.emplace(std::make_pair(std::string("target_language_code"),
                                                   std::vector<std::string>{ language_code }));
        names_to_values_map.emplace(std::make_pair(std::string("ixtheo_base_url"),
                                                   std::vector<std::string>{ UrlUtil::UrlDecode(ixtheo_base_url) }));
        names_to_values_map.emplace(std::make_pair(std::string("category"),
                                                   std::vector<std::string>{ translations.front().category_ }));
        if (translations.front().gnd_code_ != NO_GND_CODE)
            names_to_values_map.emplace(std::make_pair(std::string("gnd_code"),
                                                       std::vector<std::string>{ translations.front().gnd_code_ }));

        std::vector<std::string> language_codes, example_texts, url_escaped_example_texts;
        for (const auto &translation : translations) {
            language_codes.emplace_back(translation.language_code_);
            example_texts.emplace_back(translation.text_);
            url_escaped_example_texts.emplace_back(UrlUtil::UrlEncode(translation.text_));
        }
        names_to_values_map.emplace(std::make_pair(std::string("language_code"), language_codes));
        names_to_values_map.emplace(std::make_pair(std::string("example_text"), example_texts));
        names_to_values_map.emplace(std::make_pair(std::string("url_escaped_example_text"),
                                                   url_escaped_example_texts));

        std::ifstream translate_html("/var/lib/tuelib/translate_chainer/translate.html", std::ios::binary);
        MiscUtil::ExpandTemplate(translate_html, std::cout, names_to_values_map);
    }
}


void GetMissing(const std::multimap<std::string, std::string> &cgi_args) {
    const std::string language_code(GetCGIParameterOrDie(cgi_args, "language_code"));
    const std::string ixtheo_base_url(GetCGIParameterOrDie(cgi_args, "ixtheo_base_url"));

    const std::string GET_MISSING_COMMAND("/usr/local/bin/translation_db_tool get_missing " + language_code);
    std::string output;
    if (not ExecUtil::ExecSubcommandAndCaptureStdout(GET_MISSING_COMMAND, &output))
        Error("failed to execute \"" + GET_MISSING_COMMAND + "\" or it returned a non-zero exit code!");

    ParseTranslationsDbToolOutputAndGenerateNewDisplay(output, language_code, ixtheo_base_url);
}


void Insert(const std::multimap<std::string, std::string> &cgi_args) {
    const std::string language_code(GetCGIParameterOrDie(cgi_args, "language_code"));
    const std::string translation(GetCGIParameterOrDie(cgi_args, "translation"));
    const std::string index(GetCGIParameterOrDie(cgi_args, "index"));
    const std::string gnd_code(GetCGIParameterOrEmptyString(cgi_args, "gnd_code"));

    if (translation.empty())
        return;

    std::string insert_command("/usr/local/bin/translation_db_tool insert '" + index);
    if (not gnd_code.empty())
        insert_command += "' '" + gnd_code;
    insert_command += "' " + language_code + " '" + translation + "'";

    std::string output;
    if (not ExecUtil::ExecSubcommandAndCaptureStdout(insert_command, &output))
        Error("failed to execute \"" + insert_command + "\" or it returned a non-zero exit code!");
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        std::multimap<std::string, std::string> cgi_args;
        WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);

        if (cgi_args.size() == 1) {
            std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";
            GetMissing(cgi_args);
        } else if (cgi_args.size() == 5) {
            const std::string language_code(GetCGIParameterOrDie(cgi_args, "language_code"));
            const std::string ixtheo_base_url(GetCGIParameterOrDie(cgi_args, "ixtheo_base_url"));
            std::cout << "Status: 302 Found\r\n";
            std::cout << "Location: /cgi-bin/translate_chainer?language_code=" << language_code
                      << "&ixtheo_base_url=" << ixtheo_base_url << "\r\n\r\n";
            Insert(cgi_args);
        } else
            Error("we should be called w/ either 1 or 5 CGI arguments!");
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
