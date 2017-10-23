/** \file    summarize_logs.cc
 *  \brief   Summarizes Solr logs.
 *  \author  Dr. Johannes Ruscheinski
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

/*  We offer a list of tags and subfields where the primary data resides along
    with a list of tags and subfields where the synonym data is found and
    a list of unused fields in the title data where the synonyms can be stored
*/

#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " log_file_input summary_output\n";
    std::exit(EXIT_FAILURE);
}


std::string GetLoggingLevel(const std::string &line) {
    if (StringUtil::StartsWith(line, "DEBUG"))
        return "DEBUG";
    if (StringUtil::StartsWith(line, "INFO"))
        return "INFO";
    if (StringUtil::StartsWith(line, "WARN"))
        return "WARN";
    if (StringUtil::StartsWith(line, "SEVERE"))
        return "SEVERE";
    logger->error("in GetLoggingLevel: can't determine logging level for line: " + line);
}


bool LineAndFrequencyCompare(const std::pair<std::string, unsigned> &line_and_frequency1,
                             const std::pair<std::string, unsigned> &line_and_frequency2)
{
    const std::string logging_level1(GetLoggingLevel(line_and_frequency1.first));
    const std::string logging_level2(GetLoggingLevel(line_and_frequency2.first));
    if (logging_level1 != logging_level2) {
        if (logging_level1 == "DEBUG")
            return false;
        if (logging_level2 == "DEBUG")
            return true;
        if (logging_level1 == "INFO")
            return false;
        if (logging_level2 == "INFO")
            return true;
        if (logging_level1 == "WARN")
            return false;
        if (logging_level2 == "WARN")
            return true;
    }

    return line_and_frequency1.second > line_and_frequency2.second;
}


void SummarizeLog(File * const log_file, File * const summary_file) {
    std::string err_msg;
    RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("((?:DEBUG|INFO|WARN|SEVERE).*$)", &err_msg));
    if (matcher == nullptr)
        logger->error("in SummarizeLog: failed to compile regex: " + err_msg);
    RegexMatcher * const datetime_matcher(RegexMatcher::RegexMatcherFactory(
        "^\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{]2}", &err_msg));
    if (matcher == nullptr)
        logger->error("in SummarizeLog: failed to compile datetime regex: " + err_msg);

    std::unordered_map<std::string, unsigned> lines_and_frequencies;
    std::string max_datetime("0000-00-00 00:00:00"), min_datetime("9999-99-99 99:99:99");
    while (not log_file->eof()) {
        std::string line;
        if (unlikely(log_file->getline(&line) == 0))
            continue;

        if (not matcher->matched(line)) {
            logger->warning("in SummarizeLog: failed to match line: " + line);
            continue;
        }

        if (datetime_matcher->matched(line)) {
            const std::string datetime(line.substr(0, 16));
            if (datetime > max_datetime)
                max_datetime = datetime;
            if (datetime < min_datetime)
                min_datetime = datetime;
        }

        const std::string summary((*matcher)[1]);
        auto line_and_count(lines_and_frequencies.find(summary));
        if (line_and_count == lines_and_frequencies.end())
            lines_and_frequencies[summary] = 1;
        else
            ++lines_and_frequencies[summary];
    }

    std::vector<std::pair<std::string, unsigned>> lines_and_frequencies_as_vector;
    lines_and_frequencies_as_vector.reserve(lines_and_frequencies.size());
    for (const auto &line_and_frequency : lines_and_frequencies)
        lines_and_frequencies_as_vector.emplace_back(std::make_pair<>(line_and_frequency.first,
                                                                      line_and_frequency.second));

    std::sort(lines_and_frequencies_as_vector.begin(), lines_and_frequencies_as_vector.end(),
              LineAndFrequencyCompare);

    *summary_file << "Summary of " << log_file->getPath() << " between " << min_datetime << " and "
                  << max_datetime << ".\n";
    for (const auto &line_and_frequency : lines_and_frequencies_as_vector)
        *summary_file << line_and_frequency.first << ": " << line_and_frequency.second << '\n';
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    std::unique_ptr<File> log_file(FileUtil::OpenInputFileOrDie(argv[1]));
    std::unique_ptr<File> summary_file(FileUtil::OpenOutputFileOrDie(argv[2]));

    try {
        SummarizeLog(log_file.get(), summary_file.get());
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
