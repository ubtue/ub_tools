/** \file    control_number_filter.cc
 *  \brief   A tool for filtering MARC-21 data sets based on patterns for control numbers.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015-2017, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <memory>
#include <cstdlib>
#include <cstring>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "RegexMatcher.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << "(--keep|--delete)] pattern marc_input marc_output\n";
    std::cerr << "  Removes records whose control numbers match \"pattern\" if \"--delete\" has been specified\n";
    std::cerr << "  or only keeps those records whose control numbers match \"pattern\" if \"--keep\" has\n";
    std::cerr << "  been specified.  (\"pattern\" must be a PCRE.)\n";
    std::exit(EXIT_FAILURE);
}


void FilterMarcRecords(const bool keep, const std::string &regex_pattern, MarcReader * const marc_reader,
                       MarcWriter * const marc_writer)
{
    std::string err_msg;
    const RegexMatcher *matcher(RegexMatcher::RegexMatcherFactory(regex_pattern, &err_msg));
    if (matcher == nullptr)
        logger->error("Failed to compile pattern \"" + regex_pattern + "\": " + err_msg);

    unsigned count(0), kept_or_deleted_count(0);

    while (MarcRecord record = marc_reader->read()) {
        ++count;

        const bool matched(matcher->matched(record.getControlNumber(), &err_msg));
        if (not err_msg.empty())
            logger->error("regex matching error: " + err_msg);

        if ((keep and matched) or (not keep and not matched)) {
            ++kept_or_deleted_count;
            marc_writer->write(record);
        }
    }

    if (not err_msg.empty())
        logger->error(err_msg);

    std::cerr << "Read " << count << " records.\n";
    std::cerr << (keep ? "Kept " : "Deleted ") << kept_or_deleted_count << " record(s).\n";
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 5)
        Usage();
    if (std::strcmp(argv[1], "--keep") != 0 and std::strcmp(argv[1], "--delete") != 0)
        Usage();
    const bool keep(std::strcmp(argv[1], "--keep") == 0);
    const std::string regex_pattern(argv[2]);

    const std::string marc_input_filename(argv[3]);
    const std::string marc_output_filename(argv[4]);
    if (unlikely(marc_input_filename == marc_output_filename))
        logger->error("Master input file name equals output file name!");

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(marc_input_filename, MarcReader::BINARY));
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename, MarcWriter::BINARY));
    FilterMarcRecords(keep, regex_pattern, marc_reader.get(), marc_writer.get());
}
