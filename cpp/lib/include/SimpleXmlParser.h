/** \file   SimpleXmlParser.h
 *  \brief  A non-validating XML parser class.
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
#ifndef SIMPLE_XML_PARSER_H
#define SIMPLE_XML_PARSER_H


#include <map>
#include <string>
#include "File.h"


class SimpleXmlParser {
public:
    enum Type { UNINITIALISED, START_OF_DOCUMENT, END_OF_DOCUMENT, ERROR, OPENING_TAG, CLOSING_TAG, CHARACTERS };
private:
    File * const input_;
    unsigned line_no_;
    Type last_type_;
    std::string last_error_message_;
    bool last_element_was_empty_;
    std::string last_tag_name_;
public:
    SimpleXmlParser(File * const input): input_(input), line_no_(1), last_type_(UNINITIALISED), last_element_was_empty_(false) { }

    bool getNext(Type * const type, std::map<std::string, std::string> * const attrib_map, std::string * const data);

    const std::string &getLastErrorMessage() const { return last_error_message_; }
    unsigned getLineNo() const { return line_no_; }
    File *getInputFile() const { return input_; }

    static std::string TypeToString(const Type type);
private:
    void skipWhiteSpace();
    bool extractName(std::string * const name);
    bool extractQuotedString(const int closing_quote, std::string * const s);
    bool parseProlog();
    bool parseOpeningTag(std::string * const tag_name, std::map<std::string, std::string> * const attrib_map,
                         std::string * const error_message);
    bool parseClosingTag(std::string * const tag_name);
};


#endif // ifndef SIMPLE_XML_PARSER_H
