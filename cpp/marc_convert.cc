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
#include <cstdio>
#include <cstdlib>
#include "MARC.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--quiet] marc_input marc_output [CTLN_1 CTLN_2 .. CTLN_N]\n"
              << "       Autoconverts the MARC format of \"marc_input\" to \"marc_output\".\n"
              << "       Supported extensions are \"xml\", \"mrc\", \"marc\" and \"raw\".\n"
              << "       All extensions except for \"xml\" are assumed to imply MARC-21.\n"
              << "       If a control number list has been specified only those records will\n"
              << "       be extracted or converted.\n\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecords(const bool quiet, MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
                    const std::set<std::string> &control_numbers)
{
    unsigned record_count(0), extracted_count(0);

    while (MARC::Record record = marc_reader->read()) {
        ++record_count;

        if (not control_numbers.empty() and control_numbers.find(record.getControlNumber()) == control_numbers.end())
            continue;

        ++extracted_count;
        marc_writer->write(record);
    }

    if (not quiet) {
        logger->info("Processed " + std::to_string(record_count) + " MARC record(s).");
        logger->info("Extracted or converted " + std::to_string(extracted_count) + " record(s).");
    }
}


enum MarcType { MARC21, MARC_XML, UNKNOWN };


MarcType GetMarcType(const std::string &filename) {
    if (StringUtil::EndsWith(filename, ".xml"))
        return MARC_XML;
    if (StringUtil::EndsWith(filename, ".mrc"))
        return MARC21;
    if (StringUtil::EndsWith(filename, ".marc"))
        return MARC21;
    if (StringUtil::EndsWith(filename, ".raw"))
        return MARC21;
    return UNKNOWN;
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

    const std::string input_filename(argv[1]);
    const MarcType input_type(GetMarcType(input_filename));
    if (input_type == UNKNOWN)
        logger->error("can't determine the MARC file type for \"" + input_filename + "\" based on its extension!");
    const MARC::Reader::ReaderType reader_type((input_type == MARC21) ? MARC::Reader::BINARY : MARC::Reader::XML);

    const std::string output_filename(argv[2]);
    const MarcType output_type(GetMarcType(output_filename));
    if (output_type == UNKNOWN)
        logger->error("can't determine the MARC file type for \"" + output_filename + "\" based on its extension!");
    const MARC::Writer::WriterType writer_type((output_type == MARC21) ? MARC::Writer::BINARY : MARC::Writer::XML);

    try {
        std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(input_filename, reader_type));
        std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(output_filename, writer_type));

        std::set<std::string> control_numbers;
        for (int arg_no(3); arg_no < argc; ++arg_no)
            control_numbers.emplace(argv[arg_no]);

        ProcessRecords(quiet, marc_reader.get(), marc_writer.get(), control_numbers);
    } catch (const std::exception &e) {
        LOG_ERROR("Caught exception: " + std::string(e.what()));
    }
}
