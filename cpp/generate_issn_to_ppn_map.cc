/** \brief Utility for merging print and online editions into single records.
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
#include <cstdlib>
#include "Compiler.h"
#include "FileUtil.h"
#include "MARC.h"
#include "MiscUtil.h"
#include "util.h"
#include "Zotero.h"


namespace {


const std::string DEFAULT_OUTPUT_FILENAME("/usr/local/var/lib/tuelib/issn_to_ppn.map");


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbosity=min_log_level] marc_input [issn_to_ppn_map]\n"
              << "       If you omit the output filename, \"" << Zotero::ISSN_TO_PPN_MAP_PATH << "\" will be used.\n\n";
    std::exit(EXIT_FAILURE);
}


const std::vector<std::string> ISSN_SUBFIELDS{ "022a", "029a", "440x", "490x", "730x", "773x", "776x", "780x", "785x" };


void PopulateISSNtoControlNumberMapFile(MARC::Reader * const marc_reader, File * const output) {
    unsigned total_count(0), written_count(0), malformed_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        ++total_count;

        if (not record.isSerial())
            continue;

        for (const std::string &issn_subfield : ISSN_SUBFIELDS) {
            for (const auto &field : record.getTagRange(issn_subfield.substr(0, MARC::Record::TAG_LENGTH))) {
                const MARC::Subfields subfields(field.getSubfields());
                for (const auto &subfield_value : subfields.extractSubfields(issn_subfield[MARC::Record::TAG_LENGTH])) {
                    std::string normalised_issn;
                    if (MiscUtil::NormaliseISSN(subfield_value, &normalised_issn)) {
                        (*output) << normalised_issn << ',' << record.getControlNumber() << ',' << record.getMainTitle() << '\n';
                        ++written_count;
                    } else {
                        ++malformed_count;
                        LOG_WARNING("Weird ISSN: \"" + subfield_value + "\"!");
                    }
                }
            }
        }
    }

    LOG_INFO("Found " + std::to_string(written_count) + " ISSN's associated with " + std::to_string(total_count)
             + " record(s), " + std::to_string(malformed_count) + " ISSN's were malformed.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 2 and argc != 3)
        Usage();

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[1]));
    std::unique_ptr<File> output(FileUtil::OpenOutputFileOrDie(argc == 3 ? argv[2] : Zotero::ISSN_TO_PPN_MAP_PATH));
    PopulateISSNtoControlNumberMapFile(marc_reader.get(), output.get());

    return EXIT_SUCCESS;
}
