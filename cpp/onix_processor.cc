/** \brief Tool for title, author and full-text extraction from XMl files corresponding to the ONIX XML format.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "ControlNumberGuesser.h"
#include "FileUtil.h"
#include "FullTextImport.h"
#include "MapUtil.h"
#include "ONIX.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UBTools.h"
#include "XMLParser.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--min-log-level=min_verbosity] [--normalise-only] [--full-text-encoding=encoding] xml_input full_text_output\n"
              << "       When specifying --normalise-only we only require the input filename!\n\n";
    std::exit(EXIT_FAILURE);
}


void ExtractMetadata(XMLParser * const xml_parser, FullTextImport::FullTextData * const metadata) {
    XMLParser::XMLPart xml_part;

    bool in_series(false);
    while (xml_parser->getNext(&xml_part)) {
        if (xml_part.isOpeningTag("ProductIdentifier")) {
            std::string product_id_type;
            if (xml_parser->extractTextBetweenTags("ProductIDType", &product_id_type)) {
                if (StringUtil::ToUnsigned(product_id_type) == static_cast<unsigned>(ONIX::ProductIDType::DOI)) {
                    std::string doi;
                    xml_parser->extractTextBetweenTags("IDValue", &doi);
                    metadata->doi_ = doi;
                } else if (StringUtil::ToUnsigned(product_id_type) == static_cast<unsigned>(ONIX::ProductIDType::ISBN_13)) {
                    std::string isbn;
                    xml_parser->extractTextBetweenTags("IDValue", &isbn);
                    metadata->isbn_ = isbn;
                }
            }
        } else if (xml_part.isOpeningTag("Contributor")) {
            std::string contributer_role;
            xml_parser->extractTextBetweenTags("ContributorRole", &contributer_role);
            if (contributer_role == "A01" or contributer_role == "B01") {
                std::string author;
                xml_parser->extractTextBetweenTags("PersonName", &author);
                metadata->authors_.emplace(author);
            }
        } else if (xml_part.isOpeningTag("Series"))
            in_series = true;
        else if (xml_part.isClosingTag("Series"))
            in_series = false;
        else if (not in_series and (xml_part.isOpeningTag("Title") or xml_part.isOpeningTag("TitleElement"))) {
            std::string title_type;
            xml_parser->extractTextBetweenTags("TitleType", &title_type);
            if (StringUtil::ToUnsigned(title_type) == static_cast<unsigned>(ONIX::TitleType::DISTINCTIVE_TITLE)) {
                std::string title_text;
                xml_parser->extractTextBetweenTags("TitleText", &title_text);
                metadata->title_ = title_text;
            }
        } else if (xml_part.isOpeningTag("YearOfAnnual")) {
            if (unlikely(not xml_parser->getNext(&xml_part) or xml_part.type_ != XMLParser::XMLPart::CHARACTERS))
                LOG_ERROR("unexpected end-of-input or missing YearOfAnnual!");
            metadata->year_ = xml_part.data_;
        }
    }

    std::cerr << metadata->toString('\n');
}


void ProcessDocument(const bool normalise_only, const std::string &input_file_path, const std::string &full_text_encoding,
                     XMLParser * const xml_parser, File * const plain_text_output) {
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
        LOG_WARNING("no publication year found in file '" + input_file_path + "'");

    if (full_text_metadata.doi_.empty())
        LOG_WARNING("no doi found in file '" + input_file_path + "'");

    if (full_text_metadata.isbn_.empty())
        LOG_ERROR("missing ISBN!");

    const auto directory_prefix(FileUtil::GetDirname(xml_parser->getXmlFilenameOrString()));
    const std::string full_text_filename((directory_prefix.empty() ? "." : directory_prefix) + "/" + full_text_metadata.isbn_ + ".txt");

    std::string full_text;
    FileUtil::ReadStringOrDie(full_text_filename, &full_text);

    if (not full_text_encoding.empty()) {
        std::string utf8_full_text;
        if (not TextUtil::ConvertToUTF8(full_text_encoding, full_text, &utf8_full_text))
            LOG_ERROR("failed to convert the contents of \"" + full_text_filename + "\" from \"" + full_text_encoding + " to UTF-8!");
        utf8_full_text.swap(full_text);
    }

    FullTextImport::WriteExtractedTextToDisk(full_text, full_text_metadata.title_, full_text_metadata.authors_, full_text_metadata.year_,
                                             full_text_metadata.doi_, full_text_metadata.issn_, full_text_metadata.isbn_,
                                             full_text_metadata.text_type_, "" /* full_text_location currently not used */,
                                             plain_text_output);
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

    std::string full_text_encoding;
    if (StringUtil::StartsWith(argv[1], "--full-text-encoding=")) {
        full_text_encoding = argv[1] + __builtin_strlen("--full-text-encoding=");
        ++argc, ++argv;
    }

    if (argc != 3)
        Usage();

    std::unordered_map<std::string, std::string> onix_short_tags_to_reference_map;
    MapUtil::DeserialiseMap(UBTools::GetTuelibPath() + "onix_reference_to_short_tags.map", &onix_short_tags_to_reference_map,
                            /* revert_keys_and_values = */ true);

    XMLParser xml_parser(argv[1], XMLParser::XML_FILE);
    xml_parser.setTagAliases(onix_short_tags_to_reference_map);
    const auto plain_text_output(normalise_only ? nullptr : FileUtil::OpenOutputFileOrDie(argv[2]));

    unsigned count(0);
    for (;;) {
        std::string record_reference;
        if (not xml_parser.extractTextBetweenTags("RecordReference", &record_reference))
            break;
        LOG_DEBUG("record_reference = " + record_reference);

        std::string notification_type;
        if (not xml_parser.extractTextBetweenTags("NotificationType", &notification_type))
            LOG_ERROR("missing NotificationType after RecordReference \"" + record_reference + "\"!");
        LOG_DEBUG("notification_type = " + notification_type);

        if (unlikely(notification_type == "05")) {
            if (not xml_parser.skipTo(XMLParser::XMLPart::CLOSING_TAG, "Product"))
                break;
            continue;
        }

        ProcessDocument(normalise_only, argv[1], full_text_encoding, &xml_parser, plain_text_output.get());
        ++count;
    }

    LOG_INFO("Processed " + std::to_string(count) + " relevant record(s).");

    return EXIT_SUCCESS;
}
