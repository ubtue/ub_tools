/** \brief Test harness for the SimpleXmlParser class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "SimpleXmlParser.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " [--silent] xml_input\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc != 2 and argc != 3)
        Usage();

    bool silent(false);
    if (argc == 3) {
        if (std::strcmp(argv[1], "--silent") != 0)
            Usage();
        silent = true;
    }

    const std::string input_filename(argv[silent ? 2 : 1]);
    File input(input_filename, "r");
    if (not input)
        logger->error("can't open \"" + input_filename + "\" for reading!");

    try {
        SimpleXmlParser<File>::Type type;
        std::string data;
        std::map<std::string, std::string> attrib_map;
        SimpleXmlParser<File> xml_parser(&input);
        while (xml_parser.getNext(&type, &attrib_map, &data)) {
            switch (type) {
            case SimpleXmlParser<File>::UNINITIALISED:
                logger->error("we should never get here as UNINITIALISED should never be returned!");
            case SimpleXmlParser<File>::START_OF_DOCUMENT:
                if (not silent)
                    std::cout << xml_parser.getLineNo() << ":START_OF_DOCUMENT()\n";
                break;
            case SimpleXmlParser<File>::END_OF_DOCUMENT:
                return EXIT_SUCCESS;
            case SimpleXmlParser<File>::ERROR:
                logger->error("we should never get here because SimpleXmlParser<File>::getNext() should have "
                              "returned false!");
            case SimpleXmlParser<File>::OPENING_TAG:
                if (not silent) {
                    std::cout << xml_parser.getLineNo() << ":OPENING_TAG(" << data;
                    for (const auto &name_and_value : attrib_map)
                        std::cout << ' ' << name_and_value.first << '=' << name_and_value.second;
                    std::cout << ")\n";
                }
                break;
            case SimpleXmlParser<File>::CLOSING_TAG:
                if (not silent)
                    std::cout << xml_parser.getLineNo() << ":CLOSING_TAG(" << data << ")\n";
                break;
            case SimpleXmlParser<File>::CHARACTERS:
                if (not silent)
                    std::cout << xml_parser.getLineNo() << ":CHARACTERS(" << data << ")\n";
                break;
            }
        }

        logger->error("XML parsing error: " + xml_parser.getLastErrorMessage());
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}
