/** \brief A tool for writing log messages in a shell script.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <cstdlib>
#include "util.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "Usage: " << ::progname << " log_level message\n";
    std::cerr << "       Where \"log_level\" must be one of SEVERE, WARN, INFO or DEBUG.\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    if (argc != 3)
        Usage();

    const std::string log_level(argv[1]);
    if (log_level != "SEVERE" and log_level != "WARN" and log_level != "INFO" and log_level != "DEBUG")
        logger->error("bad log level \"" + log_level + "\"!@");
    logger->redirectOutput(STDOUT_FILENO);

    const std::string message(argv[2]);
    if (log_level == "SEVERE")
        logger->error(message);
    else if (log_level == "WARN")
        logger->warning(message);
    else if (log_level == "INFO")
        logger->info(message);
    else if (log_level == "DEBUG")
        logger->debug(message);
    else
        logger->error("unsupported log level: " + log_level);
}
