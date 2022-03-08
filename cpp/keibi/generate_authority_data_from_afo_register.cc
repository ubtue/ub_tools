/** \brief Convert Afo Register Entries to authority data
 *  \author Johannes Riedl
 *
 *  \copyright 2022 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <string>
#include <unordered_set>
#include "MARC.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"

namespace {

const unsigned ROWS_IN_CSV(5);


[[noreturn]] void Usage() {
    ::Usage("afo_register_csv_file1 [... afo_register_csv_fileN ] marc_output");
}


struct AfOEntry {
    unsigned entry_num_;
    std::string keyword_;
    std::string internal_reference_keyword_;
    std::string literature_reference_;
    std::string comment_;

    AfOEntry(const unsigned &entry_num, const std::string &keyword,
             const std::string &internal_reference_keyword,
             const std::string &literature_reference, const std::string &comment) :
             entry_num_(entry_num), keyword_(keyword),
             internal_reference_keyword_(internal_reference_keyword),
             literature_reference_(literature_reference),
             comment_(comment) {}
    AfOEntry(const std::string keyword) : AfOEntry(0, keyword, "", "", "") {}
    bool operator==(const AfOEntry &rhs) const { return keyword_ == rhs.keyword_; }
    std::string toString() const { return std::to_string(entry_num_) + " AAA " + keyword_  + " BBB " + internal_reference_keyword_
                                    + " CCC " + literature_reference_ +  " DDD "  + comment_;
    }
    friend std::ostream &operator<<(std::ostream &output, const AfOEntry &entry);
};


std::ostream &operator<<(std::ostream &output, const AfOEntry &entry) {
    output << entry.toString();
    return output;
}

} // unamed namespace

namespace std {

template <> struct hash<AfOEntry> {
    inline size_t operator()(const AfOEntry &afo_entry) const {
        return hash<std::string>()(afo_entry.keyword_);
    }
};

} // namespace std

namespace {


using AfOMultiSet = std::unordered_multiset<AfOEntry>;


void GenerateAfOSet(const std::string &afo_file_path, AfOMultiSet * const afo_multi_set) {
    std::vector<std::vector<std::string>> lines;
    TextUtil::ParseCSVFileOrDie(afo_file_path, &lines, '\t', '\0');
    unsigned linenum(0);
    for (auto &line : lines) {
       ++linenum;
       if (not StringUtil::IsUnsignedNumber(line[0])) {
           LOG_WARNING("Invalid content in line " + std::to_string(linenum) + "(" + StringUtil::Join(line, '\t') + ")");
           continue;
       }

       // Add missing columns
       for (auto i = line.size(); i < ROWS_IN_CSV; ++i)
           line.push_back("");
       AfOEntry afo_entry(std::stoi(line[0]), line[1], line[2], line[3], line[4]);
       afo_multi_set->emplace(afo_entry);
    }
}

void CleanCSVAndWriteToTempFile(const std::string &afo_file_path, FileUtil::AutoTempFile * const tmp_file) {
    std::unique_ptr<File> afo_tmp_file(FileUtil::OpenOutputFileOrDie(tmp_file->getFilePath()));
    for (auto line : FileUtil::ReadLines(afo_file_path)) {
        if (line.empty() or StringUtil::IsWhitespace(line))
            continue;
        StringUtil::RemoveTrailingLineEnd(&line);
        line = StringUtil::RightTrim(line, '\t');
        //line = StringUtil::EscapeDoubleQuotes(line);
        std::cerr << "LINE : \'" << line << "\'\n";
        (*(afo_tmp_file.get())) << line << '\n';
    }
}

} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    const std::string marc_output_path(argv[argc - 1]);
    std::vector<std::string> afo_file_paths;
    for (int arg_index = 1; arg_index < argc - 1; ++arg_index)
         afo_file_paths.emplace_back(argv[arg_index]);

    AfOMultiSet afo_multi_set;
    for (const auto afo_file_path : afo_file_paths) {
        FileUtil::AutoTempFile tmp_file;
        CleanCSVAndWriteToTempFile(afo_file_path, &tmp_file);
        GenerateAfOSet(tmp_file.getFilePath(), &afo_multi_set);
    }

    const auto afo_entries(afo_multi_set.equal_range(AfOEntry("Kunst")));
    for (auto entry(afo_entries.first); entry != afo_entries.second; ++entry)
        std::cout << *entry << '\n';

    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_path));
    (void) marc_writer;

    return EXIT_SUCCESS;
}

