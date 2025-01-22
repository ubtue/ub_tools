/** \brief Stick to the new BSZ request to avoid Sonderdruck monographies for unlinked articles
 *
 *  \author Johannes Riedl
 *
 *  \copyright 2024 Universitätsbibliothek Tübingen.  All rights reserved.
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

static unsigned modified_count(0);

namespace {

[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [-v|--verbose] marc_input marc_output\n";
    std::exit(EXIT_FAILURE);
}


void ProcessRecord(const bool verbose, MARC::Record * const record) {
    if (not record->hasFieldWithSubfieldValue("935", 'c', "so"))
        return;

    record->setLeader("00000naa a22000002  4500");

    for (auto &_773field : record->getTagRange("773")) {
        if (not(_773field.getIndicator1() == '0') or not(_773field.getIndicator2() == '8'))
            continue;

        MARC::Subfields _773subfields(_773field.getSubfields());
        _773subfields.replaceFirstSubfield('i', "Enthalten in");
        _773field.setSubfields(_773subfields);
    }

    static ThreadSafeRegexMatcher so_matcher("so");
    record->deleteFieldWithSubfieldCodeMatching("935", 'c', so_matcher);

    if (verbose)
        LOG_INFO("Adjusted PPN " + record->getControlNumber());
    ++modified_count;
}


void RevertSonderdruckRecords(bool verbose, MARC::Reader * const marc_reader, MARC::Writer * const marc_writer) {
    unsigned record_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ProcessRecord(verbose, &record);
        marc_writer->write(record);
        ++record_count;
    }

    std::cout << "Modified " << modified_count << " of " << record_count << " record(s).\n";
}


} // unnamed namespace

int Main(int argc, char **argv) {
    if (argc < 3)
        Usage();

    const bool verbose(std::strcmp("-v", argv[1]) == 0 or std::strcmp("--verbose", argv[1]) == 0);
    if (verbose)
        --argc, ++argv;

    if (argc != 3)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto marc_writer(MARC::Writer::Factory(argv[2]));

    RevertSonderdruckRecords(verbose, marc_reader.get(), marc_writer.get());

    return EXIT_SUCCESS;
}
