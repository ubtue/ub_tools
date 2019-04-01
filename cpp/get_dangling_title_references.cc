/** \brief Detect missing references in title data records
 *  \author Johannes Riedl
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "MARC.h"
#include "util.h"

namespace {

static  const std::vector<MARC::Tag> REFERENCE_FIELDS{ MARC::Tag("775"), MARC::Tag("776"), MARC::Tag("780"),
                                                       MARC::Tag("785"), MARC::Tag("787") };

[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--patch-to-k10plus] marc_input dangling_log k10plus_concordance\n\n";
    std::exit(EXIT_FAILURE);
}

} // unnamed namespace


void CollectAllPPNs(MARC::Reader * const reader, std::unordered_set<std::string> * const all_ppns) {
   while (const auto record = reader->read())
      all_ppns->emplace(record.getControlNumber());
}

bool IsPartOfTitleData(const std::unordered_set<std::string> &all_ppns, const std::string &referenced_ppn) {
    return all_ppns.find(referenced_ppn) != all_ppns.cend();
}


void CheckCrossReferences(MARC::Reader * const reader, const std::unordered_set<std::string> &all_ppns,
                          std::unique_ptr<File> &dangling_log, bool patch_to_k10plus, std::unordered_map<std::string, std::string> &concordance_map) {
     int unreferenced_ppns(0);
     while (const auto record = reader->read()) {
        for (auto &field : record) {
            std::string referenced_ppn;
            if (MARC::IsCrossLinkField(field, &referenced_ppn, REFERENCE_FIELDS)) {
                if (not IsPartOfTitleData(all_ppns, referenced_ppn)) {
                    std::string ppn = record.getControlNumber();
                    if (patch_to_k10plus) {
                        auto new_ppn = concordance_map.find(ppn);
                        if (new_ppn == concordance_map.end())
                            LOG_ERROR("Could not find K10plus-PPN for PPN " + ppn);
                        auto new_referenced_ppn = concordance_map.find(referenced_ppn);
                        if (new_referenced_ppn == concordance_map.end()) {
                            LOG_WARNING("Could not find K10plus-PPN for referenced PPN " + referenced_ppn + " [PPN: " + ppn + "]");
                            *dangling_log << new_ppn->second << ", K10+ PPN DOES NOT EXIST FOR \"" << referenced_ppn + "\"\n";
                        }
                        *dangling_log << new_ppn->second << "," << new_referenced_ppn->second << '\n';
                    } else
                        *dangling_log << ppn << "," << referenced_ppn << '\n';

                    ++unreferenced_ppns;
                }
            }
        }
     }
     LOG_INFO("Detected " + std::to_string(unreferenced_ppns) + " unreferenced ppns");
}


void PopulateConcordanceMap(std::unique_ptr<File> &concordance_file, std::unordered_map<std::string, std::string> * const concordance_map) {
     unsigned int line_no(0);
     while (not concordance_file->eof()) {
         ++line_no;
         std::string line, old_val, new_val;
         concordance_file->getline(&line);
         if (not StringUtil::SplitOnString(line, " " , &old_val, &new_val))
             LOG_ERROR("Could not properly split line \"" + line + "\"");
         if (old_val.length() > 9 or new_val.length() > 10)
             LOG_ERROR("Invalid line " + std::to_string(line_no) + " in \"" + concordance_file->getPath() + "\"");
         (*concordance_map)[old_val] = new_val;
     }
     LOG_INFO("We read " + std::to_string(line_no) + " mappings from " + concordance_file->getPath());
}


int Main(int argc, char *argv[]) {
    bool patch_to_k10plus(false);

    if (argc != 3 and argc !=5)
        Usage();

    if (argc == 5) {
        if (std::strcmp(argv[1], "--patch-to-k10plus") != 0)
            Usage();
        patch_to_k10plus = true;
        ++argv;
        --argc;
    }

    auto marc_reader(MARC::Reader::Factory(argv[1]));

    std::unique_ptr<File> dangling_log;
    dangling_log = FileUtil::OpenOutputFileOrDie(argv[2]);

    std::unordered_map<std::string, std::string> swb_to_k10plus_map;
    if (patch_to_k10plus) {
        std::unique_ptr<File> swb_to_k10plus_file;
        swb_to_k10plus_file = FileUtil::OpenInputFileOrDie(argv[3]);
        PopulateConcordanceMap(swb_to_k10plus_file, &swb_to_k10plus_map);
    }

    std::unordered_set<std::string> all_ppns;
    CollectAllPPNs(marc_reader.get(), &all_ppns);
    marc_reader->rewind();
    CheckCrossReferences(marc_reader.get(), all_ppns, dangling_log, patch_to_k10plus, swb_to_k10plus_map);

    return EXIT_SUCCESS;
}
