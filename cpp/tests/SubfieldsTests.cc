/** \brief Test cases for Subfields
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
    MARC::Subfields s1;
    s1.addSubfield('d', "Test");
    s1.addSubfield('b', "Test");
    s1.addSubfield('e', "Test");
    s1.addSubfield('c', "Test");

    CHECK_EQ(s1.begin()->value_, (s1.end() - 1)->value_);
    CHECK_EQ(s1.begin()->code_, 'b');

    s1.addSubfield('a', "Test");
}


TEST(empty) {
    MARC::Subfields s;
    CHECK_TRUE(s.empty());

    s.addSubfield('a', "Test");
    CHECK_TRUE(not s.empty());
}


TEST(size) {
    MARC::Subfields s2;
    CHECK_EQ(s2.size(), 0);

    s2.addSubfield('a', "Test");
    CHECK_EQ(s2.size(), 1);

    s2.addSubfield('b', "Test");
    s2.addSubfield('c', "Test");
    CHECK_EQ(s2.size(), 3);
}


TEST(addSubfield) {
    MARC::Subfields s1;
    CHECK_EQ(s1.size(), 0);
    CHECK_TRUE(not s1.hasSubfield('a'));
    CHECK_TRUE(not s1.hasSubfield('b'));

    s1.addSubfield('a', "Test");
    CHECK_EQ(s1.size(), 1);
    CHECK_TRUE(s1.hasSubfield('a'));
    CHECK_TRUE(s1.hasSubfieldWithValue('a', "Test"));

    s1.addSubfield('b', "Test");
    CHECK_EQ(s1.size(), 2);
    CHECK_TRUE(s1.hasSubfield('a'));
    CHECK_TRUE(s1.hasSubfieldWithValue('a', "Test"));
    CHECK_TRUE(s1.hasSubfield('b'));
    CHECK_TRUE(s1.hasSubfieldWithValue('b', "Test"));
}


TEST(Iterators) {
    MARC::Subfields s1;
    s1.addSubfield('a', "Test1");
    s1.addSubfield('b', "Test2");
    s1.addSubfield('b', "Test3");
    s1.addSubfield('c', "Test2");

    const auto iter(s1.begin());
    CHECK_EQ(iter + 4, s1.end());
}


TEST_MAIN(MARC::Subfields)
