/** \file    solr_query_test.cc
 *  \brief   A test harness for the Solr::Query function.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2016-2017, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <cstring>
#include "Solr.h"
#include "util.h"
#include "StringUtil.h"


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " query fields host_and_port timeout query_result_format\n";
    std::cerr << "       Where \"query_result_format\" must be either \"XML\" or \"JSON\".\n\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 6)
        Usage();

    const std::string query(argv[1]);
    const std::string fields(argv[2]);
    const std::string host_and_port(argv[3]);

    unsigned timeout;
    if (not StringUtil::ToUnsigned(argv[4], &timeout) or timeout < 1)
        logger->error("can't convert \"" + std::string(argv[4]) + " \" to a postive integer!");

    const std::string result_format_candidate(argv[5]);
    Solr::QueryResultFormat result_format;
    if (result_format_candidate == "xml")
        result_format = Solr::QueryResultFormat::XML;
    else if (result_format_candidate == "json")
        result_format = Solr::QueryResultFormat::JSON;
    else
        logger->error("unknown query result format \"" + result_format_candidate + "\"!");

    try {
        std::string xml_result;
        if (not Solr::Query(query, fields, &xml_result, host_and_port, timeout, result_format))
            logger->error("query failed");

        std::cout << xml_result;
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
