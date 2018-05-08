/** \brief Test cases for MarcReader and MarcWriter
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <cstdlib>
#include "MARC.h"
#include "UnitTest.h"


TEST(binary_read_write_compare) {
    std::unique_ptr<MARC::Reader> reader(MARC::Reader::Factory("data/default.mrc"));
    MARC::Record record(reader->read());
    std::unique_ptr<MARC::Writer> writer(MARC::Writer::Factory("/tmp/default.out.mrc"));
    writer->write(record);

    std::cout << ("marc_compare " + reader->getPath() + " " + writer->getFile().getPath()) << "\n";
    writer->flush();

    const int return_value(std::system(("marc_compare " + reader->getPath() + " " + writer->getFile().getPath()).c_str()));
    CHECK_EQ(return_value, 0);
}


TEST(binary_large_record) {
    std::unique_ptr<MARC::Reader> reader(MARC::Reader::Factory("data/default.mrc"));
    MARC::Record record(reader->read());

    MARC::Subfields subfields;
    subfields.addSubfield('a', "This is a test string.");
    subfields.addSubfield('b', "This is a test string.");
    subfields.addSubfield('c', "This is a test string.");
    subfields.addSubfield('d', "This is a test string.");

    const size_t NUMBER_OF_FIELDS_TO_ADD(3000);
    for (size_t i(0); i < NUMBER_OF_FIELDS_TO_ADD; ++i)
        record.insertField("TST", subfields);

    std::unique_ptr<MARC::Writer> writer(MARC::Writer::Factory("/tmp/default.out.mrc"));
    writer->write(record);
    writer.reset();

    std::unique_ptr<MARC::Reader> new_reader(MARC::Reader::Factory("/tmp/default.out.mrc"));
    MARC::Record new_record(new_reader->read());

    unsigned subfield_a_count(0);
    for (const auto &tst_field : new_record.getTagRange("TST")) {
        if (tst_field.getSubfields().hasSubfieldWithValue('a', "This is a test string."))
            ++subfield_a_count;
    }

    std::cout << "-> " << new_record.getNumberOfFields() << "\n";
    CHECK_EQ(subfield_a_count, NUMBER_OF_FIELDS_TO_ADD);
}


TEST_MAIN(MarcReaderAndWriter)
