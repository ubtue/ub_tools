/** \brief A tool for remid to copy field content.
 *  \author Steven Lolong (steven.lolong@uni-tuebingen.de)
 *
 *  \copyright 2023 Universitätsbibliothek Tübingen.  All rights reserved.
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

#include <fstream>
#include <iostream>
#include <string>
#include "FileUtil.h"
#include "MARC.h"

namespace {

[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << "  marc_input marc_output issn_output\n";
    std::cerr << "       marc_input is the marc input file\n";
    std::cerr << "       marc_output is the marc output file without serial\n";
    std::cerr << "       issn_output is the text output file for issn that ha\n";
    std::exit(EXIT_FAILURE);
}


} // namespace

int Main(int argc, char **argv) {
    if (argc != 4)
        Usage();

    std::set<std::string> issns;
    auto input_file(MARC::Reader::Factory(argv[1]));
    auto marc_output(MARC::Writer::Factory(argv[2]));
    bool serial;
    std::ofstream issn_file(argv[3]);
    while (MARC::Record record = input_file->read()) {
        serial = false;
        // It needs a temporary record (new_record) to hold all changes.
        // If it did a change on current active record, in some cases it will mis-calculate the size of the subfield inside which is
        // generate a unreadable (error) character.
        MARC::Record new_record(record);

        for (auto &field : record.getTagRange("035")) {
            const std::string issn(field.getFirstSubfieldWithCode('a'));
            if (StringUtil::ASCIIToUpper(issn.substr(0, 11)) == "(DE-599)ZDB") {
                issns.emplace(issn);
                serial = true;
                break;
            }
        }
        if (not serial) {
            for (const auto &fd : record.getTagRange("084")) {
                const auto subfds(fd.getSubfields());
                if (subfds.getFirstSubfieldWithCode('2') == "rvk") {
                    MARC::Tag mt("936");
                    MARC::Record::Field new_field(mt, 'r', 'v');

                    for (const auto &sfd : subfds)
                        new_field.appendSubfield(sfd.code_, sfd.value_);

                    new_field.deleteAllSubfieldsWithCode('2');
                    new_record.insertField(new_field);
                }
            }
            marc_output->write(new_record);
        }
    }

    for (auto &issn : issns)
        issn_file << issn << "\n";

    issn_file.close();


    return EXIT_SUCCESS;
}