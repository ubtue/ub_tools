/** \brief Tool for generating reasonable input for the FulltextImporter if only a PDF fulltext is available
 *  \author Johannes Riedl
 *
 *  \copyright 2019-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "HtmlUtil.h"
#include "PdfUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"

// Try to derive relevant information to guess the PPN
// Strategy 1: Try to find an ISBN string
// Strategy 2: Extract pages at the beginning and try to identify information at
//             the bottom of the first page and try to guess author and title


namespace {


[[noreturn]] void Usage() {
    ::Usage("pdf_input full_text_output | --output-dir=output_dir pdf_input1 pdf_input2 ...\n");
}


std::string GuessLastParagraph(const std::string &first_page_text) {
    const std::string first_page_text_trimmed(StringUtil::Trim(first_page_text, '\n'));
    const std::size_t last_paragraph_start(first_page_text_trimmed.rfind("\n\n"));
    std::string last_paragraph(last_paragraph_start != std::string::npos ? first_page_text_trimmed.substr(last_paragraph_start) : "");
    StringUtil::Map(&last_paragraph, '\n', ' ');
    return TextUtil::NormaliseDashes(&last_paragraph);
}


bool GuessISSN(const std::string &first_page_text, std::string * const issn) {
    const std::string last_paragraph(GuessLastParagraph(first_page_text));
    static RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactoryOrDie(".*ISSN\\s*([\\d\\-X]+).*"));
    if (matcher->matched(last_paragraph)) {
        *issn = (*matcher)[1];
        return true;
    }
    return false;
}


bool GuessDOI(const std::string &first_page_text, std::string * const doi) {
    const std::string last_paragraph(GuessLastParagraph(first_page_text));
    static RegexMatcher * const matcher(
        RegexMatcher::RegexMatcherFactoryOrDie(".*DOI[\\s:]*([\\d/.X]+).*", RegexMatcher::CASE_INSENSITIVE));
    if (matcher->matched(last_paragraph)) {
        *doi = (*matcher)[1];
        return true;
    }
    return false;
}


bool GuessISBN(const std::string &extracted_text, std::string * const isbn) {
    static RegexMatcher * const matcher(
        RegexMatcher::RegexMatcherFactoryOrDie(".*(?<!e-)ISBN\\s*([\\d\\-X]+).*", RegexMatcher::CASE_INSENSITIVE));
    if (matcher->matched(extracted_text)) {
        *isbn = (*matcher)[1];
        return true;
    }
    return false;
}


void GuessAuthorAndTitle(const std::string &pdf_document, FullTextImport::FullTextData * const fulltext_data) {
    std::string pdfinfo_output;
    PdfUtil::ExtractPDFInfo(pdf_document, &pdfinfo_output);
    static RegexMatcher * const authors_matcher(RegexMatcher::RegexMatcherFactoryOrDie("Author:\\s*(.*)", RegexMatcher::CASE_INSENSITIVE));
    std::vector<std::string> authors;
    if (authors_matcher->matched(pdfinfo_output)) {
        StringUtil::Split((*authors_matcher)[1], std::set<char>{ ';', '|' }, &authors);
        for (auto &author : authors)
            author = HtmlUtil::ReplaceEntitiesUTF8(author);
        std::copy(authors.cbegin(), authors.cend(), std::inserter(fulltext_data->authors_, fulltext_data->authors_.end()));
    }
    static RegexMatcher * const title_matcher(RegexMatcher::RegexMatcherFactoryOrDie("^Title:?\\s*(.*)", RegexMatcher::CASE_INSENSITIVE));
    if (title_matcher->matched(pdfinfo_output)) {
        std::string title_candidate((*title_matcher)[1]);
        // Try to detect invalid encoding
        if (not TextUtil::IsValidUTF8(title_candidate))
            LOG_WARNING("Apparently incorrect encoding for " + title_candidate);
        // Some cleanup
        title_candidate = StringUtil::ReplaceString("<ger>", "", title_candidate);
        title_candidate = StringUtil::ReplaceString("</ger>", "", title_candidate);
        title_candidate = HtmlUtil::ReplaceEntitiesUTF8(title_candidate);
        fulltext_data->title_ = title_candidate;
    }
}


void ConvertFulltextMetadataFromAssumedLatin1OriginalEncoding(FullTextImport::FullTextData * const fulltext_data) {
    std::string error_msg;
    static auto UTF8ToLatin1_converter(TextUtil::EncodingConverter::Factory("utf-8", "ISO-8859-1", &error_msg));
    UTF8ToLatin1_converter->convert(fulltext_data->title_, &(fulltext_data->title_));
}


bool GuessPDFMetadata(const std::string &pdf_document, FullTextImport::FullTextData * const fulltext_data) {
    ControlNumberGuesser control_number_guesser;
    // Try to find an ISBN in the first pages
    std::string first_pages_text;
    PdfUtil::ExtractText(pdf_document, &first_pages_text, "1", "10");
    std::string isbn;
    GuessISBN(TextUtil::NormaliseDashes(&first_pages_text), &isbn);
    if (not isbn.empty()) {
        fulltext_data->isbn_ = isbn;
        LOG_DEBUG("Extracted ISBN: " + isbn);
        std::set<std::string> control_numbers;
        control_number_guesser.lookupISBN(isbn, &control_numbers);
        if (control_numbers.size() != 1) {
            LOG_WARNING("We got more than one control number for ISBN \"" + isbn + "\"  - falling back to title and author");
            GuessAuthorAndTitle(pdf_document, fulltext_data);
            if (unlikely(not FullTextImport::CorrelateFullTextData(control_number_guesser, *(fulltext_data), &control_numbers))) {
                LOG_WARNING("Could not correlate full text data for ISBN \"" + isbn + "\"");
                return false;
            }
            if (control_numbers.size() != 1)
                LOG_WARNING("Unable to determine unique control number for ISBN \"" + isbn
                            + "\": " + StringUtil::Join(control_numbers, ", "));
        }
        const std::string control_number(*(control_numbers.begin()));
        LOG_DEBUG("Determined control number \"" + control_number + "\" for ISBN \"" + isbn + "\"\n");
        return true;
    }

    // Guess control number by DOI, author, title and and possibly ISSN
    std::string first_page_text;
    PdfUtil::ExtractText(pdf_document, &first_page_text, "1", "1"); // Get only first page
    std::string control_number;
    if (GuessDOI(first_page_text, &(fulltext_data->doi_))
        and FullTextImport::CorrelateFullTextData(control_number_guesser, *(fulltext_data), &control_number))
        return true;
    GuessISSN(first_page_text, &(fulltext_data->issn_));
    GuessAuthorAndTitle(pdf_document, fulltext_data);
    if (not FullTextImport::CorrelateFullTextData(control_number_guesser, *(fulltext_data), &control_number))
        // We frequently have the case that author and title extracted we encoded in Latin-1 in some time in the past such that our search
        // fails so force normalisation and make another attempt.
        ConvertFulltextMetadataFromAssumedLatin1OriginalEncoding(fulltext_data);
    return FullTextImport::CorrelateFullTextData(control_number_guesser, *(fulltext_data), &control_number);
}


void CreateFulltextImportFile(const std::string &fulltext_pdf, const std::string &fulltext_txt) {
    std::cout << "Processing \"" << fulltext_pdf << "\"\n";
    FullTextImport::FullTextData fulltext_data;
    std::string pdf_document;
    if (not FileUtil::ReadString(fulltext_pdf, &pdf_document))
        LOG_ERROR("Could not read \"" + fulltext_pdf + "\"");
    if (PdfUtil::PdfDocContainsNoText(pdf_document))
        LOG_ERROR("Apparently no text in \"" + fulltext_pdf + "\"");
    if (unlikely(not GuessPDFMetadata(pdf_document, &fulltext_data)))
        LOG_ERROR("Unable to determine metadata for \"" + fulltext_pdf + "\"");
    if (unlikely(not PdfUtil::ExtractText(pdf_document, &(fulltext_data.full_text_))))
        LOG_ERROR("Unable to extract fulltext from \"" + fulltext_pdf + "\"");
    auto plain_text_output(FileUtil::OpenOutputFileOrDie(fulltext_txt));
    fulltext_data.full_text_location_ = FileUtil::MakeAbsolutePath(fulltext_pdf);
    FullTextImport::WriteExtractedTextToDisk(fulltext_data.full_text_, fulltext_data.title_, fulltext_data.authors_, fulltext_data.year_,
                                             fulltext_data.doi_, fulltext_data.issn_, fulltext_data.isbn_, fulltext_data.text_type_,
                                             fulltext_data.full_text_location_, plain_text_output.get());
}


std::string DeriveOutputFilename(const std::string &pdf_filename) {
    return (StringUtil::ASCIIToLower(FileUtil::GetExtension(pdf_filename)) == "pdf")
               ? FileUtil::GetFilenameWithoutExtensionOrDie(pdf_filename) + ".txt"
               : pdf_filename + ".txt";
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    if (argc < 3)
        Usage();

    bool use_output_dir(false);
    std::string output_dir(".");
    if (argc > 2 and StringUtil::StartsWith(argv[1], "--output-dir=")) {
        use_output_dir = true;
        output_dir = argv[1] + __builtin_strlen("--output-dir=");
        --argc;
        ++argv;
    }

    if (argc < 2)
        Usage();

    if (not use_output_dir and argc < 3)
        Usage();

    if (not use_output_dir) {
        const std::string fulltext_pdf(argv[1]);
        CreateFulltextImportFile(fulltext_pdf, argv[2]);
        return EXIT_SUCCESS;
    }

    for (int arg_no(1); arg_no < argc; ++arg_no)
        CreateFulltextImportFile(argv[arg_no], output_dir + '/' + DeriveOutputFilename(argv[arg_no]));
    return EXIT_SUCCESS;
}
