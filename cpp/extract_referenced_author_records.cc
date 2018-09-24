/** \file    extract_referenced_author_records.cc
 *  \brief   Selects referenced author records from a collection of authority records.
 *  \author  Dr. Johannes Ruscheinski
 *
 *  Copyright (C) 2018, Library of the University of Tübingen
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
#include <unordered_set>
#include <cstdlib>
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " title_records authority_records referenced_author_records\n";
    std::exit(EXIT_FAILURE);
}


void ExtractAuthorPPN(const MARC::Record &record, const std::string &tag, std::unordered_set<std::string> * const referenced_author_ppns) {
    for (const auto &field : record.getTagRange(tag)) {
        for (const auto &subfield : field.getSubfields()) {
            if (subfield.code_ == '0' and StringUtil::StartsWith(subfield.value_, "(DE-576)"))
                referenced_author_ppns->emplace(subfield.value_.substr(__builtin_strlen("(DE-576)")));
        }
    }
}


void CollectAuthorPPNs(MARC::Reader * const title_reader, std::unordered_set<std::string> * const referenced_author_ppns) {
    while (const auto record = title_reader->read()) {
        ExtractAuthorPPN(record, "100", referenced_author_ppns);
        ExtractAuthorPPN(record, "400", referenced_author_ppns);
    }

    LOG_INFO("extracted " + std::to_string(referenced_author_ppns->size()) + " referenced author PPN's.");
}


void FilterAuthorityRecords(MARC::Reader * const authority_reader, MARC::Writer * const authority_writer,
                            const std::unordered_set<std::string> &referenced_author_ppns)
{
    unsigned count(0);
    while (const auto record = authority_reader->read()) {
        if (referenced_author_ppns.find(record.getControlNumber()) != referenced_author_ppns.cend()) {
            authority_writer->write(record);
            ++count;
        }
    }

    LOG_INFO("identified " + std::to_string(count) + " referenced author records.");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 4)
        Usage();

    const std::string title_records_filename(argv[1]);
    const std::string authority_records_filename(argv[2]);
    const std::string referenced_author_records_filename(argv[3]);

    if (unlikely(title_records_filename == referenced_author_records_filename))
        LOG_ERROR("Title input file name equals authority output file name!");
    if (unlikely(authority_records_filename == referenced_author_records_filename))
        LOG_ERROR("Authority data input file name equals authority output file name!");

    auto title_reader(MARC::Reader::Factory(title_records_filename));
    auto authority_reader(MARC::Reader::Factory(authority_records_filename));
    auto authority_writer(MARC::Writer::Factory(referenced_author_records_filename));

    try {
        std::unordered_set<std::string> referenced_author_ppns;
        CollectAuthorPPNs(title_reader.get(), &referenced_author_ppns);
        FilterAuthorityRecords(authority_reader.get(), authority_writer.get(), referenced_author_ppns);
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }

    return EXIT_SUCCESS;
}
