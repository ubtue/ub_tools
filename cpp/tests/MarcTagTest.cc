#define BOOST_TEST_MODULE MarcRecord
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <string>
#include "MarcTag.h"

#define BOOST_CHECK_FALSE(__EXPR__) BOOST_CHECK(! __EXPR__ )

TEST(order) {
        size_t number_of_tags = 10;
        MarcTag unique_ordered_tags[] = { "001", "002", "010", "011", "012", "100", "101", "110", "111", "112" };
        MarcTag ordered_tags[] = { "001", "002", "011", "011", "012", "100", "101", "112", "112", "112" };

        // Test for <
        for (size_t i = 0; i + 1 < number_of_tags; ++i) {
            BOOST_CHECK_LT(unique_ordered_tags[i], unique_ordered_tags[i + 1]);
        }

        // Test for >
        for (size_t i = number_of_tags - 1; i > 1; --i) {
            BOOST_CHECK_GT(unique_ordered_tags[i], unique_ordered_tags[i - 1]);
        }

        // Test for <=
        for (size_t i = 0; i + 1 < number_of_tags; ++i) {
            BOOST_CHECK_LE(ordered_tags[i], ordered_tags[i + 1]);
        }

        // Test for >=
        for (size_t i = number_of_tags - 1; i > 1; --i) {
            BOOST_CHECK_GE(ordered_tags[i], ordered_tags[i - 1]);
        }

        // Test for !=
        for (size_t i = number_of_tags - 1; i > 1; --i) {
            BOOST_CHECK_NE(unique_ordered_tags[i], unique_ordered_tags[i - 1]);
        }
}

TEST(Equals) {
        MarcTag a("001"), b("001"), c("100");

        // MarcTag == MarcTag
        BOOST_CHECK_EQUAL(a, b);
        BOOST_CHECK_NE(a, c);
        BOOST_CHECK_NE(b, c);

        // MarcTag == String
        std::string _001("001"), _100("100");
        BOOST_CHECK_EQUAL(a, _001);
        BOOST_CHECK_NE(a, _100);

        // MarcTag == char*
        const char *_001_cstr(_001.c_str()), *_100_cstr(_100.c_str());
        BOOST_CHECK_EQUAL(a, _001_cstr);
        BOOST_CHECK_NE(a, _100_cstr);
}

TEST(isTagOfControlField) {
        BOOST_CHECK(MarcTag("001").isTagOfControlField());
        BOOST_CHECK(MarcTag("002").isTagOfControlField());
        BOOST_CHECK(MarcTag("003").isTagOfControlField());
        BOOST_CHECK(MarcTag("004").isTagOfControlField());
        BOOST_CHECK(MarcTag("005").isTagOfControlField());
        BOOST_CHECK(MarcTag("006").isTagOfControlField());
        BOOST_CHECK(MarcTag("007").isTagOfControlField());
        BOOST_CHECK(MarcTag("008").isTagOfControlField());
        BOOST_CHECK(MarcTag("009").isTagOfControlField());
        BOOST_CHECK_FALSE(MarcTag("010").isTagOfControlField());
        BOOST_CHECK_FALSE(MarcTag("011").isTagOfControlField());
        BOOST_CHECK_FALSE(MarcTag("012").isTagOfControlField());
        BOOST_CHECK_FALSE(MarcTag("100").isTagOfControlField());
        BOOST_CHECK_FALSE(MarcTag("101").isTagOfControlField());
        BOOST_CHECK_FALSE(MarcTag("110").isTagOfControlField());
}

TEST(constructor) {
        std::string _001("001");
        const char *_001_cstr(_001.c_str());
        BOOST_CHECK_EQUAL(MarcTag(_001), MarcTag(_001_cstr));

        BOOST_CHECK_THROW(MarcTag(std::string("invalid")), std::runtime_error);
}

TEST(toString) {
        MarcTag _001("001"), _100("100");
        BOOST_CHECK_EQUAL(_001.to_string(), "001");
        BOOST_CHECK_EQUAL(_100.to_string(), "100");
}
