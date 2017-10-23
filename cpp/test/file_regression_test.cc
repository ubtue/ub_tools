/** \file   file_regression_test.cc
 *  \brief  Regression test
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
#include <iostream>
#include <fstream>
#include <cstdlib>
#include "File.h"
#include "RegressionTest.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << ::progname << " logfile_path\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
        Usage();

    const std::string logfile_path(argv[1]);
    std::ofstream logfile(logfile_path);
    if (not logfile)
        logger->error("failed to open \"" + logfile_path + "\" for writing!");

    // Redirect std::clog to write to logfile:
    auto old_clog_rdbuf(std::clog.rdbuf()); // We need to reset clog's buffer back to the original before exiting!
    std::clog.rdbuf(logfile.rdbuf());
    
    try {
        File file("/tmp/file_regression_test.file", "r+", File::THROW_ON_ERROR);
        RegressionTest::Assert("make sure our file is open", "file", !!file);

        const std::string JELLO_MOLD("Jello mold!");
        file << JELLO_MOLD;
        file.flush();
        file.rewind();
        std::string s;
        file.getline(&s);
        if (s != JELLO_MOLD)
            logger->error("Expected \"" + JELLO_MOLD + "\" read \"" + s + "\"!");

        file.seek(6);
        file << "world!";
        file.rewind();
        file.getline(&s);
        if (s != "Jello world!")
            logger->error("Expected \"Jello world!\" read \"" + s + "\"!");
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }

    // Restore clog's original buffer or we crash!
    std::clog.rdbuf(old_clog_rdbuf);
}
