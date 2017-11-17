/** \brief Utility for removing unreferenced authority records
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "Leader.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace {


void Usage() __attribute__((noreturn));


void Usage() {
    std::cerr << "Usage: " << ::progname << " title_data authority_data filtered_authority_data\n";
    std::exit(EXIT_FAILURE);
}


void CollectGNDReferences(MarcReader * const marc_reader, std::unordered_set<std::string> * const gnd_numbers) {
    std::string err_msg;
    RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("\x1F""0\\(DE-588\\)([^\x1F]+).*\x1F""2gnd",
                                                                   &err_msg));
    if (matcher == nullptr)
        logger->error("failed to compile a regex in CollectGNDReferences: " + err_msg);

    unsigned record_count(0);
    while (const MarcRecord record = marc_reader->read()) {
        ++record_count;

        for (const auto field_contents : record) {
            if (matcher->matched(field_contents))
                gnd_numbers->emplace((*matcher)[1]);
        }
    }

    std::cout << "Extracted " << gnd_numbers->size() << " GND number(s) from " <<  record_count
              << " title record(s).\n";
}


std::string GetGNDNumber(const MarcRecord &record) {
    std::vector<size_t> _035_indices;
    record.getFieldIndices("035", &_035_indices);

    for (const auto index : _035_indices) {
        const Subfields _035_subfields(record.getFieldData(index));
        const std::string _035a_contents(_035_subfields.getFirstSubfieldValue('a'));
        if (StringUtil::StartsWith(_035a_contents, "(DE-588)"))
            return _035a_contents.substr(__builtin_strlen("(DE-588)"));
    }

    return "";
}


const std::string DROPPED_GND_LIST_FILE("/usr/local/var/log/tuefind/dropped_gnd_numbers.list");


void FilterAuthorityData(MarcReader * const marc_reader, MarcWriter * const marc_writer,
                         const std::unordered_set<std::string> &gnd_numbers)
{
    std::unique_ptr<File> gnd_list_file(FileUtil::OpenOutputFileOrDie(DROPPED_GND_LIST_FILE));
    unsigned record_count(0), dropped_count(0), authority_records_without_gnd_numbers_count(0);
    while (const MarcRecord record = marc_reader->read()) {
        ++record_count;

        const std::string gnd_number(GetGNDNumber(record));
        if (not gnd_number.empty() and gnd_numbers.find(gnd_number) == gnd_numbers.cend()) {
            gnd_list_file->write(gnd_number + "\n");
            ++dropped_count;
            continue;
        }

        if (gnd_number.empty())
            ++authority_records_without_gnd_numbers_count;
        marc_writer->write(record);
    }

    std::cerr << "Read " << record_count << " authority record(s) of which " << dropped_count << " were dropped.\n";
    std::cerr << "Found and kept " << authority_records_without_gnd_numbers_count
              << " authority records w/o a GND number.\n";
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    try {
        std::unique_ptr<MarcReader> marc_title_reader(MarcReader::Factory(argv[1]));
        std::unique_ptr<MarcReader> marc_authority_reader(MarcReader::Factory(argv[2]));
        std::unique_ptr<MarcWriter> marc_authority_writer(MarcWriter::Factory(argv[3]));

        std::unordered_set<std::string> gnd_numbers;
        CollectGNDReferences(marc_title_reader.get(), &gnd_numbers);
        FilterAuthorityData(marc_authority_reader.get(), marc_authority_writer.get(), gnd_numbers);
    } catch (const std::exception &e) {
        logger->error("Caught exception: " + std::string(e.what()));
    }
}
