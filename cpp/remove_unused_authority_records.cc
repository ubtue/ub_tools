/** \brief Utility for removing unreferenced authority records
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " title_data authority_data filtered_authority_data\n";
    std::exit(EXIT_FAILURE);
}


void CollectGNDReferences(MARC::Reader * const marc_reader, std::unordered_set<std::string> * const gnd_numbers) {
    std::string err_msg;
    RegexMatcher * const matcher(
        RegexMatcher::RegexMatcherFactory("\x1F"
                                          "0\\(DE-588\\)([^\x1F]+).*\x1F"
                                          "2gnd",
                                          &err_msg));
    if (matcher == nullptr)
        logger->error("failed to compile a regex in CollectGNDReferences: " + err_msg);

    unsigned record_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        for (const auto &field : record) {
            if (matcher->matched(field.getContents()))
                gnd_numbers->emplace((*matcher)[1]);
        }
    }

    std::cout << "Extracted " << gnd_numbers->size() << " GND number(s) from " << record_count << " title record(s).\n";
}


std::string GetGNDNumber(const MARC::Record &record) {
    for (const auto &_035_field : record.getTagRange("035")) {
        const MARC::Subfields _035_subfields(_035_field.getSubfields());
        const std::string _035a_contents(_035_subfields.getFirstSubfieldWithCode('a'));
        if (StringUtil::StartsWith(_035a_contents, "(DE-588)"))
            return _035a_contents.substr(__builtin_strlen("(DE-588)"));
    }

    return "";
}


const std::string DROPPED_GND_LIST_FILE("/usr/local/var/log/tuefind/dropped_gnd_numbers.list");


void FilterAuthorityData(MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                         const std::unordered_set<std::string> &gnd_numbers) {
    std::unique_ptr<File> gnd_list_file(FileUtil::OpenOutputFileOrDie(DROPPED_GND_LIST_FILE));
    unsigned record_count(0), dropped_count(0), authority_records_without_gnd_numbers_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        const std::string gnd_number(GetGNDNumber(record));
        if (not gnd_number.empty() and gnd_numbers.find(gnd_number) == gnd_numbers.cend()) {
            gnd_list_file->writeln(gnd_number);
            ++dropped_count;
            continue;
        }

        if (gnd_number.empty())
            ++authority_records_without_gnd_numbers_count;
        marc_writer->write(record);
    }

    std::cerr << "Read " << record_count << " authority record(s) of which " << dropped_count << " were dropped.\n";
    std::cerr << "Found and kept " << authority_records_without_gnd_numbers_count << " authority records w/o a GND number.\n";
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    std::unique_ptr<MARC::Reader> marc_title_reader(MARC::Reader::Factory(argv[1]));
    std::unique_ptr<MARC::Reader> marc_authority_reader(MARC::Reader::Factory(argv[2]));
    std::unique_ptr<MARC::Writer> marc_authority_writer(MARC::Writer::Factory(argv[3]));

    std::unordered_set<std::string> gnd_numbers;
    CollectGNDReferences(marc_title_reader.get(), &gnd_numbers);
    FilterAuthorityData(marc_authority_reader.get(), marc_authority_writer.get(), gnd_numbers);

    return EXIT_SUCCESS;
}
