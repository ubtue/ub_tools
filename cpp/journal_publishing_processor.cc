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
#include "ControlNumberGuesser.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "util.h"
#include "XMLParser.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_verbosity] [--normalise-only] xml_input plain_test_output no_match_list\n"
              << "       \"no_match_list\" is the file that we append titles and author to for which we could not identify\n"
              << "       a corresponding control number for.  When specifying --normalise-only we only require the input filename!\n\n";
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


void ProcessDocument(const bool normalise_only, const std::string &input_filename, XMLParser * const xml_parser,
                     File * const plain_text_output, File * const no_match_list)
{
    std::string article_title;
    if (not ExtractTitle(xml_parser, &article_title))
        LOG_ERROR("no article title found!");

    std::vector<std::string> article_authors;
    std::string text_opening_tag;
    if (not ExtractAuthors(xml_parser, &article_authors, &text_opening_tag))
        LOG_ERROR("no article authors found or an error or end-of-document were found while trying to extract an author name!");

    if (normalise_only) {
        std::cout << ControlNumberGuesser::NormaliseTitle(article_title) << '\n';
        for (const auto &article_author : article_authors)
            std::cout << ControlNumberGuesser::NormaliseAuthorName(article_author) << '\n';
        return;
    }

    ControlNumberGuesser control_number_guesser(ControlNumberGuesser::DO_NOT_CLEAR_DATABASES);
    const auto matching_control_numbers(control_number_guesser.getGuessedControlNumbers(article_title, article_authors));
    if (matching_control_numbers.empty()) {
        (*no_match_list) << FileUtil::GetBasename(input_filename) << "\n\t" << article_title << "\n\t"
                         << StringUtil::Join(article_authors, "; ") << '\n';
        LOG_ERROR("no matching control numbers found!");
    }

    std::cout << "Matching control numbers:\n";
    for (const auto matching_control_number : matching_control_numbers)
        std::cout << '\t' << matching_control_number << '\n';

    std::string text;
    if (not ExtractText(xml_parser, text_opening_tag, &text))
        LOG_ERROR("no text found!");
    plain_text_output->write(text);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    bool normalise_only(false);
    if (std::strcmp(argv[1], "--normalise-only") == 0) {
        if (argc != 3)
            Usage();
        normalise_only = true;
    } else if (argc != 4)
        Usage();

    XMLParser xml_parser (argv[normalise_only ? 2 : 1], XMLParser::XML_FILE);
    auto plain_text_output(normalise_only ? nullptr : FileUtil::OpenOutputFileOrDie(argv[2]));
    auto no_match_list(normalise_only ? nullptr : FileUtil::OpenForAppendingOrDie(argv[3]));
    ProcessDocument(normalise_only, argv[normalise_only ? 2 : 1], &xml_parser, plain_text_output.get(), no_match_list.get());

    return EXIT_SUCCESS;
}
