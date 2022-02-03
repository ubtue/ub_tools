/** \brief Test cases for MARC::Record
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
#include <vector>
#include "File.h"
#include "MARC.h"
#include "UnitTest.h"


TEST(empty) {
    MARC::Record emptyRecord(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::MONOGRAPHIC_COMPONENT_PART);
    CHECK_EQ(emptyRecord, false);

    std::unique_ptr<MARC::Reader> reader(MARC::Reader::Factory("data/marc_record_test.mrc"));
    MARC::Record record(reader->read());
    CHECK_EQ(record, true);
}


TEST(getNumberOfFields) {
    MARC::Record emptyRecord(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::MONOGRAPHIC_COMPONENT_PART);
    CHECK_EQ(emptyRecord.getNumberOfFields(), 0);

    std::unique_ptr<MARC::Reader> reader(MARC::Reader::Factory("data/marc_record_test.mrc"));
    MARC::Record record(reader->read());
    CHECK_EQ(record.getNumberOfFields(), 13);

    size_t index(record.insertField("TST", { { 'a', "TEST" } }));
    CHECK_EQ(record.getNumberOfFields(), 14);

    record.deleteFields({ index });
    CHECK_EQ(record.getNumberOfFields(), 13);
}


TEST(getFirstField) {
    MARC::Record emptyRecord(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::MONOGRAPHIC_COMPONENT_PART);
    CHECK_EQ(emptyRecord.getFirstField("001"), emptyRecord.end());

    std::unique_ptr<MARC::Reader> reader(MARC::Reader::Factory("data/marc_record_test.mrc"));
    MARC::Record record(reader->read());

    CHECK_NE(record.getFirstField("001"), emptyRecord.end());

    CHECK_EQ(record.getFirstField("001")->getTag().toString(), "001");
    CHECK_EQ(record.getFirstField("100")->getTag().toString(), "100");
    CHECK_EQ(record.getFirstField("LOK")->getTag().toString(), "LOK");
}


TEST(getTagRange) {
    MARC::Record emptyRecord(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::MONOGRAPHIC_COMPONENT_PART);
    CHECK_EQ(emptyRecord.getFirstField("001"), emptyRecord.end());

    size_t count(emptyRecord.getTagRange("001").size());
    CHECK_EQ(count, 0);

    std::unique_ptr<MARC::Reader> reader(MARC::Reader::Factory("data/marc_record_test.mrc"));
    MARC::Record record(reader->read());
    count = record.getTagRange("001").size();
    CHECK_EQ(count, 1);

    count = record.getTagRange("935").size();
    CHECK_EQ(count, 2);

    count = record.getTagRange("LOK").size();
    CHECK_EQ(count, 5);
}


TEST(hasTag) {
    MARC::Record emptyRecord(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::MONOGRAPHIC_COMPONENT_PART);
    CHECK_FALSE(emptyRecord.hasTag("001"));

    std::unique_ptr<MARC::Reader> reader(MARC::Reader::Factory("data/marc_record_test.mrc"));
    MARC::Record record(reader->read());

    CHECK_TRUE(record.hasTag("001"));
}


TEST(deleteFields) {
    std::unique_ptr<MARC::Reader> reader(MARC::Reader::Factory("data/marc_record_test.mrc"));
    MARC::Record record(reader->read());

    CHECK_EQ(record.getNumberOfFields(), 13);

    const std::vector<size_t> indices{ 3, 5, 6 };
    record.deleteFields(indices);

    CHECK_EQ(record.getNumberOfFields(), 10);
}


TEST(hasSubfield) {
    std::unique_ptr<MARC::Reader> reader(MARC::Reader::Factory("data/marc_record_test.mrc"));
    MARC::Record record(reader->read());

    unsigned _591_a_count(0);
    for (const auto &_591_field : record.getTagRange("591")) {
        if (_591_field.getSubfields().hasSubfield('a'))
            ++_591_a_count;
    }
    CHECK_EQ(_591_a_count, 1);

    unsigned LOK_0_count(0);
    for (const auto &LOK_field : record.getTagRange("LOK")) {
        if (LOK_field.getSubfields().hasSubfield('a'))
            ++LOK_0_count;
    }
    CHECK_EQ(LOK_0_count, 1);
}


TEST(filterTags) {
    std::unique_ptr<MARC::Reader> reader(MARC::Reader::Factory("data/marc_record_test.mrc"));
    MARC::Record record(reader->read());

    auto field_iter(record.findTag("LOK"));
    while (field_iter != record.end())
        field_iter = record.erase(field_iter);

    std::vector<std::pair<MARC::Record::const_iterator, MARC::Record::const_iterator>> local_blocks;
    CHECK_TRUE(record.findStartOfAllLocalDataBlocks().empty());
}


TEST(getLanguageCode) {
    MARC::Record emptyRecord(MARC::Record::TypeOfRecord::LANGUAGE_MATERIAL, MARC::Record::BibliographicLevel::MONOGRAPHIC_COMPONENT_PART);
    CHECK_NE(MARC::GetLanguageCode(emptyRecord), "not found");
    CHECK_EQ(MARC::GetLanguageCode(emptyRecord), "");

    std::unique_ptr<MARC::Reader> reader(MARC::Reader::Factory("data/marc_record_test.mrc"));
    MARC::Record record(reader->read());
    CHECK_NE(MARC::GetLanguageCode(record), "not found");
    CHECK_EQ(MARC::GetLanguageCode(record), "ger");
}


TEST_MAIN(MARC::Record)
