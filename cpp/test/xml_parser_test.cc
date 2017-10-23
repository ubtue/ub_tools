/** \file    xml_parser_test.cc
 *  \brief   Tests the XmlParser class
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2002-2007 Project iVia.
 *  Copyright 2002-2007 The Regents of The University of California.
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
#include <cstring>
#include <cstdlib>
#include "StringUtil.h"
#include "util.h"
#include "XmlParser.h"


class TestParser: public XmlParser {
public:
    TestParser(const std::string &filename, unsigned notification_mask)
        : XmlParser(filename, true, notification_mask) { }
    virtual void notify(const Chunk &chunk);
};


void TestParser::notify(const Chunk &chunk) {
    static bool error_or_warning_found(false);
    if (error_or_warning_found)
        return;

    if (chunk.type_ == XmlParser::START_DOCUMENT)
        std::cerr << "Document started on line " << chunk.lineno_ << ".\n";
    else if (chunk.type_ == XmlParser::END_DOCUMENT)
        std::cerr << "Document ended on line " << chunk.lineno_ << ".\n";
    else if (chunk.type_ == XmlParser::START_ELEMENT) {
        std::cerr << "Starting element: " << chunk.text_ << " (line " << chunk.lineno_ << ").\n";
        for (XmlParser::AttributeMap::const_iterator attrib(chunk.attribute_map_->begin());
             attrib != chunk.attribute_map_->end(); ++attrib)
            std::cerr << '\t' << attrib->first << " = " << attrib->second << '\n';
    } else if (chunk.type_ == XmlParser::CHARACTERS)
        std::cerr << "Characters: " << StringUtil::CStyleEscape(chunk.text_) << " (line " << chunk.lineno_
                  << ").\n";
    else if (chunk.type_ == XmlParser::IGNORABLE_WHITESPACE)
        std::cerr << "Ignorable whitespace: " << StringUtil::CStyleEscape(chunk.text_) << " (line "
                  << chunk.lineno_ << ").\n";
    else if (chunk.type_ == XmlParser::END_ELEMENT)
        std::cerr << "Ending element: " << chunk.text_ << " (line " << chunk.lineno_ << ").\n";
    else if (chunk.type_ == XmlParser::WARNING) {
        error_or_warning_found = true;
        std::cerr << "Warning:  " << chunk.text_ << " (line " << chunk.lineno_ << ").\n";
    } else if (chunk.type_ == XmlParser::ERROR) {
        error_or_warning_found = true;
        std::cerr << "Error:  " << chunk.text_ << " (line " << chunk.lineno_ << ").\n";
    } else if (chunk.type_ == XmlParser::FATAL_ERROR) {
        error_or_warning_found = true;
        std::cerr << "Fatal error:  " << chunk.text_ << " (line " << chunk.lineno_ << ").\n";
    }
}


__attribute__((noreturn)) void Usage() {
    std::cerr << "usage: " << ::progname << " [--report-start-of-document]"
              << " [--report-end-of-document] [--report-warnings] [--report-errors]"
              << " input_filename\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    try {
        if (argc < 2)
            Usage();

        unsigned notification_mask(0);
        for (int arg_no = 1; arg_no < argc - 1; ++arg_no) {
            if (std::strcmp("--report-start-of-document", argv[arg_no]) == 0)
                notification_mask |= XmlParser::START_DOCUMENT;
            else if (std::strcmp("--report-end-of-document", argv[arg_no]) == 0)
                notification_mask |= XmlParser::END_DOCUMENT;
            else if (std::strcmp("--report-warnings", argv[arg_no]) == 0)
                notification_mask |= XmlParser::WARNING;
            else if (std::strcmp("--report-errors", argv[arg_no]) == 0)
                notification_mask |= XmlParser::ERROR;
            else
                Usage();
        }
        if (notification_mask == 0)
            notification_mask = XmlParser::EVERYTHING;

        TestParser test_parser(argv[argc - 1], notification_mask);
        if (!test_parser.parse())
            std::cerr << "The parse failed!\n";

        return EXIT_SUCCESS;
    } catch (const std::exception &x) {
        logger->error("caught exception: " + std::string(x.what()));
    }
}

