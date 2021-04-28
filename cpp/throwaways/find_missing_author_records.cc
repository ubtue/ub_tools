/** \brief Generate a list of records w/ missing authors in 100 w/ certain selection criteria in local fields.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "util.h"


namespace {


void ProcessRecords(MARC::Reader * const marc_reader, File * const output) {
    unsigned record_count(0), missing_author_count(0);
process_next_record:
    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        if (not record.getMainAuthor().empty())
            continue;

        for (const auto &local_field : record.getTagRange("LOK")) {
            if (local_field.getLocalTag() == "935") {
                const auto subfield_a(local_field.getFirstSubfieldWithCode('a'));
                if (subfield_a == "iFSA" or subfield_a == "iSWA" or subfield_a == "iZSA") { // See tuefind issue #1462
                    (*output) << record.getControlNumber() << '\n';
                    ++missing_author_count;
                    goto process_next_record;
                }
            }
        }
    }

    LOG_INFO("Processed " + std::to_string(record_count) + " record(s) and found " + std::to_string(missing_author_count)
             + " record(s) w/ missing 100$a subfields.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        ::Usage("marc_input ppn_list_output");

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    const auto output(FileUtil::OpenOutputFileOrDie(argv[2]));
    ProcessRecords(marc_reader.get(), output.get());

    return EXIT_SUCCESS;
}
