/** \brief Tool for title, author and full-text extraction from XMl files corresponding to the Journal Publishing DTD.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2018,2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "FileUtil.h"
#include "FullTextImport.h"
#include "PdfUtil.h"
#include "StringUtil.h"
#include "XMLParser.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname << " [--min-log-level=min_verbosity] [--normalise-only|--force-ocr] xml_input full_text_output\n"
              << "       When specifying --normalise-only we only require the input filename!\n\n";
    std::exit(EXIT_FAILURE);
}


std::string ReadCharactersUntilNextClosingTag(XMLParser * const xml_parser) {
    XMLParser::XMLPart xml_part;
    std::string extracted_data;

    while (xml_parser->getNext(&xml_part)) {
        if (xml_part.isClosingTag())
            break;
        else if (xml_part.type_ == XMLParser::XMLPart::CHARACTERS)
            extracted_data += xml_part.data_;
    }

    return extracted_data;
}


void ExtractAuthor(XMLParser * const xml_parser, std::set<std::string> * const article_authors) {
    if (not xml_parser->skipTo(XMLParser::XMLPart::OPENING_TAG, "surname"))
        return;

    XMLParser::XMLPart xml_part;
    if (not xml_parser->getNext(&xml_part) or xml_part.type_ != XMLParser::XMLPart::CHARACTERS)
        return;
    const std::string surname(xml_part.data_);

    while (xml_parser->getNext(&xml_part)) {
        if (xml_part.type_ == XMLParser::XMLPart::CLOSING_TAG and xml_part.data_ == "contrib") {
            article_authors->insert(surname);
            return;
        } else if (xml_part.type_ == XMLParser::XMLPart::OPENING_TAG and xml_part.data_ == "given-names") {
            if (not xml_parser->getNext(&xml_part) or xml_part.type_ != XMLParser::XMLPart::CHARACTERS)
                return;
            article_authors->insert(xml_part.data_ + " " + surname);
            return;
        }
    }
}


void ExtractMetadata(XMLParser * const xml_parser, FullTextImport::FullTextData * const metadata) {
    XMLParser::XMLPart xml_part;

    while (xml_parser->getNext(&xml_part, std::set<std::string>{ "body" })) {
        if (xml_part.isOpeningTag("article-title"))
            metadata->title_ = ReadCharactersUntilNextClosingTag(xml_parser);
        else if (xml_part.isOpeningTag("contrib")) {
            const auto contrib_type_and_value(xml_part.attributes_.find("contrib-type"));
            if (contrib_type_and_value != xml_part.attributes_.cend() and contrib_type_and_value->second == "author")
                ExtractAuthor(xml_parser, &metadata->authors_);
        } else if (xml_part.isOpeningTag("pub-date")) {
            if (xml_parser->skipTo(XMLParser::XMLPart::OPENING_TAG, "year"))
                metadata->year_ = ReadCharactersUntilNextClosingTag(xml_parser);
        } else if (xml_part.isOpeningTag("article-id")) {
            const auto id_type_and_value(xml_part.attributes_.find("pub-id-type"));
            if (id_type_and_value != xml_part.attributes_.cend() and id_type_and_value->second == "doi")
                metadata->doi_ = ReadCharactersUntilNextClosingTag(xml_parser);
        } else if (xml_part.isOpeningTag("self-uri")) {
            const auto fulltext_location(xml_part.attributes_.find("xlink:href"));
            if (fulltext_location != xml_part.attributes_.cend())
                metadata->full_text_location_ = fulltext_location->second;
        }
    }
}


bool ExtractText(XMLParser * const xml_parser, const std::string &text_opening_tag, std::string * const text) {
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


void ExtractPDFFulltext(const bool force_ocr, const std::string &fulltext_location, std::string * const full_text) {
    if (not StringUtil::EndsWith(fulltext_location, ".pdf", true /* ignore case */))
        LOG_ERROR("Don't know how to handle file \"" + fulltext_location + "\"");
    std::string pdf_document;
    if (not FileUtil::ReadString(fulltext_location, &pdf_document))
        LOG_ERROR("Could not read \"" + fulltext_location + "\"");
    if (not force_ocr) {
        if (not PdfUtil::PdfDocContainsNoText(pdf_document))
            PdfUtil::ExtractText(pdf_document, full_text);
        else
            LOG_ERROR("Apparently no text in \"" + fulltext_location + "\"");
    } else {
        if (not PdfUtil::GetOCRedTextFromPDF(fulltext_location, "eng+grc+heb", full_text, 120))
            LOG_ERROR("Could not extract text from \"" + fulltext_location + "\"");
    }
}


void ProcessDocument(const bool normalise_only, const bool force_ocr, const std::string &input_file_path, XMLParser * const xml_parser,
                     File * const plain_text_output) {
    FullTextImport::FullTextData full_text_metadata;
    ExtractMetadata(xml_parser, &full_text_metadata);

    if (normalise_only) {
        std::cout << ControlNumberGuesser::NormaliseTitle(full_text_metadata.title_) << '\n';
        for (const auto &article_author : full_text_metadata.authors_)
            std::cout << ControlNumberGuesser::NormaliseAuthorName(article_author) << '\n';
        return;
    }

    if (full_text_metadata.title_.empty())
        LOG_ERROR("no article title found in file '" + input_file_path + "'");

    if (full_text_metadata.authors_.empty())
        LOG_ERROR("no article authors found in file '" + input_file_path + "'");

    if (full_text_metadata.year_.empty())
        LOG_ERROR("no publication year found in file '" + input_file_path + "'");

    if (full_text_metadata.doi_.empty())
        LOG_WARNING("no doi found in file '" + input_file_path + "'");

    std::string full_text, abstract;

    if (not ExtractText(xml_parser, "body", &full_text)) {
        if (not full_text_metadata.full_text_location_.empty())
            ExtractPDFFulltext(force_ocr, full_text_metadata.full_text_location_, &full_text);
        else
            ExtractText(xml_parser, "abstract", &abstract);
    }

    if (full_text.empty())
        LOG_WARNING("Could not extract fulltext for '" + input_file_path + "'");

    if (full_text.empty() and abstract.empty())
        LOG_ERROR("neither full-text nor abstract text was found in file '" + input_file_path + "'");

    FullTextImport::WriteExtractedTextToDisk(not full_text.empty() ? full_text : abstract, full_text_metadata.title_,
                                             full_text_metadata.authors_, full_text_metadata.year_, full_text_metadata.doi_,
                                             /* ISSN */ "", /* ISBN */ "", full_text_metadata.text_type_,
                                             FileUtil::MakeAbsolutePath(full_text_metadata.full_text_location_), plain_text_output);
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    bool normalise_only(false);
    bool force_ocr(false);
    if (std::strcmp(argv[1], "--normalise-only") == 0) {
        normalise_only = true;
        --argc, ++argv;
    } else if (std::strcmp(argv[1], "--force-ocr") == 0) {
        force_ocr = true;
        --argc, ++argv;
    }


    if ((normalise_only and argc != 2) or (not normalise_only and not force_ocr and argc != 3))
        Usage();

    XMLParser xml_parser(argv[1], XMLParser::XML_FILE);
    auto plain_text_output(normalise_only ? nullptr : FileUtil::OpenOutputFileOrDie(argv[2]));
    ProcessDocument(normalise_only, force_ocr, argv[1], &xml_parser, plain_text_output.get());

    return EXIT_SUCCESS;
}
