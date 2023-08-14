/** \brief Replace RGG4 titles by scraped titles from the website
 *
 *  \copyright 2023 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include "FileUtil.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "util.h"


namespace {
[[noreturn]] void Usage() {
    ::Usage("orig_titles.txt web_titles.txt output.txt");
}


void ReadTitles(File * const titles_file, std::multiset<std::string> * const titles) {
    while (not titles_file->eof()) {
        std::string line;
        titles_file->getline(&line);
        titles->emplace(line);
    }
}

} // end unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    const std::string orig_titles_path(argv[1]);
    const std::string web_titles_path(argv[2]);
    const std::string output_path(argv[3]);

    const std::unique_ptr<File> orig_titles_file(FileUtil::OpenInputFileOrDie(orig_titles_path));
    const std::unique_ptr<File> web_titles_file(FileUtil::OpenInputFileOrDie(web_titles_path));
    std::ofstream output_file(output_path);

    std::multiset<std::string> orig_titles;
    std::multiset<std::string> web_titles;
    ReadTitles(orig_titles_file.get(), &orig_titles);
    ReadTitles(web_titles_file.get(), &web_titles);

    std::map<std::string, std::vector<std::string>> multiple_candidates;
    const std::string duplicated_person_name("([^\\s]+)\\s+\\1,.*");
    static ThreadSafeRegexMatcher name_matcher(duplicated_person_name);

    for (const auto &web_title : web_titles) {
        std::vector<std::string> matches;
        std::copy_if(orig_titles.begin(), orig_titles.end(), std::back_inserter(matches), [&web_title](const std::string &orig_title) {
            // Skip matches that are not entire words and do not include patterns like "last_name last_name, first_name", as
            // they will be treated differently
            return orig_title == web_title
                       ? true
                       : (orig_title.starts_with(std::string_view(web_title + ' ')) and not name_matcher.match(orig_title));
        });
        if (matches.size() == 0) {
            output_file << " ||| " << web_title << '\n';
        } else if (matches.size() == 1) {
            output_file << matches[0] << " | " << web_title << '\n';
            orig_titles.erase(matches[0]);
        } else
            multiple_candidates.emplace(web_title, matches);
    }

    for (const auto &entry : multiple_candidates) {
        output_file << entry.first << ":\n";
        for (const auto &candidate : entry.second) {
            output_file << '\t' << candidate << '\n';
            orig_titles.erase(candidate);
        }
    }

    for (const auto &unmatched : orig_titles) {
        output_file << unmatched << " |||| \n";
    }
    return EXIT_SUCCESS;
}
