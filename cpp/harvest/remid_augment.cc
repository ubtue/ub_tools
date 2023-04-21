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
    std::cerr << "Usage: " << ::progname << " <--insert_084_r_v | --serial_splitter> marc_input marc_output [issn_output]\n";
    std::cerr << "       --insert_084_r_v marc_input marc_output\n";
    std::cerr << "          marc_input is the marc input file\n";
    std::cerr << "          marc_output is the marc output file\n";
    std::cerr << "       --insert_084_r_v marc_input marc_output issn_output\n";
    std::cerr << "          marc_input is the marc input file\n";
    std::cerr << "          marc_output is the marc output file without serial\n";
    std::cerr << "          issn_output is the text output file for issn\n";
    std::exit(EXIT_FAILURE);
}

void SerialSplitter(char **argv) {
    auto input_file(MARC::Reader::Factory(argv[2]));
    auto output_file_marc(MARC::Writer::Factory(argv[3]));
    std::set<std::string> issns;
    bool serial;
    std::ofstream issn_file(argv[4]);

    while (MARC::Record record = input_file->read()) {
        serial = false;
        for (auto &field : record) {
            if (field.getTag() == "035") {
                const std::string issn(field.getFirstSubfieldWithCode('a'));
                if (not issn.empty() && StringUtil::ASCIIToUpper(issn.substr(0, 11)) == "(DE-599)ZDB") {
                    issns.emplace(issn);
                    serial = true;
                }
                break;
            }
        }
        if (not serial)
            output_file_marc->write(record);
    }

    for (auto &issn : issns)
        issn_file << issn << "\n";

    issn_file.close();
}

void InsertRVTo084(char **argv) {
    const std::string filter_field("084");
    const char filter_subfield('2');
    const std::string filter_subfield_value("rvk");
    const std::string target_field("936");
    int total_record(0);
    int total_new_field_added(0);

    auto marc_reader_pointer(MARC::Reader::Factory(argv[2]));
    MARC::Reader * const marc_reader(marc_reader_pointer.get());
    auto marc_writer(MARC::Writer::Factory(argv[3]));

    while (MARC::Record marc_record = marc_reader->read()) {
        const auto check_field(marc_record.findTag(filter_field));
        ++total_record;
        if (check_field == marc_record.end())
            continue;

        for (const auto &field : marc_record.getTagRange(filter_field)) {
            if (field.getTag() != filter_field)
                continue;

            const auto subfields(field.getSubfields());
            if (not subfields.hasSubfield(filter_subfield))
                continue;

            if (subfields.getFirstSubfieldWithCode(filter_subfield) == filter_subfield_value) {
                MARC::Tag new_marc_tag(target_field);
                MARC::Subfields marc_subfields(field.getContents());
                MARC::Record::Field new_field(new_marc_tag, marc_subfields, 'r', 'v');
                new_field.deleteAllSubfieldsWithCode(filter_subfield);
                marc_record.insertField(new_field);
                ++total_new_field_added;
            }
        }
        marc_writer->write(marc_record);
    }
    std::cout << "Processed a total of " << total_record << " record(s)\n";
    std::cout << "Added " << total_new_field_added << " new field(s)\n";
}

} // namespace

int Main(int argc, char **argv) {
    if (argc < 4 || argc > 5)
        Usage();

    if (argc == 4 && (std::strcmp(argv[1], "--insert_084_r_v") == 0))
        InsertRVTo084(argv);

    if (argc == 5 && (std::strcmp(argv[1], "--serial_splitter") == 0))
        SerialSplitter(argv);

    return EXIT_SUCCESS;
}