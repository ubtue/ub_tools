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
#include "MARC.h"

namespace {

[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " marc_input marc_output\n";
    std::cerr << "       marc_input is the marc input file\n";
    std::cerr << "       marc_output is the marc output file\n";
    std::exit(EXIT_FAILURE);
}

} // namespace

int Main(int argc, char **argv) {
    if (argc < 3)
        Usage();


    const std::string input_filename(argv[1]);
    const std::string output_filename(argv[2]);
    const std::string filter_field("084");
    const char filter_subfield('2');
    const std::string filter_subfield_value("rvk");
    const std::string target_field("936");
    int total_record(0);
    int total_new_field_added(0);

    auto marc_reader_pointer(MARC::Reader::Factory(input_filename));
    MARC::Reader * const marc_reader(marc_reader_pointer.get());
    auto marc_writer(MARC::Writer::Factory(output_filename));

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
                new_field.deleteAllSubfieldsWithCode('2');
                marc_record.insertField(new_field);
                ++total_new_field_added;
            }
        }
        marc_writer->write(marc_record);
    }
    std::cout << "Processed a total of " << total_record << " record(s)\n";
    std::cout << "Added " << total_new_field_added << " new field(s)\n";
    return EXIT_SUCCESS;
}