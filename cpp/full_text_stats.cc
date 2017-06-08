/** \brief Utility for monitoring our full-text database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "DbConnection.h"
#include "EmailSender.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"
#include "VuFind.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " stats_file_path email_address\n"
              << "       A report will be sent to \"email_address\".\n\n";
    std::exit(EXIT_FAILURE);
}


void LoadOldStats(const std::string &stats_file_path,
                  std::vector<std::pair<std::string, unsigned>> * const domains_and_counts)
{
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
        if (unlikely(not StringUtil::ToUnsigned(line.substr(vertical_bar_pos + 1))))
            Error("in LoadOldStats: line #" + std::to_string(line_no) + "in \"" + input->getPath()
                  + "\" contains junk!");

        domains_and_counts->emplace_back(std::make_pair(domain, count));
    }
}


// Here we assume we only deal with HTTP and HTTPS URL's.
std::string GetHost(const std::string &url) {
    if (unlikely(url.length() < 9))
        Error("in GetHost: we don't know how to deal with this \"URL\": \"" + url + "\"!");

    const std::string::size_type first_colon_pos(url.find(':', 9));
    if (first_colon_pos != std::string::npos)
        return url.substr(0, first_colon_pos);

    const std::string::size_type first_slash_pos(url.find('/', 9));
    if (first_slash_pos != std::string::npos)
        return url.substr(0, first_slash_pos);

    return url;
}


void DetermineNewStats(std::vector<std::pair<std::string, unsigned>> * const domains_and_counts) {
    domains_and_counts->clear();
    
    std::string mysql_url;
    VuFind::GetMysqlURL(&mysql_url);
    DbConnection db_connection(mysql_url);

    const std::string SELECT_STMT("SELECT url FROM full_text_cache");
    if (not db_connection.query(SELECT_STMT))
        throw std::runtime_error("Query \"" + SELECT_STMT + "\" failed because: "
                                 + db_connection.getLastErrorMessage());
    DbResultSet result_set(db_connection.getLastResultSet());

    std::unordered_map<std::string, unsigned> domains_to_counts_map;
    while (const DbRow row = result_set.getNextRow()) {
        const std::string domain(GetHost(row["url"]));

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
                                             const std::pair<std::string, unsigned> &domain_and_count2)
{
    return domain_and_count1.first > domain_and_count2.first;
}


void CompareStatsAndGenerateReport(const std::string &email_address,
                                   std::vector<std::pair<std::string, unsigned>> old_domains_and_counts,
                                   std::vector<std::pair<std::string, unsigned>> new_domains_and_counts)
{
    std::sort(old_domains_and_counts.begin(), old_domains_and_counts.end(), CompareDomainsAndCountsByDomains);
    std::sort(new_domains_and_counts.begin(), new_domains_and_counts.end(), CompareDomainsAndCountsByDomains);

    std::string report_text;
    bool found_one_or_more_problems(false);
    auto old_iter(old_domains_and_counts.cbegin());
    auto new_iter(new_domains_and_counts.cbegin());

    while (old_iter != old_domains_and_counts.cend() or new_iter != new_domains_and_counts.cend()) {
        if (old_iter == old_domains_and_counts.cend()) {
            report_text += new_iter->first + " (count: " + std::to_string(new_iter->second) + ") was added.\n";
            ++new_iter;
        } else if (new_iter == new_domains_and_counts.cend()) {
            report_text += old_iter->first + " (count: " + std::to_string(old_iter->second) + ") disappeared.\n";
            ++old_iter;
            found_one_or_more_problems = true;
        } else {
            if (old_iter->first == new_iter->first) {
                report_text += old_iter->first + ", old count: " + std::to_string(old_iter->second) + ", new count: "
                               + std::to_string(old_iter->second) + "\n"; 
                ++new_iter, ++old_iter;
            } else if (old_iter->first < new_iter->first) {
                report_text += old_iter->first + " (count: " + std::to_string(old_iter->second) + ") disappeared.\n";
                ++old_iter;
                found_one_or_more_problems = true;
            } else if (old_iter->first > new_iter->first) {
                report_text += new_iter->first + " (count: " + std::to_string(new_iter->second) + ") was added.\n";
                ++new_iter;
            }
        }
    }
    
    EmailSender::SendEmail("no_return@ub.uni-tuebingen.de", email_address, "Full Text Stats", report_text,
                           found_one_or_more_problems ? EmailSender::VERY_HIGH : EmailSender::VERY_LOW);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    try {
        std::vector<std::pair<std::string, unsigned>> old_domains_and_counts;
        LoadOldStats(argv[1], &old_domains_and_counts);
        
        std::vector<std::pair<std::string, unsigned>> new_domains_and_counts;
        DetermineNewStats(&new_domains_and_counts);

        CompareStatsAndGenerateReport(argv[2], old_domains_and_counts, new_domains_and_counts);
    } catch (const std::exception &e) {
        Error("caught exception: " + std::string(e.what()));
    }
}
