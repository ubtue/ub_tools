/** \brief Test harness for the XMLParser class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "File.h"
#include "XMLParser.h"
#include "util.h"


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--silent] xml_input\n";
    std::exit(EXIT_FAILURE);
}


int Main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2 and argc != 3)
        Usage();

    bool silent(false);
    if (argc == 3) {
        if (std::strcmp(argv[1], "--silent") != 0)
            Usage();
        silent = true;
    }

    const std::string input_filename(argv[silent ? 2 : 1]);

    XMLParser::XMLPart xml_part;
    XMLParser xml_parser(input_filename, XMLParser::XML_FILE);
    while (xml_parser.getNext(&xml_part)) {
        switch (xml_part.type_) {
        case XMLParser::XMLPart::UNINITIALISED:
            LOG_ERROR("we should never get here as UNINITIALISED should never be returned!");
        case XMLParser::XMLPart::OPENING_TAG:
            if (not silent) {
                std::cout << xml_parser.getLineNo() << ":OPENING_TAG(" << xml_part.data_;
                for (const auto &name_and_value : xml_part.attributes_)
                    std::cout << ' ' << name_and_value.first << '=' << name_and_value.second;
                std::cout << ")\n";
            }
            break;
        case XMLParser::XMLPart::CLOSING_TAG:
            if (not silent)
                std::cout << xml_parser.getLineNo() << ":CLOSING_TAG(" << xml_part.data_ << ")\n";
            break;
        case XMLParser::XMLPart::CHARACTERS:
            if (not silent)
                std::cout << xml_parser.getLineNo() << ":CHARACTERS(" << xml_part.data_ << ")\n";
            break;
        }
    }

    return EXIT_SUCCESS;
}
