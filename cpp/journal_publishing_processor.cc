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
#include "FullTextImport.h"
#include "StringUtil.h"
#include "util.h"
#include "XMLParser.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_verbosity] [--normalise-only] xml_input full_text_output\n"
              << "       When specifying --normalise-only we only require the input filename!\n\n";
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


bool ExtractAuthor(XMLParser * const xml_parser, std::set<std::string> * const article_authors) {
    if (not xml_parser->skipTo(XMLParser::XMLPart::OPENING_TAG, "surname"))
        return false;

    XMLParser::XMLPart xml_part;
    if (not xml_parser->getNext(&xml_part) or xml_part.type_ != XMLParser::XMLPart::CHARACTERS)
        return false;
    std::string surname(xml_part.data_);

    while (xml_parser->getNext(&xml_part)) {
        if (xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG and xml_part.data_ == "contrib") {
            article_authors->insert(surname);
            return true;
        } else if (xml_part.type_ == XMLParser::XMLPart::OPENING_TAG and xml_part.data_ == "given-names") {
            if (not xml_parser->getNext(&xml_part) or xml_part.type_ != XMLParser::XMLPart::CHARACTERS)
                return false;
            article_authors->insert(xml_part.data_ + " " + surname);
            return true;
        }
    }

    return false;
}


bool ExtractAuthors(XMLParser * const xml_parser, std::set<std::string> * const article_authors) {
    XMLParser::XMLPart xml_part;
    while (xml_parser->getNext(&xml_part)) {
        if (xml_part.type_ == XMLParser::XMLPart::OPENING_TAG) {
            if (xml_part.data_ == "abstract" or xml_part.data_ == "body")
                return true;
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


// Extracts abstracts and bodies.
bool ExtractText(XMLParser * const xml_parser, const std::string &text_opening_tag, std::string * const text) {
    xml_parser->rewind();

    XMLParser::XMLPart xml_part;
    if (not xml_parser->skipTo(XMLParser::XMLPart::OPENING_TAG, { text_opening_tag }, &xml_part))
        return false;

    do {
        if (xml_part.isClosingTag(text_opening_tag))
            break;

        // format the text as it's read in
        if (xml_part.isClosingTag("sec"))
            *text += FullTextImport::CHUNK_DELIMITER;
        else if (xml_part.isClosingTag("label"))
            *text += ": ";
        else if (xml_part.isClosingTag("title") or xml_part.isClosingTag("p"))
            *text += FullTextImport::PARAGRAPH_DELIMITER;
        else if (xml_part.isCharacters())
            *text += xml_part.data_;

    } while (xml_parser->getNext(&xml_part));

    return not text->empty();
}


void ProcessDocument(const bool normalise_only, XMLParser * const xml_parser, File * const plain_text_output) {
    std::string article_title;
    if (not ExtractTitle(xml_parser, &article_title))
        LOG_ERROR("no article title found!");

    std::set<std::string> article_authors;
    if (not ExtractAuthors(xml_parser, &article_authors))
        LOG_ERROR("no article authors found or an error or end-of-document were found while trying to extract an author name!");

    if (normalise_only) {
        std::cout << ControlNumberGuesser::NormaliseTitle(article_title) << '\n';
        for (const auto &article_author : article_authors)
            std::cout << ControlNumberGuesser::NormaliseAuthorName(article_author) << '\n';
        return;
    }

    std::string full_text, abstract;
    ExtractText(xml_parser, "body", &full_text);
    ExtractText(xml_parser, "abstract", &abstract);

    if (full_text.empty() and abstract.empty())
        LOG_ERROR("Neither full-text nor abstract text was found");

    FullTextImport::WriteExtractedTextToDisk(not full_text.empty() ? full_text : abstract, article_title, article_authors, plain_text_output);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    bool normalise_only(false);
    if (std::strcmp(argv[1], "--normalise-only") == 0) {
        normalise_only = true;
        ++argc, ++argv;
    }

    if (argc != 3)
        Usage();

    XMLParser xml_parser (argv[1], XMLParser::XML_FILE);
    auto plain_text_output(normalise_only ? nullptr : FileUtil::OpenOutputFileOrDie(argv[2]));
    ProcessDocument(normalise_only, &xml_parser, plain_text_output.get());

    return EXIT_SUCCESS;
}
