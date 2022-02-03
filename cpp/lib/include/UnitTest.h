/** \brief Unit test macros.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#pragma once


#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <cstdlib>
#include "util.h"


static std::vector<std::pair<void (*)(), std::string>> tests;
static unsigned success_count, failure_count;


#define TEST_MAIN(name)                                                        \
    int main(int /*argc*/, char *argv[]) {                                     \
        ::progname = argv[0];                                                  \
        std::cerr << "*** " << #name << " ***\n";                              \
        for (const auto &func_and_name : tests) {                              \
            std::cerr << "Calling test \"" << func_and_name.second << "\".\n"; \
            func_and_name.first();                                             \
        }                                                                      \
                                                                               \
        std::cerr << "*** " << success_count << " tests succeeded. ***\n";     \
        std::cerr << "*** " << failure_count << " tests failed. ***\n";        \
        return (failure_count > 0) ? EXIT_FAILURE : EXIT_SUCCESS;              \
    }

#define TEST(test_name)                             \
    static void test_name();                        \
    static int register_##test_name() {             \
        tests.emplace_back(test_name, #test_name);  \
        return 0;                                   \
    }                                               \
    int dummy_##test_name = register_##test_name(); \
    void test_name()

#define CHECK_TRUE(a)                                                  \
    do {                                                               \
        if ((a))                                                       \
            ++success_count;                                           \
        else {                                                         \
            ++failure_count;                                           \
            std::cerr << "\tTest failed: " << #a << " is not true!\n"; \
        }                                                              \
    } while (0)

#define CHECK_FALSE(a)                                                  \
    do {                                                                \
        if (not(a))                                                     \
            ++success_count;                                            \
        else {                                                          \
            ++failure_count;                                            \
            std::cerr << "\tTest failed: " << #a << " is not false!\n"; \
        }                                                               \
    } while (0)

#define CHECK_LT(a, b)                                                \
    do {                                                              \
        if ((a) < (b))                                                \
            ++success_count;                                          \
        else {                                                        \
            ++failure_count;                                          \
            std::cerr << "\tTest failed: " << #a " < " << #b << '\n'; \
        }                                                             \
    } while (0)

#define CHECK_GT(a, b)                                                \
    do {                                                              \
        if ((a) > (b))                                                \
            ++success_count;                                          \
        else {                                                        \
            ++failure_count;                                          \
            std::cerr << "\tTest failed: " << #a " > " << #b << '\n'; \
        }                                                             \
    } while (0)

#define CHECK_LE(a, b)                                                 \
    do {                                                               \
        if ((a) <= (b))                                                \
            ++success_count;                                           \
        else {                                                         \
            ++failure_count;                                           \
            std::cerr << "\tTest failed: " << #a " <= " << #b << '\n'; \
        }                                                              \
    } while (0)

#define CHECK_GE(a, b)                                                 \
    do {                                                               \
        if ((a) >= (b))                                                \
            ++success_count;                                           \
        else {                                                         \
            ++failure_count;                                           \
            std::cerr << "\tTest failed: " << #a " >= " << #b << '\n'; \
        }                                                              \
    } while (0)

#define CHECK_EQ(a, b)                                                 \
    do {                                                               \
        if ((a) == (b))                                                \
            ++success_count;                                           \
        else {                                                         \
            ++failure_count;                                           \
            std::cerr << "\tTest failed: " << #a " == " << #b << '\n'; \
        }                                                              \
    } while (0)

#define CHECK_NE(a, b)                                                 \
    do {                                                               \
        if ((a) != (b))                                                \
            ++success_count;                                           \
        else {                                                         \
            ++failure_count;                                           \
            std::cerr << "\tTest failed: " << #a " != " << #b << '\n'; \
        }                                                              \
    } while (0)
