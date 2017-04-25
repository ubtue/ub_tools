/** \brief Test cases for MarcReader and MarcWriter
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016,2017 Universitätsbiblothek Tübingen.  All rights reserved.
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

#define BOOST_TEST_MODULE MarcRecord
#define BOOST_TEST_DYN_LINK


#include <boost/test/unit_test.hpp>
#include <cstdlib>
#include <string>
#include <vector>
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"


TEST(binary_read_write_compare) {
    File input("data/default.mrc", "rb");
    File output("/tmp/default.out.mrc", "wb");
    BinaryMarcReader reader(&input);
    MarcRecord record(reader.read());
    BinaryMarcWriter writer(&output);
    writer.write(record);
    std::cout << ("marc_compare " + input.getPath() + " " + output.getPath()) << "\n";
    output.close();

    int return_value(std::system(("marc_compare " + input.getPath() + " " + output.getPath()).c_str()));
    BOOST_CHECK_EQUAL(return_value, 0);
}


/*
TEST(xml_read_write_compare) {
    File input("data/default.xml", "r");
    File output("/tmp/default.out.xml", "w");
    XmlMarcReader reader(&input);
    MarcRecord record(reader.read());
    XmlMarcWriter writer(&output);
    writer.write(record);
    std::cout << ("marc_compare " + input.getPath() + " " + output.getPath()) << "\n";
    output.close();

    int return_value(std::system(("marc_compare " + input.getPath() + " " + output.getPath()).c_str()));
    BOOST_CHECK_EQUAL(return_value, 0);
}
*/


TEST(binary_large_record) {
    File input("data/default.mrc", "rb");
    File output("/tmp/default.out.mrc", "wb");
    BinaryMarcReader reader(&input);
    MarcRecord record(reader.read());

    Subfields subfields(' ', ' ');
    subfields.addSubfield('a', "This is a test string.");
    subfields.addSubfield('b', "This is a test string.");
    subfields.addSubfield('c', "This is a test string.");
    subfields.addSubfield('d', "This is a test string.");

    const size_t NUMBER_OF_FIELDS_TO_ADD(3000);
    for (size_t i = 0; i < NUMBER_OF_FIELDS_TO_ADD; ++i)
        record.insertField("TST", subfields.toString());

    BinaryMarcWriter writer(&output);
    writer.write(record);
    output.close();

    File new_input("/tmp/default.out.mrc", "r");
    BinaryMarcReader new_reader(&input);
    MarcRecord new_record(new_reader.read());

    std::vector<std::string> values;
    new_record.extractSubfield("TST", 'a', &values);
    std::cout << "-> " << new_record.getNumberOfFields() << "\n";
    BOOST_CHECK_EQUAL(values.size(), NUMBER_OF_FIELDS_TO_ADD);
}
