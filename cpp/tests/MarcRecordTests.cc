/** \brief Test cases for MarcRecord
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
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
#include <typeinfo>
#include <vector>
#include "File.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include <iterator>


TEST(empty) {
    MarcRecord emptyRecord;
    BOOST_CHECK_EQUAL(emptyRecord, false);

    File input("data/marc_record_test.mrc", "r");
    BinaryMarcReader reader(&input);
    MarcRecord record(reader.read());
    BOOST_CHECK_EQUAL(record, true);
}


TEST(getNumberOfFields) {
    MarcRecord emptyRecord;
    BOOST_CHECK_EQUAL(emptyRecord.getNumberOfFields(), 0);

    File input("data/marc_record_test.mrc", "r");
    BinaryMarcReader reader(&input);
    MarcRecord record(reader.read());
    BOOST_CHECK_EQUAL(record.getNumberOfFields(), 13);

    size_t index(record.insertSubfield("TST", 'a', "TEST"));
    BOOST_CHECK_EQUAL(record.getNumberOfFields(), 14);
    
    record.deleteField(index);
    BOOST_CHECK_EQUAL(record.getNumberOfFields(), 13);
}


TEST(getFieldIndex) {
    MarcRecord emptyRecord;
    BOOST_CHECK_EQUAL(emptyRecord.getFieldIndex("001"), MarcRecord::FIELD_NOT_FOUND);

    File input("data/marc_record_test.mrc", "r");
    BinaryMarcReader reader(&input);
    MarcRecord record(reader.read());

    BOOST_CHECK_NE(record.getFieldIndex("001"), MarcRecord::FIELD_NOT_FOUND);

    BOOST_CHECK_EQUAL(record.getTag(record.getFieldIndex("001")), "001");
    BOOST_CHECK_EQUAL(record.getTag(record.getFieldIndex("100")), "100");
    BOOST_CHECK_EQUAL(record.getTag(record.getFieldIndex("LOK")), "LOK");
}


TEST(getFieldIndices) {
    MarcRecord emptyRecord;
    BOOST_CHECK_EQUAL(emptyRecord.getFieldIndex("001"), MarcRecord::FIELD_NOT_FOUND);

    size_t count;
    std::vector<size_t> indices;
    count = emptyRecord.getFieldIndices("001", &indices);
    BOOST_CHECK_EQUAL(count, 0);

    File input("data/marc_record_test.mrc", "r");
    BinaryMarcReader reader(&input);
    MarcRecord record(reader.read());
    count = record.getFieldIndices("001", &indices);
    BOOST_CHECK_EQUAL(count, 1);
    BOOST_CHECK_EQUAL(indices[0], 0);

    count = record.getFieldIndices("935", &indices);
    BOOST_CHECK_EQUAL(count, 2);
    BOOST_CHECK_EQUAL(indices.size(), count);

    count = record.getFieldIndices("LOK", &indices);
    BOOST_CHECK_EQUAL(count, 5);
    BOOST_CHECK_EQUAL(indices.size(), count);
}


TEST(getTag) {
    MarcRecord emptyRecord;
    BOOST_CHECK_EQUAL(emptyRecord.getTag(0), "");

    File input("data/marc_record_test.mrc", "r");
    BinaryMarcReader reader(&input);
    MarcRecord record(reader.read());

    BOOST_CHECK_EQUAL(record.getTag(0), "001");
}


TEST(deleteFields) {
    File input("data/marc_record_test.mrc", "r");
    BinaryMarcReader reader(&input);
    MarcRecord record(reader.read());

    std::vector<std::pair<size_t, size_t>> indices;
    indices.emplace_back(1, 3);
    indices.emplace_back(5, 10);

    BOOST_CHECK_EQUAL(record.getNumberOfFields(), 13);

    record.deleteFields(indices);

    BOOST_CHECK_EQUAL(record.getNumberOfFields(), 6);

}


TEST(findAllLocalDataBlocks) {
    File input("data/marc_record_test.mrc", "r");
    BinaryMarcReader reader(&input);
    MarcRecord record(reader.read());

    std::vector<std::pair<size_t, size_t>> local_blocks;
    size_t count(record.findAllLocalDataBlocks(&local_blocks));

    BOOST_CHECK_EQUAL(count, 2);
    BOOST_CHECK_EQUAL(local_blocks.size(), count);

    const auto first_block_length(local_blocks[0].second - local_blocks[0].first);
    BOOST_CHECK_EQUAL(first_block_length, 2);

    const auto second_block_length(local_blocks[1].second - local_blocks[1].first);
    BOOST_CHECK_EQUAL(second_block_length, 3);

}


TEST(extractSubfield) {
    File input("data/marc_record_test.mrc", "r");
    BinaryMarcReader reader(&input);
    MarcRecord record(reader.read());

    std::vector<std::string> values;
    record.extractSubfield("591", 'a', &values);
    BOOST_CHECK_EQUAL(values.size(), 1);

    record.extractSubfield("LOK", '0', &values);
    BOOST_CHECK_EQUAL(values.size(), 5);
}


TEST(filterTags) {
    File input("data/marc_record_test.mrc", "r");
    BinaryMarcReader reader(&input);
    MarcRecord record(reader.read());

    std::unordered_set<MarcTag> tags;
    tags.emplace("LOK");

    record.filterTags(tags);

    std::vector<std::pair<size_t, size_t>> local_blocks;
    size_t count(record.findAllLocalDataBlocks(&local_blocks));
    BOOST_CHECK_EQUAL(count, 0);
}


TEST(getLanguage) {
    MarcRecord emptyRecord;
    BOOST_CHECK_EQUAL(emptyRecord.getLanguage("not found"), "not found");
    BOOST_CHECK_EQUAL(emptyRecord.getLanguage(), "ger");

    File input("data/marc_record_test.mrc", "r");
    BinaryMarcReader reader(&input);
    MarcRecord record(reader.read());
    BOOST_CHECK_EQUAL(record.getLanguage("not found"), "ger");
    BOOST_CHECK_EQUAL(record.getLanguage(), "ger");
}


TEST(getLanguageCode) {
    MarcRecord emptyRecord;
    BOOST_CHECK_EQUAL(emptyRecord.getLanguageCode(), "");

    File input("data/marc_record_test.mrc", "r");
    BinaryMarcReader reader(&input);
    MarcRecord record(reader.read());
    BOOST_CHECK_EQUAL(record.getLanguageCode(), "ger");
}
