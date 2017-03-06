/** \file    crossref_downloader.cc
 *  \brief   Downloads metadata from crossref.org and generates MARC-21 records.
 *  \author  Dr. Johannes Ruscheinski
 *
 *  \copyright (C) 2017, Library of the University of Tübingen
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "Compiler.h"
#include "Downloader.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UrlUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--timeout seconds] journal_list marc_output\n";
    std::exit(EXIT_FAILURE);
}


// Compares "s1" and "s2" while ignoring any occurences of characters found in "ignore_chars".
bool EqualIgnoreChars(const std::string &s1, const std::string &s2, const std::string &ignore_chars) {
    auto ch1(s1.cbegin());
    auto ch2(s2.cbegin());
    while (ch1 != s1.cend() and ch2 != s2.cend()) {
        if (ignore_chars.find(*ch1) != std::string::npos)
            ++ch1;
        else if (ignore_chars.find(*ch2) != std::string::npos)
            ++ch2;
        else if (*ch1 != *ch2)
            return false;
        else
            ++ch1, ++ch2;
    }

    while (ch1 != s1.cend() and ignore_chars.find(*ch1) != std::string::npos)
        ++ch1;
    while (ch2 != s2.cend() and ignore_chars.find(*ch2) != std::string::npos)
        ++ch2;

    return ch1 == s1.cend() and ch2 == s2.cend();
}


bool FuzzyTextMatch(const std::string &s1, const std::string &s2) {
    std::string lowercase_s1;
    if (unlikely(not TextUtil::UTF8ToLower(s1, &lowercase_s1)))
        Error("failed to convert supposed UTF-8 string \"" + s1 + "\" to a wide-character string! (1)");

    std::string lowercase_s2;
    if (unlikely(not TextUtil::UTF8ToLower(s2, &lowercase_s2)))
        Error("failed to convert supposed UTF-8 string \"" + s2 + "\" to a wide-character string! (2)");

    static const std::string IGNORE_CHARS(" :-");
    return EqualIgnoreChars(lowercase_s1, lowercase_s2, IGNORE_CHARS);
}


bool ProcessJournal(const unsigned timeout, const std::string &journal_name) {
        std::string json_document;
        if (Download("https://search.crossref.org/dois?q=" + UrlUtil::UrlEncode(journal_name), timeout,
                     &json_document) != 0)
            return false;

        std::stringstream query_input(json_document, std::ios_base::in);
        boost::property_tree::ptree query_property_tree;
        boost::property_tree::json_parser::read_json(query_input, query_property_tree);

        unsigned document_count(0);
        for (const auto &array_entry : query_property_tree) {
            const std::string doi_url(array_entry.second.get_child("doi").data());
            if (Download("https://api.crossref.org/v1/works/" + UrlUtil::UrlEncode(doi_url), timeout,
                         &json_document) != 0)
                continue;

            std::stringstream record_input(json_document, std::ios_base::in);
            boost::property_tree::ptree record_property_tree;
            boost::property_tree::json_parser::read_json(record_input, record_property_tree);
            const boost::property_tree::ptree::const_assoc_iterator container_titles(
                record_property_tree.get_child("message").find("container-title"));
            if (container_titles == record_property_tree.not_found())
                continue;

            bool matched_at_least_one(false);
            for (const auto container_title_node : container_titles->second) {
                if (FuzzyTextMatch(journal_name, container_title_node.second.data())) {
                    matched_at_least_one = true;
                    break;
                }
            }
            if (not matched_at_least_one)
                continue;

            ++document_count;
        }

        return document_count > 0;
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3 and argc != 5)
        Usage();

    const unsigned DEFAULT_TIMEOUT(20); // seconds
    unsigned timeout(DEFAULT_TIMEOUT);
    if (std::strcmp(argv[1], "--timeout") == 0) {
        if (not StringUtil::ToUnsigned(argv[2], &timeout))
            Error("bad timeout \"" + std::string(argv[2]) + "\"!");
        argc -= 2;
        argv += 2;
    }

    if (argc != 3)
        Usage();

    const std::string journal_list_filename(argv[1]);
    const std::string marc_output_filename(argv[2]);

    try {
        const std::unique_ptr<File> journal_list_file(FileUtil::OpenInputFileOrDie(journal_list_filename));

        unsigned success_count(0);
        while (not journal_list_file->eof()) {
            std::string line;
            journal_list_file->getline(&line);
            StringUtil::Trim(&line);
            if (not line.empty() and ProcessJournal(timeout, line))
                ++success_count;
        }

        std::cout << "Downloaded metadata for at least one article from " << success_count << " journals.\n";
        return success_count == 0 ? EXIT_FAILURE : EXIT_SUCCESS;
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
