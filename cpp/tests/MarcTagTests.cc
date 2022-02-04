/** \brief Test cases for MarcTag
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016,2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MARC.h"
#include "UnitTest.h"


TEST(order) {
    std::vector<MARC::Tag> unique_ordered_tags{ "001", "002", "010", "011", "012", "100", "101", "110", "111", "112" };
    std::vector<MARC::Tag> ordered_tags{ "001", "002", "011", "011", "012", "100", "101", "112", "112", "112" };

    // Test for <
    for (size_t i(0); i + 1 < unique_ordered_tags.size(); ++i)
        CHECK_LT(unique_ordered_tags[i], unique_ordered_tags[i + 1]);

    // Test for >
    for (size_t i(unique_ordered_tags.size() - 1); i > 1; --i)
        CHECK_GT(unique_ordered_tags[i], unique_ordered_tags[i - 1]);

    // Test for <=
    for (size_t i(0); i + 1 < ordered_tags.size(); ++i)
        CHECK_LE(ordered_tags[i], ordered_tags[i + 1]);

    // Test for >=
    for (size_t i(ordered_tags.size() - 1); i > 1; --i)
        CHECK_GE(ordered_tags[i], ordered_tags[i - 1]);

    // Test for !=
    for (size_t i(unique_ordered_tags.size() - 1); i > 1; --i)
        CHECK_NE(unique_ordered_tags[i], unique_ordered_tags[i - 1]);
}


TEST(order2) {
    size_t number_of_tags = 6;
    std::vector<MARC::Tag> ordered_tags{ MARC::Tag("000"), MARC::Tag("001"), MARC::Tag("004"),
                                         MARC::Tag("005"), MARC::Tag("008"), MARC::Tag("852") };

    // Test for <
    for (size_t i = 0; i + 1 < number_of_tags; ++i) {
        CHECK_LT(ordered_tags[i], ordered_tags[i + 1]);
    }

    // Test for >
    for (size_t i = number_of_tags - 1; i > 1; --i) {
        CHECK_GT(ordered_tags[i], ordered_tags[i - 1]);
    }

    // Test for <=
    for (size_t i = 0; i + 1 < number_of_tags; ++i) {
        CHECK_LE(ordered_tags[i], ordered_tags[i + 1]);
    }

    // Test for >=
    for (size_t i = number_of_tags - 1; i > 1; --i) {
        CHECK_GE(ordered_tags[i], ordered_tags[i - 1]);
    }

    // Test for !=
    for (size_t i = number_of_tags - 1; i > 1; --i) {
        CHECK_NE(ordered_tags[i], ordered_tags[i - 1]);
    }
}


TEST(Equals) {
    MARC::Tag a("001"), b("001"), c("100");

    // MARC::Tag == MARC::Tag
    CHECK_EQ(a, b);
    CHECK_NE(a, c);
    CHECK_NE(b, c);

    // MARC::Tag == String
    std::string _001("001"), _100("100");
    CHECK_EQ(a, _001);
    CHECK_NE(a, _100);

    // MARC::Tag == char*
    const char *_001_cstr(_001.c_str()), *_100_cstr(_100.c_str());
    CHECK_EQ(a, _001_cstr);
    CHECK_NE(a, _100_cstr);
}


TEST(isTagOfControlField) {
    CHECK_TRUE(MARC::Tag("001").isTagOfControlField());
    CHECK_TRUE(MARC::Tag("002").isTagOfControlField());
    CHECK_TRUE(MARC::Tag("003").isTagOfControlField());
    CHECK_TRUE(MARC::Tag("004").isTagOfControlField());
    CHECK_TRUE(MARC::Tag("005").isTagOfControlField());
    CHECK_TRUE(MARC::Tag("006").isTagOfControlField());
    CHECK_TRUE(MARC::Tag("007").isTagOfControlField());
    CHECK_TRUE(MARC::Tag("008").isTagOfControlField());
    CHECK_TRUE(MARC::Tag("009").isTagOfControlField());
    CHECK_FALSE(MARC::Tag("010").isTagOfControlField());
    CHECK_FALSE(MARC::Tag("011").isTagOfControlField());
    CHECK_FALSE(MARC::Tag("012").isTagOfControlField());
    CHECK_FALSE(MARC::Tag("100").isTagOfControlField());
    CHECK_FALSE(MARC::Tag("101").isTagOfControlField());
    CHECK_FALSE(MARC::Tag("110").isTagOfControlField());
}


TEST(constructor) {
    std::string _001("001");
    const char *_001_cstr(_001.c_str());
    CHECK_EQ(MARC::Tag(_001), MARC::Tag(_001_cstr));
}


TEST(toString) {
    MARC::Tag _001("001"), _100("100");
    CHECK_EQ(_001.toString(), "001");
    CHECK_EQ(_100.toString(), "100");
}


TEST_MAIN(MarcTags)
