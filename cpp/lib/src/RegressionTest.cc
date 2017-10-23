/** \file   RegressionTest.cc
 *  \brief  Regression test related utility functions and classes.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2016-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "RegressionTest.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include "FileUtil.h"
#include "util.h"


namespace RegressionTest {


void Assert(const std::string &test_name, const std::string &condition_as_string, const bool condition) {
    if (not condition) {
        std::clog << test_name << ": condition failed: " << condition_as_string << '\n';
        logger->error("in RegessionTest::Assert:  condition failed: \"" + condition_as_string + "\"!");
    }
}
    

bool CompareStrings(const std::string &test_name, const std::string &actual_string,
                    const std::string &expected_string)
{
    if (actual_string == expected_string) {
        logger->info(test_name + ": strings matched as expected.");
        return true;
    } else {
        logger->info(test_name + ": strings \"" + actual_string + "\" and \"" + expected_string
                     + "\" did not match!");
        return false;
    }
}

    
bool CompareFiles(const std::string &test_name, const std::string &actual_file, const std::string &expected_file,
                  const bool delete_actual)
{
    if (FileUtil::FilesDiffer(actual_file, expected_file)) {
        logger->info(test_name + ": files \"" + actual_file + "\" and \"" + expected_file + "\" differ!.");
        return false;
    } else {
        logger->info(test_name + ": files \"" + actual_file + "\" and \"" + expected_file
                     + "\" matched as expected.");

        if (delete_actual and ::unlink(actual_file.c_str()) != 0)
            logger->error("in RegessionTest::CompareFiles: failed to unlink(2) \"" + actual_file + "\"!");
        
        return true;
    }
}
    

} // namespace RegressionTest
