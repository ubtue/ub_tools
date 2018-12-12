/** \file    full_text_lookup.cc
 *  \brief   A cgi script for looking up texts in a database. Each text is referenced by an ID.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015,2017 Library of the University of Tübingen

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
#include <string>
#include <cstdlib>
#include <cstring>
#include "FullTextCache.h"
#include "util.h"


namespace {


bool GetIdFromCGI(std::string * const id) {
    const char * const query = ::getenv("QUERY_STRING");
    const size_t ID_OFFSET(3);
    if (query == nullptr or std::strlen(query) < (ID_OFFSET + 1))
        return false;

    *id = query + ID_OFFSET;
    return true;
}


void Lookup(const std::string &id) {
    try {
        FullTextCache cache;

        std::string data;
        if (not cache.getFullText(id, &data))
            throw std::runtime_error("fulltext not found for id: " + id);

        std::cout << "Content-Type: text/plain\r\n\r\n";
        std::cout << data;
    } catch (const std::exception &e) {
        logger->error(std::string("caught exception: ") + e.what());
    }
}


} // unnamed namespace


int main(int argc, char* argv[]) {
    ::progname = argv[0];

    std::string id;
    if (argc == 2)
        id = argv[1];
    else if (not GetIdFromCGI(&id)) {
        std::cerr << "ERROR: couldn't parse input!\n";
        return EXIT_FAILURE;
    }

    Lookup(id);
}
