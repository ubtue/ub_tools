#define BOOST_TEST_MODULE Subfields
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include "Subfields.h"

TEST(indicators) {
        Subfields s1;
        BOOST_CHECK_EQUAL(s1.getIndicator1(), '\0');
        BOOST_CHECK_EQUAL(s1.getIndicator2(), '\0');

        s1.setIndicator1('a');
        BOOST_CHECK_EQUAL(s1.getIndicator1(), 'a');
        BOOST_CHECK_EQUAL(s1.getIndicator2(), '\0');

        s1.setIndicator2('b');
        BOOST_CHECK_EQUAL(s1.getIndicator1(), 'a');
        BOOST_CHECK_EQUAL(s1.getIndicator2(), 'b');
}

TEST(empty) {
        Subfields s1;
        BOOST_CHECK(s1.empty());

        Subfields s2(' ', ' ');
        BOOST_CHECK(s2.empty());

        s2.addSubfield('a', "Test");
        BOOST_CHECK(!s2.empty());

        s2.addSubfield('b', "Test");
        s2.addSubfield('c', "Test");
        BOOST_CHECK(!s2.empty());
}

TEST(size) {
        Subfields s1;
        BOOST_CHECK_EQUAL(s1.size(), 0);

        Subfields s2(' ', ' ');
        BOOST_CHECK_EQUAL(s2.size(), 0);

        s2.addSubfield('a', "Test");
        BOOST_CHECK_EQUAL(s2.size(), 1);

        s2.addSubfield('b', "Test");
        s2.addSubfield('c', "Test");
        BOOST_CHECK_EQUAL(s2.size(), 3);
}

TEST(addSubfield) {
        Subfields s1;
        BOOST_CHECK_EQUAL(s1.size(), 0);
        BOOST_CHECK(!s1.hasSubfield('a'));
        BOOST_CHECK(!s1.hasSubfield('b'));

        s1.addSubfield('a', "Test");
        BOOST_CHECK_EQUAL(s1.size(), 1);
        BOOST_CHECK(s1.hasSubfield('a'));
        BOOST_CHECK(s1.hasSubfieldWithValue('a', "Test"));

        s1.addSubfield('b', "Test");
        BOOST_CHECK_EQUAL(s1.size(), 2);
        BOOST_CHECK(s1.hasSubfield('a'));
        BOOST_CHECK(s1.hasSubfieldWithValue('a', "Test"));
        BOOST_CHECK(s1.hasSubfield('b'));
        BOOST_CHECK(s1.hasSubfieldWithValue('b', "Test"));
}

TEST(erase) {
        Subfields s1;
        s1.addSubfield('a', "Test1");
        s1.addSubfield('a', "Test2");
        s1.addSubfield('a', "Test3");

        BOOST_CHECK_EQUAL(s1.size(), 3);

        s1.erase('a');

        BOOST_CHECK_EQUAL(s1.size(), 0);
        BOOST_CHECK(!s1.hasSubfieldWithValue('a', "Test1"));
        BOOST_CHECK(!s1.hasSubfieldWithValue('a', "Test2"));
        BOOST_CHECK(!s1.hasSubfieldWithValue('a', "Test3"));

        s1.addSubfield('a', "Test1");
        s1.addSubfield('a', "Test2");
        s1.addSubfield('a', "Test3");

        s1.erase('a', "Test2");

        BOOST_CHECK_EQUAL(s1.size(), 2);
        BOOST_CHECK(s1.hasSubfieldWithValue('a', "Test1"));
        BOOST_CHECK(!s1.hasSubfieldWithValue('a', "Test2"));
        BOOST_CHECK(s1.hasSubfieldWithValue('a', "Test3"));
}

TEST(moveSubfield) {
        Subfields s1;
        s1.addSubfield('a', "Test1");
        s1.addSubfield('a', "Test2");
        s1.addSubfield('a', "Test3");

        BOOST_CHECK_EQUAL(s1.size(), 3);

        bool success = s1.moveSubfield('a', 'b');

        BOOST_CHECK(success);
        BOOST_CHECK_EQUAL(s1.size(), 3);
        BOOST_CHECK(s1.hasSubfieldWithValue('b', "Test1"));
        BOOST_CHECK(s1.hasSubfieldWithValue('b', "Test2"));
        BOOST_CHECK(s1.hasSubfieldWithValue('b', "Test3"));
}
