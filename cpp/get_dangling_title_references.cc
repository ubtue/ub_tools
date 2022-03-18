/** \brief Detect missing references in title data records
 *  \author Johannes Riedl
 *
 *  \copyright 2019-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("[--consider-only-reviews] marc_input dangling_log");
}


void CollectAllPPNs(MARC::Reader * const reader, std::unordered_set<std::string> * const all_ppns) {
    while (const auto record = reader->read())
        all_ppns->emplace(record.getControlNumber());
}


inline bool IsPartOfTitleData(const std::unordered_set<std::string> &all_ppns, const std::string &referenced_ppn) {
    return all_ppns.find(referenced_ppn) != all_ppns.cend();
}


void FindDanglingCrossReferences(MARC::Reader * const reader, const bool consider_only_reviews,
                                 const std::unordered_set<std::string> &all_ppns, File * const dangling_log) {
    unsigned unreferenced_ppns(0);
    while (const auto record = reader->read()) {
        if (consider_only_reviews and not record.isReviewArticle())
            continue;

        for (const auto &field : record) {
            std::string referenced_ppn;
            if (MARC::IsCrossLinkField(field, &referenced_ppn, MARC::CROSS_LINK_FIELD_TAGS) and not IsPartOfTitleData(all_ppns, referenced_ppn)) {
                *dangling_log << record.getControlNumber() << "," << referenced_ppn << '\n';
                ++unreferenced_ppns;
            }
        }
    }
    LOG_INFO("Detected " + std::to_string(unreferenced_ppns) + " unreferenced ppns");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3 and argc != 4)
        Usage();

    bool consider_only_reviews(false);
    if (__builtin_strcmp(argv[1], "--consider-only-reviews") == 0) {
        consider_only_reviews = true;
        --argc, ++argv;
    }
    if (argc != 3)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    const auto dangling_log(FileUtil::OpenOutputFileOrDie(argv[2]));

    std::unordered_set<std::string> all_ppns;
    CollectAllPPNs(marc_reader.get(), &all_ppns);

    marc_reader->rewind();
    FindDanglingCrossReferences(marc_reader.get(), consider_only_reviews, all_ppns, dangling_log.get());

    return EXIT_SUCCESS;
}
