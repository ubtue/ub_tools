/** \brief Utility for converting between MARC formats.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <set>
#include <stdexcept>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--quiet] [--limit max_no_of_records] [--output-individual-files] marc_input marc_output [CTLN_1 CTLN_2 .. CTLN_N]\n"
              << "       Autoconverts the MARC format of \"marc_input\" to \"marc_output\".\n"
              << "       Supported extensions are \"xml\", \"mrc\", \"marc\" and \"raw\".\n"
              << "       All extensions except for \"xml\" are assumed to imply MARC-21.\n"
              << "       If a control number list has been specified only those records will\n"
              << "       be extracted or converted.\n"
              << "       If --output-individual-files is specified marc_output must be a writable directory\n"
              << "       and files are named from the control numbers and written as XML\n\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecords(const bool quiet, const bool output_individual_files, const unsigned max_no_of_records,
                    MARC::Reader * const marc_reader, const std::string output_filename_or_directory,
                    const std::set<std::string> &control_numbers) {
    unsigned record_count(0), extracted_count(0);

    std::unique_ptr<MARC::Writer> marc_writer;
    if (output_individual_files)
        FileUtil::ChangeDirectoryOrDie(output_filename_or_directory);
    else
        marc_writer = MARC::Writer::Factory(output_filename_or_directory);

    while (MARC::Record record = marc_reader->read()) {
        ++record_count;

        if (not control_numbers.empty() and control_numbers.find(record.getControlNumber()) == control_numbers.end())
            continue;

        ++extracted_count;

        // Open a new XML file with the name derived from the control number for each record
        if (output_individual_files)
            marc_writer = MARC::Writer::Factory(record.getControlNumber() + ".xml", MARC::FileType::XML);

        marc_writer->write(record);

        if (record_count == max_no_of_records)
            break;
    }

    if (not quiet) {
        logger->info("Processed " + std::to_string(record_count) + " MARC record(s).");
        logger->info("Extracted or converted " + std::to_string(extracted_count) + " record(s).");
    }
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 3)
        Usage();

    bool quiet(false);
    if (std::strcmp(argv[1], "--quiet") == 0) {
        quiet = true;
        --argc, ++argv;
    }

    if (argc < 3)
        Usage();

    unsigned max_no_of_records(UINT_MAX);
    if (std::strcmp(argv[1], "--limit") == 0) {
        if (not StringUtil::ToUnsigned(argv[2], &max_no_of_records) or max_no_of_records == 0)
            Usage();
        argc -= 2;
        argv += 2;
    }

    bool output_individual_files(false);
    if (std::strcmp(argv[1], "--output-individual-files") == 0) {
        output_individual_files = true;
        --argc, ++argv;
    }

    if (argc < 3)
        Usage();

    const std::string input_filename(argv[1]);
    const std::string output_file_or_directory(argv[2]);

    try {
        std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(input_filename));

        std::set<std::string> control_numbers;
        for (int arg_no(3); arg_no < argc; ++arg_no)
            control_numbers.emplace(argv[arg_no]);

        ProcessRecords(quiet, output_individual_files, max_no_of_records, marc_reader.get(), output_file_or_directory, control_numbers);
    } catch (const std::exception &e) {
        LOG_ERROR("Caught exception: " + std::string(e.what()));
    }
}
