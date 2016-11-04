/** \brief Test cases for Subfields
 *  \author Oliver Obenland (oliver.obenland@uni-tuebingen.de)
 *
 *  \copyright 2016 Universitätsbiblothek Tübingen.  All rights reserved.
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

#define BOOST_TEST_MODULE Subfields
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <iostream>
#include "Subfields.h"

TEST(order) {
    Subfields s1;
    s1.addSubfield('d', "Test");
    s1.addSubfield('b', "Test");
    s1.addSubfield('e', "Test");
    s1.addSubfield('c', "Test");

    BOOST_CHECK(s1.getIterators('b').second == s1.getIterators('c').first);
    BOOST_CHECK(s1.getIterators('c').second == s1.getIterators('d').first);
    BOOST_CHECK(s1.getIterators('d').second == s1.getIterators('e').first);

    s1.addSubfield('a', "Test");

    BOOST_CHECK(s1.getIterators('a').second == s1.getIterators('b').first);
    BOOST_CHECK(s1.getIterators('b').second == s1.getIterators('c').first);
    BOOST_CHECK(s1.getIterators('c').second == s1.getIterators('d').first);
    BOOST_CHECK(s1.getIterators('d').second == s1.getIterators('e').first);

}

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
    BOOST_CHECK(not s2.empty());

    s2.addSubfield('b', "Test");
    s2.addSubfield('c', "Test");
    BOOST_CHECK(not s2.empty());
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
    Subfields s1('1', '2');
    BOOST_CHECK_EQUAL(s1.size(), 0);
    BOOST_CHECK(not s1.hasSubfield('a'));
    BOOST_CHECK(not s1.hasSubfield('b'));

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
    s1.addSubfield('0', "Test");
    s1.addSubfield('a', "Test1");
    s1.addSubfield('a', "Test2");
    s1.addSubfield('a', "Test3");
    s1.addSubfield('b', "Test");

    BOOST_CHECK_EQUAL(s1.size(), 5);

    s1.erase('a');

    BOOST_CHECK_EQUAL(s1.size(), 2);
    BOOST_CHECK(s1.hasSubfieldWithValue('0', "Test"));
    BOOST_CHECK(not s1.hasSubfieldWithValue('a', "Test1"));
    BOOST_CHECK(not s1.hasSubfieldWithValue('a', "Test2"));
    BOOST_CHECK(not s1.hasSubfieldWithValue('a', "Test3"));
    BOOST_CHECK(s1.hasSubfieldWithValue('b', "Test"));
    s1.erase('0');
    s1.erase('b');

    s1.addSubfield('a', "Test1");
    s1.addSubfield('a', "Test2");
    s1.addSubfield('a', "Test3");

    s1.erase('a', "Test2");

    BOOST_CHECK_EQUAL(s1.size(), 2);
    BOOST_CHECK(s1.hasSubfieldWithValue('a', "Test1"));
    BOOST_CHECK(not s1.hasSubfieldWithValue('a', "Test2"));
    BOOST_CHECK(s1.hasSubfieldWithValue('a', "Test3"));
}

TEST(moveSubfield) {
    Subfields s1(' ', ' ');
    s1.addSubfield('c', "Test");
    s1.addSubfield('a', "Test1");
    s1.addSubfield('a', "Test2");
    s1.addSubfield('a', "Test3");

    BOOST_CHECK_EQUAL(s1.size(), 4);

    s1.moveSubfield('a', 'b');

    BOOST_CHECK_EQUAL(s1.size(), 4);
    BOOST_CHECK(s1.hasSubfieldWithValue('b', "Test1"));
    BOOST_CHECK(s1.hasSubfieldWithValue('b', "Test2"));
    BOOST_CHECK(s1.hasSubfieldWithValue('b', "Test3"));
    BOOST_CHECK(s1.hasSubfieldWithValue('c', "Test"));
}

TEST(getIterators) {
    Subfields s1;
    s1.addSubfield('a', "Test1");
    s1.addSubfield('b', "Test2");
    s1.addSubfield('b', "Test3");
    s1.addSubfield('c', "Test2");

    const auto begin_end_a(s1.getIterators('a'));
    const auto begin_end_b(s1.getIterators('b'));

    BOOST_CHECK(begin_end_a.second == begin_end_b.first);
    BOOST_CHECK(begin_end_a.second + 2 == begin_end_b.second);
}