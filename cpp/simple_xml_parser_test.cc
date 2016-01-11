/** \brief Test harness for the SimpleXmlParser class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "SimpleXmlParser.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " xml_input\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc != 2)
        Usage();

    const std::string input_filename(argv[1]);
    File input(input_filename, "rm");
    if (not input)
	Error("can't open \"" + input_filename + "\" for reading!");

    SimpleXmlParser::Type type;
    std::string data;
    std::map<std::string, std::string> attrib_map;
    SimpleXmlParser xml_parser(&input);
    while (xml_parser.getNext(&type, &attrib_map, &data)) {
	switch (type) {
	case SimpleXmlParser::UNINITIALISED:
	    Error("we should never get here as UNINITIALISED should never be returned!");
	case SimpleXmlParser::START_OF_DOCUMENT:
	    std::cout << "START_OF_DOCUMENT()\n";
	    break;
	case SimpleXmlParser::END_OF_DOCUMENT:
	    break;
	case SimpleXmlParser::ERROR:
	    Error("we should never get here because SimpleXmlParser::getNext() should have returned false!");
	case SimpleXmlParser::OPENING_TAG:
	    std::cout << "OPENING_TAG(" << data;
	    for (const auto &name_and_value : attrib_map)
		std::cout << ' ' << name_and_value.first << '=' << name_and_value.second;
	    std::cout << ")\n";
	    break;
	case SimpleXmlParser::CLOSING_TAG:
	    std::cout << "CLOSING_TAG(" << data << ")\n";
	    break;
	case SimpleXmlParser::CHARACTERS:
	    std::cout << "CHARACTERS(" << data << ")\n";
	    break;
	}
    }

    Error("XML parsing error: " + xml_parser.getLastErrorMessage());
}
