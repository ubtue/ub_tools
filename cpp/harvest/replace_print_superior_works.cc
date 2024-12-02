/** \brief Replace the PPN of the superior work for print records
 *         to 787.
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


using OnlineToPrintPPNMap = std::map<std::string, std::string>;

static unsigned modified_count(0);

namespace {

[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [-v|--verbose] marc_input ppn_mapping_file marc_output\n";
    std::exit(EXIT_FAILURE);
}


void SetupOnlineToPrintPPNMap(const bool verbose, File * const mapping_file, OnlineToPrintPPNMap * const online_to_print) {
    while (not mapping_file->eof()) {
        std::string line(mapping_file->getline());
        std::vector<std::string> online_and_print;
        StringUtil::SplitThenTrimWhite(line, ":", &online_and_print);
        if (online_and_print.size() < 2) {
            if (verbose)
                LOG_WARNING("Skipping incomplete line: \"" + line + "\"");
        } else if (online_and_print.size() == 2)
            online_to_print->insert(std::make_pair(online_and_print[0], online_and_print[1]));
        else
            LOG_ERROR("Invalid line [1]: \"" + line + "\"");
    }
}


void ProcessRecord(const bool verbose, MARC::Record * const record, const OnlineToPrintPPNMap &online_to_print) {
    if (not record->isPrintResource())
        return;

    for (auto &_773field : record->getTagRange("773")) {
        if (not(_773field.getIndicator1() == '0') or not(_773field.getIndicator2() == '8'))
            continue;

        std::string online_superior_ppn(record->getFirstSubfieldValue("773", 'w'));
        if (online_superior_ppn.empty())
            return;

        online_superior_ppn = online_superior_ppn.substr(__builtin_strlen("(DE-627)"));
        if (not online_to_print.contains(online_superior_ppn)) {
            // Remove online superior PPN anyway
            _773field.deleteAllSubfieldsWithCode('w');
            if (verbose)
                LOG_INFO("Removed 773w with PPN " + online_superior_ppn + " as no print PPN present");
            ++modified_count;
            return;
        }

        MARC::Subfields _773subfields(_773field.getSubfields());
        const auto print_superior_ppn(online_to_print.at(online_superior_ppn));
        _773subfields.replaceFirstSubfield('w', "(DE-627)" + print_superior_ppn);
        _773field.setSubfields(_773subfields);
        // Remove now invalid ISSN
        _773field.deleteAllSubfieldsWithCode('x');

        if (verbose)
            LOG_INFO("Mapped " + online_superior_ppn + " to " + print_superior_ppn);
        ++modified_count;
    }
}


void ReplaceSuperiorPPN(bool verbose, MARC::Reader * const marc_reader, const OnlineToPrintPPNMap &online_to_print,
                        MARC::Writer * const marc_writer) {
    unsigned record_count(0);
    while (MARC::Record record = marc_reader->read()) {
        ProcessRecord(verbose, &record, online_to_print);
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

    if (argc < 3)
        Usage();

    if (argc != 4)
        Usage();

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto ppn_mapping_file(FileUtil::OpenInputFileOrDie(argv[2]));
    auto marc_writer(MARC::Writer::Factory(argv[3]));

    OnlineToPrintPPNMap online_to_print;
    SetupOnlineToPrintPPNMap(verbose, ppn_mapping_file.get(), &online_to_print);
    ReplaceSuperiorPPN(verbose, marc_reader.get(), online_to_print, marc_writer.get());

    return EXIT_SUCCESS;
}
