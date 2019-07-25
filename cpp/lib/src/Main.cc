/** \file   Main.cc
 *  \brief  Default main entry point.
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
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <cerrno>
#include "StringUtil.h"
#include "util.h"


int Main(int argc, char *argv[]);


int main(int argc, char *argv[]) __attribute__((weak));


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    Logger::LogLevel log_level(Logger::LL_INFO);
    if (argc > 1 and StringUtil::StartsWith(argv[1], "--min-log-level=")) {
        const std::string level(argv[1] + __builtin_strlen("--min-log-level="));
        if (level == "ERROR")
            log_level = Logger::LL_ERROR;
        else if (level == "WARNING")
            log_level = Logger::LL_WARNING;
        else if (level == "INFO")
            log_level = Logger::LL_INFO;
        else if (level == "DEBUG")
            log_level = Logger::LL_DEBUG;
        else
            LOG_ERROR("unknown log level \"" + level + "\"!");
        --argc, ++argv;
    }
    logger->setMinimumLogLevel(log_level);

    try {
        errno = 0;
        return Main(argc, argv);
    } catch (const std::exception &x) {
        LOG_ERROR("caught exception: " + std::string(x.what()));
    }
}

