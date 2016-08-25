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
        Error("weird escaped string end in backslash \"" + escaped_text + "\"!");
    
    list->emplace_back(StringUtil::RightTrim(&unescaped_text, '\n'));
}


struct Translation {
    std::string index_, language_code_, text_, category_;
};


void ParseGetMissingLine(const std::string &line, Translation * const translation) {
    std::vector<std::string> parts;
    ParseEscapedCommaSeparatedList(line, &parts);
    if (unlikely(parts.size() != 4))
        Error("expected 4 parts, found \"" + line + "\"!");
    
    translation->index_         = parts[0];
    translation->language_code_ = parts[1];
    translation->text_          = parts[2];
    translation->category_      = parts[3];
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


void ParseTranslationsDbToolOutputAndGenerateNewDisplay(const std::string &output, const std::string &language_code) {
    std::vector<Translation> translations;
    ParseTranslationsDbToolOutput(output, &translations);
    if (translations.empty()) {
        std::ifstream done_html("/var/lib/tuelib/translate_chainer/done_translating.html", std::ios::binary);
        std::cout << done_html.rdbuf();
    } else {
        std::map<std::string, std::vector<std::string>> names_to_values_map;
        names_to_values_map.emplace(std::make_pair(std::string("index"),
                                                   std::vector<std::string>{ translations.front().index_ }));
        names_to_values_map.emplace(std::make_pair(std::string("target_language_code"),
                                                   std::vector<std::string>{ language_code }));
        names_to_values_map.emplace(std::make_pair(std::string("category"),
                                                   std::vector<std::string>{ translations.front().category_ }));

        std::vector<std::string> language_codes, example_texts;
        for (const auto &translation : translations) {
            language_codes.emplace_back(translation.language_code_);
            example_texts.emplace_back(translation.text_);
        }
        names_to_values_map.emplace(std::make_pair(std::string("language_code"), language_codes));
        names_to_values_map.emplace(std::make_pair(std::string("example_text"), example_texts));

        std::ifstream translate_html("/var/lib/tuelib/translate_chainer/translate.html", std::ios::binary);
        MiscUtil::ExpandTemplate(translate_html, std::cout, names_to_values_map);
    }
}


// The first call should only provide the 3-letter language code.
void GetMissing(const std::multimap<std::string, std::string> &cgi_args) {
    const std::string language_code(GetCGIParameterOrDie(cgi_args, "language_code"));

    const std::string GET_MISSING_COMMAND("/usr/local/bin/translation_db_tool get_missing " + language_code);
    std::string output;
    if (not ExecUtil::ExecSubcommandAndCaptureStdout(GET_MISSING_COMMAND, &output))
        Error("failed to execute \"" + GET_MISSING_COMMAND + "\" or it returned a non-zero exit code!");

    ParseTranslationsDbToolOutputAndGenerateNewDisplay(output, language_code);
}


// A standard call should provide "index", "language_code", "category" and "translation".
void Insert(const std::multimap<std::string, std::string> &cgi_args) {
    const std::string language_code(GetCGIParameterOrDie(cgi_args, "language_code"));
    const std::string translation(GetCGIParameterOrDie(cgi_args, "translation"));
    const std::string index(GetCGIParameterOrDie(cgi_args, "index"));
    const std::string category(GetCGIParameterOrDie(cgi_args, "category"));

    if (translation.empty())
        return;
    
    const std::string INSERT_COMMAND("/usr/local/bin/translation_db_tool insert " + index + " " + language_code
                                     + " " + category + " " + translation);
    std::string output;
    if (not ExecUtil::ExecSubcommandAndCaptureStdout(INSERT_COMMAND, &output))
        Error("failed to execute \"" + INSERT_COMMAND + "\" or it returned a non-zero exit code!");
}


void EmitHeader() {
    std::cout << "Content-Type: text/html; charset=utf-8\r\n\r\n";
}

void EmitRedirectHeader(const std::string language_code) {
    std::cout << "Status: 302 Found\r\n";
    std::cout << "Location: /cgi-bin/translate_chainer?language_code=" << language_code << "\r\n\r\n";
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        std::multimap<std::string, std::string> cgi_args;
        WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);
        
        if (cgi_args.size() == 1) {
            EmitHeader();
            GetMissing(cgi_args);
        } else if (cgi_args.size() == 4) {
            const std::string language_code(GetCGIParameterOrDie(cgi_args, "language_code"));
            EmitRedirectHeader(language_code);
            Insert(cgi_args);
        } else
            Error("we should be called w/ either 1 or 4 CGI arguments!");
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
