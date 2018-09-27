/** \brief Tool for title, author and full-text extraction from XMl files corresponding to the Journal Publishing DTD.
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

#include <iostream>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "FileUtil.h"
#include "util.h"
#include "XMLParser.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " xml_input plain_test_output\n";
    std::exit(EXIT_FAILURE);
}


bool ExtractTitle(XMLParser * const xml_parser, std::string * const article_title) {
    article_title->clear();

    if (not xml_parser->skipTo(XMLParser::XMLPart::OPENING_TAG, "article-title"))
        return false;

    XMLParser::XMLPart xml_part;
    while (xml_parser->getNext(&xml_part)) {
        if (xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG and xml_part.data_ == "article-title")
            return not article_title->empty();
        if (xml_part.type_ == XMLParser::XMLPart::CHARACTERS)
            *article_title += xml_part.data_;
    }

    return false;
}


bool ExtractAuthor(XMLParser * const xml_parser, std::vector<std::string> * const article_authors) {
    if (not xml_parser->skipTo(XMLParser::XMLPart::OPENING_TAG, "surname"))
        return false;

    XMLParser::XMLPart xml_part;
    if (not xml_parser->getNext(&xml_part) or xml_part.type_ != XMLParser::XMLPart::CHARACTERS)
        return false;
    std::string surname(xml_part.data_);

    while (xml_parser->getNext(&xml_part)) {
        if (xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG and xml_part.data_ == "contrib") {
            article_authors->emplace_back(surname);
            return true;
        } else if (xml_part.type_ == XMLParser::XMLPart::OPENING_TAG and xml_part.data_ == "given-names") {
            if (not xml_parser->getNext(&xml_part) or xml_part.type_ != XMLParser::XMLPart::CHARACTERS)
                return false;
            article_authors->emplace_back(xml_part.data_ + " " + surname);
            return true;
        }
    }

    return false;
}


bool ExtractAuthors(XMLParser * const xml_parser, std::vector<std::string> * const article_authors, std::string * const text_opening_tag) {
    XMLParser::XMLPart xml_part;
    while (xml_parser->getNext(&xml_part)) {
        if (xml_part.type_ == XMLParser::XMLPart::OPENING_TAG) {
            if (xml_part.data_ == "abstract" or xml_part.data_ == "body") {
                *text_opening_tag = xml_part.data_;
                return true;
            }
            else if (xml_part.data_ == "contrib") {
                const auto contrib_type_and_value(xml_part.attributes_.find("contrib-type"));
                if (contrib_type_and_value != xml_part.attributes_.cend() and contrib_type_and_value->second == "author") {
                    if (not ExtractAuthor(xml_parser, article_authors))
                        return false;
                }
            }
        }
    }

    return false;
}


bool ExtractTextHelper(XMLParser * const xml_parser, const std::string &closing_tag, std::string * const text) {
    XMLParser::XMLPart xml_part;
    while (xml_parser->getNext(&xml_part)) {
        if (xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG and xml_part.data_ == closing_tag)
            return true;
        if (xml_part.type_ == XMLParser::XMLPart::CHARACTERS)
            *text += xml_part.data_;
    }

    return false;
}


// Extracts abstracts and bodies.
bool ExtractText(XMLParser * const xml_parser, const std::string &text_opening_tag, std::string * const text) {
    if (not ExtractTextHelper(xml_parser, text_opening_tag, text))
        return false;
    *text += '\n';

    if (text_opening_tag == "body")
        return true;

    if (xml_parser->skipTo(XMLParser::XMLPart::OPENING_TAG, "body")) {
        if (not ExtractTextHelper(xml_parser, "body", text))
            return false;
        *text += '\n';
    }

    return not text->empty();
}


void ProcessDocument(XMLParser * const xml_parser, File * const plain_text_output) {
    std::string article_title;
    if (not ExtractTitle(xml_parser, &article_title))
        LOG_ERROR("no article title found!");
    std::cout << "Article title is " << article_title << '\n';

    std::vector<std::string> article_authors;
    std::string text_opening_tag;
    if (not ExtractAuthors(xml_parser, &article_authors, &text_opening_tag))
        LOG_ERROR("no article authors found or an error or end-of-docuemnt were found while trying to extract an author name!");
    std::cout << "Article authors are:\n";
    for (const auto &author : article_authors)
        std::cout << '\t' << author << '\n';

    std::string text;
    if (not ExtractText(xml_parser, text_opening_tag, &text))
        LOG_ERROR("no text found!");
    plain_text_output->write(text);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc != 3)
        Usage();

    XMLParser xml_parser (argv[1], XMLParser::XML_FILE);
    auto plain_text_output(FileUtil::OpenOutputFileOrDie(argv[2]));
    ProcessDocument(&xml_parser, plain_text_output.get());

    return EXIT_SUCCESS;
}
