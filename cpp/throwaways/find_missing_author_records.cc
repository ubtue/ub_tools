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

#include <algorithm>
#include <cstdlib>
#include "FileUtil.h"
#include "MARC.h"
#include "util.h"


namespace {


void ProcessRecords(const std::vector<std::string> &lokale_abrufzeichen, MARC::Reader * const marc_reader, File * const output) {
    unsigned record_count(0), missing_author_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        ++record_count;

        if (record.hasTag("100"))
            continue;

        bool found_a_match(false);
        for (const auto &local_field : record.getTagRange("LOK")) {
            if (local_field.getFirstSubfieldWithCode('0') != "935  ")
                continue;

            const auto subfield_a(local_field.getFirstSubfieldWithCode('a'));
            if (std::find(lokale_abrufzeichen.cbegin(), lokale_abrufzeichen.cend(), subfield_a) != lokale_abrufzeichen.cend()) {
                found_a_match = true;
                break;
            }
        }

        if (found_a_match) {
            std::string STAR_ID;
            for (const auto &local_field : record.getTagRange("LOK")) {
                if (local_field.getFirstSubfieldWithCode('0') != "035  ")
                    continue;

                const auto _035_subfield_a(local_field.getFirstSubfieldWithCode('a'));
                if (StringUtil::StartsWith(_035_subfield_a, "(DE-Tue135-1)")) {
                    STAR_ID = _035_subfield_a.substr(__builtin_strlen("(DE-Tue135-1)"));
                    break;
                }
            }

            (*output) << record.getControlNumber() << ',' << (STAR_ID.empty() ? "\"STAR-ID fehlt!\"" : STAR_ID) << '\n';
            ++missing_author_count;
        }
    }

    LOG_INFO("Processed " + std::to_string(record_count) + " record(s) and found " + std::to_string(missing_author_count)
             + " record(s) w/ missing 100$a subfields.");
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 4)
        ::Usage("marc_input ppn+STAR-ID_list_output lokales_abrufzeichen1 [lokales_abrufzeichen2 .. lokales_abrufzeichenN]");

    auto marc_reader(MARC::Reader::Factory(argv[1]));
    const auto output(FileUtil::OpenOutputFileOrDie(argv[2]));

    std::vector<std::string> lokale_abrufzeichen;
    for (int arg_no(3); arg_no < argc; ++arg_no)
        lokale_abrufzeichen.emplace_back(argv[arg_no]);

    ProcessRecords(lokale_abrufzeichen, marc_reader.get(), output.get());

    return EXIT_SUCCESS;
}
