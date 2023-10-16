/** \file marc_split.cc
 *  \brief Splits a MARC 21 file in equally sized files.
 *
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *
 *  \copyright 2016-2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <vector>
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "usage: " << ::progname << " marc_input marc_output_prefix [--target_record_count=N|--target_file_count=N]\n";
    std::cerr
        << "       - marc_output_prefix: prefix for output filenames (underscore _ and a consecutive number will be added afterwards).\n";
    std::cerr << "                             the file extension will be detected from the input file.\n";
    std::cerr << "       - record_count: Number of records per output file.\n";
    std::cerr << "       - target_file_count: Number of target files.\n";
    std::exit(EXIT_FAILURE);
}


std::string GenerateOutputFileName(const std::string &output_prefix, unsigned sequence_number, const std::string &output_extension) {
    return output_prefix + "_" + std::to_string(sequence_number) + "." + output_extension;
}


void SplitByFileCount(MARC::Reader * const marc_reader, const std::string &output_prefix, const unsigned target_file_count,
                      const std::string &output_extension) {
    std::vector<std::unique_ptr<MARC::Writer>> marc_writers;
    for (size_t i(0); i < target_file_count; ++i) {
        const std::string output_filename(GenerateOutputFileName(output_prefix, i, output_extension));
        marc_writers.emplace_back(MARC::Writer::Factory(output_filename));
    }

    unsigned index(0);
    while (const MARC::Record record = marc_reader->read()) {
        marc_writers[index % marc_writers.size()]->write(record);
        ++index;
    }
    std::cout << "~" << (index / marc_writers.size()) << " records per file.\n";
}


void SplitByRecordCount(MARC::Reader * const marc_reader, const std::string &output_prefix, const unsigned record_count,
                        const std::string &output_extension) {
    unsigned record_index(0);
    unsigned file_index(0);
    std::string output_filename;
    std::unique_ptr<MARC::Writer> writer = nullptr;
    while (const MARC::Record record = marc_reader->read()) {
        if (output_filename.empty() or (record_index % record_count == 0) or writer == nullptr) {
            ++file_index;
            output_filename = GenerateOutputFileName(output_prefix, file_index, output_extension);
            writer = MARC::Writer::Factory(output_filename);
        }
        writer->write(record);
        ++record_index;
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 4)
        Usage();

    const std::string input_file(argv[1]);
    const std::string output_prefix(argv[2]);
    const std::string output_extension(FileUtil::GetExtension(input_file));
    const std::string mode_params(argv[3]);

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(input_file));

    const std::string mode_prefix_target_record_count("--target_record_count=");
    const std::string mode_prefix_target_file_count("--target_file_count=");

    if (StringUtil::StartsWith(mode_params, mode_prefix_target_record_count)) {
        const int record_count(StringUtil::ToInt(mode_params.substr(mode_prefix_target_record_count.length())));
        SplitByRecordCount(marc_reader.get(), output_prefix, record_count, output_extension);
    } else if (StringUtil::StartsWith(mode_params, mode_prefix_target_file_count)) {
        const int target_file_count(StringUtil::ToInt(mode_params.substr(mode_prefix_target_file_count.length())));
        SplitByFileCount(marc_reader.get(), output_prefix, target_file_count, output_extension);
    } else
        LOG_ERROR("Unknown mode params: " + mode_params);


    return EXIT_SUCCESS;
}
