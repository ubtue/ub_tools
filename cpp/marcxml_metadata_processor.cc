/** \brief Tool for title, author and full-text extraction from a combination of of MARCXML metadata and a corresponding PDF file
 *         This is primarily intended for the conversion of the new Mohr-Siebeck publisher data including metadata
 *  \author Johannes Riedl
 *
 *  \copyright 2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MARC.h"
#include "PdfUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--min-log-level=min_verbosity] [--normalize only] [--force-ocr] marcxml_metadata [fulltext_pdf full_text_output]\n"
              << "       When specifying --normalise-only we only require the input filename!\n\n";
    std::exit(EXIT_FAILURE);
}


void ExtractTitle(const MARC::Record &record, std::string * const title) {
    *title = record.getCompleteTitle();
}


void ExtractAuthors(MARC::Record record, std::set<std::string> * const authors) {
    *authors = record.getAllAuthors();
}


void ExtractDOI(const MARC::Record &record, std::string * const doi) {
    const auto dois = record.getDOIs();
    if (dois.size() < 1) {
        LOG_WARNING("Could not extract DOI for title \"" + record.getCompleteTitle() + "\"");
        return;
    }
    if (dois.size() > 1)
        LOG_WARNING("Could not uniquely determine DOI for \"" + record.getCompleteTitle() + "\":" + " using arbitrary result");
    *doi = *dois.begin();
}


void ExtractYear(const MARC::Record &record, std::string * const year) {
    *year = record.getMostRecentPublicationYear();
}


void ExtractMetadata(MARC::Reader * const marc_reader, FullTextImport::FullTextData * const metadata) {
    auto record = marc_reader->read();
    ExtractTitle(record, &metadata->title_);
    ExtractAuthors(record, &metadata->authors_);
    ExtractDOI(record, &metadata->doi_);
    ExtractYear(record, &metadata->year_);
    if (marc_reader->read())
        LOG_ERROR("More than one record in " + marc_reader->getPath());
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


void ProcessDocument(const bool normalise_only, const bool force_ocr, MARC::Reader * const marc_reader, const std::string &pdf_file_path,
                     File * const plain_text_output) {
    FullTextImport::FullTextData full_text_metadata;
    ExtractMetadata(marc_reader, &full_text_metadata);
    full_text_metadata.full_text_location_ = pdf_file_path;

    if (normalise_only) {
        std::cout << ControlNumberGuesser::NormaliseTitle(full_text_metadata.title_) << '\n';
        for (const auto &article_author : full_text_metadata.authors_)
            std::cout << ControlNumberGuesser::NormaliseAuthorName(article_author) << '\n';
        return;
    }

    if (full_text_metadata.title_.empty())
        LOG_ERROR("no article title found in file '" + marc_reader->getPath() + "'");

    if (full_text_metadata.authors_.empty())
        LOG_ERROR("no article authors found in file '" + marc_reader->getPath() + "'");

    if (full_text_metadata.year_.empty())
        LOG_ERROR("no publication year found in file '" + marc_reader->getPath() + "'");

    if (full_text_metadata.doi_.empty())
        LOG_WARNING("no doi found in file '" + marc_reader->getPath() + "'");

    std::string full_text;
    if (not full_text_metadata.full_text_location_.empty())
        ExtractPDFFulltext(force_ocr, full_text_metadata.full_text_location_, &full_text);
    else
        LOG_ERROR("No fulltext location given");

    if (full_text.empty())
        LOG_ERROR("Could not extract fulltext for '" + full_text_metadata.full_text_location_ + "'");

    FullTextImport::WriteExtractedTextToDisk(full_text, full_text_metadata.title_, full_text_metadata.authors_, full_text_metadata.year_,
                                             full_text_metadata.doi_,
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

    if ((normalise_only and argc != 2) or argc != 4)
        Usage();

    const auto marc_reader(MARC::Reader::Factory(argv[1]));
    auto plain_text_output(normalise_only ? nullptr : FileUtil::OpenOutputFileOrDie(argv[3]));
    ProcessDocument(normalise_only, force_ocr, marc_reader.get(), argv[2], plain_text_output.get());

    return EXIT_SUCCESS;
}
