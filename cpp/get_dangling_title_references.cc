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
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_verbosity] marc_input dangling_log\n\n";
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
                          std::unique_ptr<File> &dangling_log) {
     int unreferenced_ppns(0);
     while (const auto record = reader->read()) {
        for (auto &field : record) {
            std::string referenced_ppn;
            if (MARC::IsCrossLinkField(field, &referenced_ppn, REFERENCE_FIELDS)) {
                if (not IsPartOfTitleData(all_ppns, referenced_ppn)) {
                    *dangling_log << record.getControlNumber() << "," << referenced_ppn << '\n';
                    ++unreferenced_ppns;
                }
            }
        }
     }
     LOG_INFO("Detected " + std::to_string(unreferenced_ppns) + " unreferenced ppns");
}


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));

    std::unique_ptr<File> dangling_log;
    dangling_log = FileUtil::OpenOutputFileOrDie(argv[2]);

    std::unordered_set<std::string> all_ppns;
    CollectAllPPNs(marc_reader.get(), &all_ppns);
    marc_reader->rewind();
    CheckCrossReferences(marc_reader.get(), all_ppns, dangling_log);

    return EXIT_SUCCESS;
}
