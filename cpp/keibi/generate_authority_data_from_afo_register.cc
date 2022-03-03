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
    

[[noreturn]] void Usage() {
    ::Usage("afo_register_csv_file marc_output");
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
    bool operator==(const AfOEntry &rhs) const { return keyword_ == rhs.keyword_; }
};

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
    TextUtil::ParseCSVFileOrDie(afo_file_path, &lines, '\t');
    unsigned linenum(0);
    for (const auto &line : lines) {
       ++linenum;
       std::cerr << "NUMBER: " << line[1] << '\n';
       if (not StringUtil::IsUnsignedNumber(line[0])) {
           LOG_WARNING("Invalid content in line " + std::to_string(linenum) + "(" + StringUtil::Join(line, '\t') + ")");
           continue;
       }

       AfOEntry afo_entry(std::stoi(line[0]), line[1], line[2], line[3], line[4]);
       afo_multi_set->emplace(afo_entry);
    }
}

void CleanCSVAndWriteToTempFile(const std::string &afo_file_path, FileUtil::AutoTempFile * const tmp_file) {
    std::unique_ptr<File> afo_tmp_file(FileUtil::OpenOutputFileOrDie(tmp_file->getFilePath()));
    for (const auto line : FileUtil::ReadLines(afo_file_path)) {
        (*(afo_tmp_file.get())) << StringUtil::RightTrim(line, '\t') << '\n';
    }
}

} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();
    const std::string afo_file_path(argv[1]);
    const std::string marc_output_path(argv[2]);

    FileUtil::AutoTempFile tmp_file;
    CleanCSVAndWriteToTempFile(afo_file_path, &tmp_file);

    const std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_path));
    AfOMultiSet afo_multi_set;
    GenerateAfOSet(tmp_file.getFilePath(), &afo_multi_set);
    (void) marc_writer;

    return EXIT_SUCCESS;
}

