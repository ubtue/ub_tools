/** \file   downloader_test.cc
 *  \brief  Test harness for the Downloader class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2017 Universitätsbibliothek Tübingen.
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
#include <stdexcept>
#include <cstdlib>
#include "Downloader.h"
#include "StringUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " [--timeout milli_seconds] [--honour-robots-dot-txt] url\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc < 2)
        Usage();

    Downloader::Params params;

    TimeLimit time_limit(Downloader::DEFAULT_TIME_LIMIT);
    if (std::strcmp(argv[1], "--timeout") == 0) {
        unsigned timeout;
        if (not StringUtil::ToUnsigned(argv[2], &timeout))
            Error("bad timeout \"" + std::string(argv[2]) + "\"!");
        --argc, ++argv;
        time_limit = timeout;
    }

    if (argc < 2)
        Usage();

    if (std::strcmp(argv[1], "--honour-robots-dot-txt") == 0) {
        params.honour_robots_dot_txt_ = true;
        --argc, ++argv;
    }

    if (argc != 2)
        Usage();

    const std::string url(argv[1]);

    try {
        Downloader downloader(url, params, time_limit);
        if (downloader.anErrorOccurred())
            Error(downloader.getLastErrorMessage());
        std::cout << downloader.getMessageBody() << '\n';
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
