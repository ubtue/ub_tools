#define BOOST_TEST_MODULE MarcRecord
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <vector>
#include "File.h"
#include "MarcReader.h"
#include "MarcRecord.h"


TEST(empty) {
        MarcRecord emptyRecord;
        BOOST_CHECK_EQUAL(emptyRecord, false);

        File input("data/000596574.mrc", "r");
        MarcRecord record(MarcReader::Read(&input));
        BOOST_CHECK_EQUAL(record, true);
}

TEST(getNumberOfFields) {
        MarcRecord emptyRecord;
        BOOST_CHECK_EQUAL(emptyRecord.getNumberOfFields(), 0);

        File input("data/000596574.mrc", "r");
        MarcRecord record(MarcReader::Read(&input));
        BOOST_CHECK_EQUAL(record.getNumberOfFields(), 84);

        size_t index(record.insertSubfield("TST", 'a', "TEST"));
        BOOST_CHECK_EQUAL(record.getNumberOfFields(), 85);
        
        record.deleteField(index);
        BOOST_CHECK_EQUAL(record.getNumberOfFields(), 84);
}

TEST(getFieldIndex) {
        MarcRecord emptyRecord;
        BOOST_CHECK_EQUAL(emptyRecord.getFieldIndex("001"), MarcRecord::FIELD_NOT_FOUND);

        File input("data/000596574.mrc", "r");
        MarcRecord record(MarcReader::Read(&input));

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

        File input("data/000596574.mrc", "r");
        MarcRecord record(MarcReader::Read(&input));
        count = record.getFieldIndices("001", &indices);
        BOOST_CHECK_EQUAL(count, 1);
        BOOST_CHECK_EQUAL(indices[0], 0);

        count = record.getFieldIndices("935", &indices);
        BOOST_CHECK_EQUAL(count, 2);
        BOOST_CHECK_EQUAL(indices.size(), count);

        count = record.getFieldIndices("LOK", &indices);
        BOOST_CHECK_EQUAL(count, 57);
        BOOST_CHECK_EQUAL(indices.size(), count);
}

TEST(getTag) {
        MarcRecord emptyRecord;
        BOOST_CHECK_EQUAL(emptyRecord.getTag(0), "");

        File input("data/000596574.mrc", "r");
        MarcRecord record(MarcReader::Read(&input));

        BOOST_CHECK_EQUAL(record.getTag(0), "001");
}

TEST(deleteFields) {
        File input("data/000596574.mrc", "r");
        MarcRecord record(MarcReader::Read(&input));

        std::vector<std::pair<size_t, size_t>> indices;
        indices.emplace_back(0, 5);
        indices.emplace_back(10, 15);

        BOOST_CHECK_EQUAL(record.getNumberOfFields(), 84);

        record.deleteFields(indices);

        BOOST_CHECK_EQUAL(record.getNumberOfFields(), 74);

}

TEST(findAllLocalDataBlocks) {
        File input("data/000596574.mrc", "r");
        MarcRecord record(MarcReader::Read(&input));

        std::vector<std::pair<size_t, size_t>> local_blocks;
        size_t count(record.findAllLocalDataBlocks(&local_blocks));

        BOOST_CHECK_EQUAL(count, 6);
        BOOST_CHECK_EQUAL(local_blocks.size(), count);

 //       BOOST_CHECK_EQUAL(local_blocks[0], std::make_pair());
}

TEST(getLanguage) {
        MarcRecord emptyRecord;
        BOOST_CHECK_EQUAL(emptyRecord.getLanguage("not found"), "not found");
        BOOST_CHECK_EQUAL(emptyRecord.getLanguage(), "ger");

        File input("data/000596574.mrc", "r");
        MarcRecord record(MarcReader::Read(&input));
        BOOST_CHECK_EQUAL(record.getLanguage("not found"), "ger");
        BOOST_CHECK_EQUAL(record.getLanguage(), "ger");
}

TEST(getLanguageCode) {
        MarcRecord emptyRecord;
        BOOST_CHECK_EQUAL(emptyRecord.getLanguageCode(), "");

        File input("data/000596574.mrc", "r");
        MarcRecord record(MarcReader::Read(&input));
        BOOST_CHECK_EQUAL(record.getLanguageCode(), "ger");
}