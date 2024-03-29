/** \brief Utility for monitoring our full-text database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <cstdlib>
#include "Compiler.h"
#include "DnsUtil.h"
#include "EmailSender.h"
#include "FileUtil.h"
#include "FullTextCache.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "stats_file_path email_address\n"
        "A report will be sent to \"email_address\".");
}


void LoadOldStats(const std::string &stats_file_path, std::vector<std::pair<std::string, unsigned>> * const domains_and_counts) {
    if (not FileUtil::Exists(stats_file_path)) // This should only be the case the first time we run this program!
        return;

    domains_and_counts->clear();
    std::unique_ptr<File> input(FileUtil::OpenInputFileOrDie(stats_file_path));
    unsigned line_no(0);
    while (not input->eof()) {
        std::string line;
        input->getline(&line);
        ++line_no;

        const std::string::size_type vertical_bar_pos(line.find('|'));
        if (vertical_bar_pos == std::string::npos)
            continue;

        const std::string domain(line.substr(0, vertical_bar_pos));
        unsigned count;
        if (unlikely(not StringUtil::ToUnsigned(line.substr(vertical_bar_pos + 1), &count)))
            logger->error("in LoadOldStats: line #" + std::to_string(line_no) + "in \"" + input->getPath() + "\" contains junk!");

        domains_and_counts->emplace_back(std::make_pair(domain, count));
    }
}


void DetermineNewStats(std::vector<std::pair<std::string, unsigned>> * const domains_and_counts) {
    domains_and_counts->clear();

    FullTextCache cache;
    std::unordered_map<std::string, unsigned> domains_to_counts_map;
    for (const auto &domain : cache.getDomains()) {
        const auto domain_and_count_iter(domains_to_counts_map.find(domain));
        if (domain_and_count_iter == domains_to_counts_map.end())
            domains_to_counts_map[domain] = 1;
        else
            ++domains_to_counts_map[domain];
    }

    for (const auto &domain_and_count : domains_to_counts_map)
        domains_and_counts->emplace_back(domain_and_count);
}


inline bool CompareDomainsAndCountsByDomains(const std::pair<std::string, unsigned> &domain_and_count1,
                                             const std::pair<std::string, unsigned> &domain_and_count2) {
    return domain_and_count1.first < domain_and_count2.first;
}


void CompareStatsAndGenerateReport(const std::string &email_address, std::vector<std::pair<std::string, unsigned>> old_domains_and_counts,
                                   std::vector<std::pair<std::string, unsigned>> new_domains_and_counts) {
    std::sort(old_domains_and_counts.begin(), old_domains_and_counts.end(), CompareDomainsAndCountsByDomains);
    std::sort(new_domains_and_counts.begin(), new_domains_and_counts.end(), CompareDomainsAndCountsByDomains);

    std::string report_text;
    bool found_one_or_more_problems(false);
    auto old_iter(old_domains_and_counts.cbegin());
    auto new_iter(new_domains_and_counts.cbegin());
    unsigned added_count(0), disappeared_count(0);

    while (old_iter != old_domains_and_counts.cend() or new_iter != new_domains_and_counts.cend()) {
        if (old_iter == old_domains_and_counts.cend()) {
            added_count += new_iter->second;
            report_text += new_iter->first + " (count: " + std::to_string(new_iter->second) + ") was added.\n";
            ++new_iter;
        } else if (new_iter == new_domains_and_counts.cend()) {
            disappeared_count += old_iter->second;
            report_text += old_iter->first + " (count: " + std::to_string(old_iter->second) + ") disappeared.\n";
            ++old_iter;
            found_one_or_more_problems = true;
        } else {
            if (old_iter->first == new_iter->first) {
                report_text += old_iter->first + ", old count: " + std::to_string(old_iter->second)
                               + ", new count: " + std::to_string(new_iter->second) + "\n";
                ++new_iter, ++old_iter;
            } else if (old_iter->first < new_iter->first) {
                disappeared_count += old_iter->second;
                report_text += old_iter->first + " (count: " + std::to_string(old_iter->second) + ") disappeared.\n";
                ++old_iter;
                found_one_or_more_problems = true;
            } else if (old_iter->first > new_iter->first) {
                added_count += new_iter->second;
                report_text += new_iter->first + " (count: " + std::to_string(new_iter->second) + ") was added.\n";
                ++new_iter;
            }
        }
    }

    report_text = "Overall " + std::to_string(added_count) + " new items were added and " + std::to_string(disappeared_count)
                  + " old items disappeared.\n\n" + report_text;

    const unsigned short response_code(
        EmailSender::SimplerSendEmail("no-reply@ub.uni-tuebingen.de", { email_address }, "Full Text Stats (" + DnsUtil::GetHostname() + ")",
                                      report_text, found_one_or_more_problems ? EmailSender::VERY_HIGH : EmailSender::VERY_LOW));
    if (response_code > 299)
        LOG_ERROR("failed to send email! (response code was " + std::to_string(response_code) + ")");
}


void WriteStats(const std::string &stats_filename, const std::vector<std::pair<std::string, unsigned>> &domains_and_counts) {
    std::string stats;
    for (const auto &domain_and_count : domains_and_counts)
        stats += domain_and_count.first + "|" + std::to_string(domain_and_count.second) + '\n';

    if (unlikely(not FileUtil::WriteString(stats_filename, stats)))
        logger->error("failed to write new stats to \"" + stats_filename + "\"!");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    std::vector<std::pair<std::string, unsigned>> old_domains_and_counts;
    LoadOldStats(argv[1], &old_domains_and_counts);

    std::vector<std::pair<std::string, unsigned>> new_domains_and_counts;
    DetermineNewStats(&new_domains_and_counts);

    CompareStatsAndGenerateReport(argv[2], old_domains_and_counts, new_domains_and_counts);
    WriteStats(argv[1], new_domains_and_counts);

    LOG_INFO("finished successfully");

    return EXIT_SUCCESS;
}
