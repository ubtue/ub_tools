/** \file    PageFetcherTest.cc
 *  \brief   Tests the PageFetcher class.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  \copyright 2003-2009 Project iVia.
 *  \copyright 2003-2009 The Regents of The University of California.
 *  \copyright 2017 Universitätsbibliothek Tübingen
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <iostream>
#include <cstdlib>
#include "PageFetcher.h"
#include "util.h"


void Usage() {
    std::cerr << "usage: " << ::progname << " [--ignore-robots-dot-txt] Web_URL\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if ((argc != 2 and argc != 3) or (argc == 3 and std::strcmp(argv[1], "--ignore-robots-dot-txt") != 0))
        Usage();
    const std::string url(argc == 3 ? argv[2] : argv[1]);
    const bool ignore_robots_dot_txt(argc == 3);

    try {
        const PageFetcher::RobotsDotTxtOption robots_dot_txt_option(
            ignore_robots_dot_txt ? PageFetcher::IGNORE_ROBOTS_DOT_TXT : PageFetcher::CONSULT_ROBOTS_DOT_TXT);
        PageFetcher page_fetcher(url, /* additional_http_headers = */ "", TimeLimit(20000), /* max_redirects = */ 7,
                                 /* ignore_redirect_errors = */ false, /* transparently_unzip_content = */true,
                                 "iVia Page Fetcher (http://ivia.ucr.edu/useragents.shtml)",
                                 /* acceptable_languages = */ "", robots_dot_txt_option);
        if (page_fetcher.anErrorOccurred()) {
            std::cerr << ::progname << ": " << page_fetcher.getErrorMsg() << '\n';
            std::exit(EXIT_FAILURE);
        }

        std::cout << page_fetcher.getData();
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
