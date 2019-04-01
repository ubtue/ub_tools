/** \brief Utility for augmenting MARC records with links to a local full-text database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2019 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <memory>
#include <vector>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "Compiler.h"
#include "FullTextCache.h"
#include "MARC.h"
#include "MediaTypeUtil.h"
#include "OCR.h"
#include "PdfUtil.h"
#include "Semaphore.h"
#include "SmartDownloader.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


namespace {


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " [--pdf-extraction-timeout=timeout]file_offset marc_input marc_output\n"
              << "       \"--pdf-extraction-timeout\" timeout in seconds (default " << PdfUtil::DEFAULT_PDF_EXTRACTION_TIMEOUT << ").\n"
              << "       \"file_offset\" Where to start reading a MARC data set from in marc_input.\n\n";
    std::exit(EXIT_FAILURE);
}


// \note Sets "error_message" when it returns false.
bool GetDocumentAndMediaType(const std::string &url, const unsigned timeout, std::string * const document,
                             std::string * const media_type, std::string * const media_subtype,
                             std::string * const http_header_charset, std::string * const error_message)
{
    if (not SmartDownload(url, timeout, document, http_header_charset, error_message))
        return false;

    *media_type = MediaTypeUtil::GetMediaType(*document, media_subtype);
    if (media_type->empty()) {
        *error_message = "Failed to get media type";
        return false;
    }

    return true;
}


static const std::map<std::string, std::string> marc_to_tesseract_language_codes_map {
    { "bul", "bul" },
    { "cze", "ces" },
    { "dan", "dan" },
    { "dut", "nld" },
    { "eng", "eng" },
    { "fin", "fin" },
    { "fre", "fra" },
    { "ger", "deu" },
    { "grc", "grc" },
    { "heb", "heb" },
    { "hun", "hun" },
    { "ita", "ita" },
    { "lat", "lat" },
    { "nor", "nor" },
    { "pol", "pol" },
    { "por", "por" },
    { "rus", "rus" },
    { "slv", "slv" },
    { "spa", "spa" },
    { "swe", "swe" },
};


std::string GetTesseractLanguageCode(const MARC::Record &record) {
    const auto map_iter(marc_to_tesseract_language_codes_map.find(MARC::GetLanguageCode(record)));
    return (map_iter == marc_to_tesseract_language_codes_map.cend()) ? "" : map_iter->second;
}


// Checks subfields "3" and "z" to see if they start w/ "Rezension".
bool IsProbablyAReview(const MARC::Subfields &subfields) {
    const std::vector<std::string> _3_subfields(subfields.extractSubfields('3'));
    if (not _3_subfields.empty()) {
        for (const auto &subfield_value : _3_subfields) {
            if (StringUtil::StartsWith(subfield_value, "Rezension"))
                return true;
        }
    } else {
        const std::vector<std::string> z_subfields(subfields.extractSubfields('z'));
        for (const auto &subfield_value : z_subfields) {
            if (StringUtil::StartsWith(subfield_value, "Rezension"))
                return true;
        }
    }

    return false;
}


// \return The concatenated contents of all 520$a subfields.
std::string GetTextFrom520a(const MARC::Record &record) {
    std::string concatenated_text;

    for (const auto &field : record.getTagRange("520")) {
        const MARC::Subfields subfields(field.getSubfields());
        if (subfields.hasSubfield('a')) {
            if (not concatenated_text.empty())
                concatenated_text += ' ';
            concatenated_text += subfields.getFirstSubfieldWithCode('a');
        }
    }

    return concatenated_text;
}


bool IsUTF8(const std::string &charset) {
    return charset == "utf-8" or charset == "utf8" or charset == "UFT-8" or charset == "UTF8";
}


std::string ConvertToPlainText(const std::string &media_type, const std::string &media_subtype, const std::string &http_header_charset,
                               const std::string &tesseract_language_code, const std::string &document,
                               const unsigned pdf_extraction_timeout, std::string * const error_message)
{
    std::string extracted_text;
    if (media_type == "text/html" or media_type == "text/xhtml") {
        extracted_text = TextUtil::ExtractTextFromHtml(document, http_header_charset);
        return TextUtil::CollapseWhitespace(&extracted_text);
    }

    if (media_type == "text/xml" and media_subtype == "tei") {
        extracted_text = TextUtil::ExtractTextFromUBTei(document);
        return TextUtil::CollapseWhitespace(&extracted_text);
    }

    if (StringUtil::StartsWith(media_type, "text/")) {
        if (not (media_type == "text/plain"))
            LOG_WARNING("treating " + media_type + " as text/plain");

        if (IsUTF8(http_header_charset))
            return document;

        std::string error_msg;
        std::unique_ptr<TextUtil::EncodingConverter> encoding_converter(TextUtil::EncodingConverter::Factory(http_header_charset,
                                                                                                             "utf8", &error_msg));
        if (encoding_converter.get() == nullptr) {
            LOG_WARNING("can't convert from \"" + http_header_charset + "\" to UTF-8! (" + error_msg + ")");
            return document;
        }

        std::string utf8_document;
        if (unlikely(not encoding_converter->convert(document, &utf8_document)))
            LOG_WARNING("conversion error while converting text from \"" + http_header_charset + "\" to UTF-8!");
        return TextUtil::CollapseWhitespace(&utf8_document);
    }

    if (StringUtil::StartsWith(media_type, "application/pdf")) {
        if (PdfUtil::PdfDocContainsNoText(document)) {
            if (not PdfUtil::GetTextFromImagePDF(document, tesseract_language_code, &extracted_text, pdf_extraction_timeout)) {
                *error_message = "Failed to extract text from an image PDF!";
                LOG_WARNING(*error_message);
                return "";
            }
            return TextUtil::CollapseWhitespace(&extracted_text);
        }
        PdfUtil::ExtractText(document, &extracted_text);
        return TextUtil::CollapseWhitespace(&extracted_text);
    }

    if (media_type == "image/jpeg" or media_type == "image/png") {
        if (OCR(document, &extracted_text, tesseract_language_code) != 0) {
            *error_message = "Failed to extract text by using OCR on " + media_type;
            LOG_WARNING(*error_message);
            return "";
        }
        return TextUtil::CollapseWhitespace(&extracted_text);
    }

    *error_message = "Don't know how to handle media type: " + media_type;
    LOG_WARNING(*error_message);
    return "";
}


bool ProcessRecordUrls(MARC::Record * const record, const unsigned pdf_extraction_timeout) {
    const std::string ppn(record->getControlNumber());
    std::vector<std::string> urls;

    // Get URL's:
    for (const auto _856_field : record->getTagRange("856")) {
        const MARC::Subfields _856_subfields(_856_field.getSubfields());

        if (_856_field.getIndicator1() == '7' or not _856_subfields.hasSubfield('u'))
            continue;

        if (IsProbablyAReview(_856_subfields))
            continue;

        urls.emplace_back(_856_subfields.getFirstSubfieldWithCode('u'));
    }

    // Get or create cache entry
    FullTextCache cache;
    std::string combined_text_final;
    bool success(false);
    if (not cache.entryExpired(ppn, urls)) {
        cache.getFullText(ppn, &combined_text_final);
        Semaphore semaphore("/full_text_cached_counter", Semaphore::ATTACH);
        ++semaphore;
        success = true;
    } else {
        FullTextCache::Entry entry;
        std::vector<FullTextCache::EntryUrl> entry_urls;
        std::string combined_text(GetTextFrom520a(*record));
        constexpr unsigned PER_DOC_TIMEOUT(30000); // in milliseconds
        bool at_least_one_error(false);

        for (const auto &url : urls) {
            FullTextCache::EntryUrl entry_url;
            entry_url.id_ = ppn;
            entry_url.url_ = url;
            std::string domain;
            cache.getDomainFromUrl(url, &domain);
            entry_url.domain_ = domain;
            std::string document, media_type, media_subtype, http_header_charset, error_message;
            if ((not GetDocumentAndMediaType(url, PER_DOC_TIMEOUT, &document, &media_type, &media_subtype, &http_header_charset,
                                             &error_message))) {
                LOG_WARNING("URL " + url + ": could not get document and media type! (" + error_message + ")");
                entry_url.error_message_ = "could not get document and media type! (" + error_message + ")";
            } else {
                std::string extracted_text(ConvertToPlainText(media_type, media_subtype, http_header_charset, GetTesseractLanguageCode(*record),
                                                              document, pdf_extraction_timeout, &error_message));

                if (unlikely(extracted_text.empty())) {
                    LOG_WARNING("URL " + url + ": failed to extract text from the downloaded document! (" + error_message + ")");
                    entry_url.error_message_ = "failed to extract text from the downloaded document! (" + error_message + ")";
                } else {
                    if (combined_text.empty())
                        combined_text.swap(extracted_text);
                    else
                        combined_text += " " + extracted_text;
                }
            }
            at_least_one_error = at_least_one_error ? at_least_one_error : not entry_url.error_message_.empty();
            entry_urls.push_back(entry_url);
        }

        success = not at_least_one_error && not urls.empty();


        combined_text_final = TextUtil::CollapseAndTrimWhitespace(&combined_text);
        cache.insertEntry(ppn, combined_text_final, entry_urls);
    }

    if (not combined_text_final.empty())
        record->insertField("FUL", { { 'e', "http://localhost/cgi-bin/full_text_lookup?id=" + ppn } });

    return success;
}


bool ProcessRecord(MARC::Record * const record, const std::string &marc_output_filename, const unsigned pdf_extraction_timeout) {
    bool success(false);
    try {
        success = ProcessRecordUrls(record, pdf_extraction_timeout);
    } catch (const std::exception &x) {
        LOG_WARNING("caught exception: " + std::string(x.what()));
    }

    // Safely append the modified MARC data to the MARC output file:
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename, MARC::FileType::BINARY,
                                                                    MARC::Writer::APPEND));
    MARC::FileLockedComposeAndWriteRecord(marc_writer.get(), *record);

    return success;
}


// Returns true if text has been successfully extracted, else false.
bool ProcessRecord(MARC::Reader * const marc_reader, const std::string &marc_output_filename, const unsigned pdf_extraction_timeout) {
    MARC::Record record(marc_reader->read());
    try {
        LOG_INFO("processing record " + record.getControlNumber());
        return ProcessRecord(&record, marc_output_filename, pdf_extraction_timeout);
    } catch (const std::exception &x) {
        throw std::runtime_error(x.what() + std::string(" (PPN: ") + record.getControlNumber() + ")");
    }
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    unsigned pdf_extraction_timeout(PdfUtil::DEFAULT_PDF_EXTRACTION_TIMEOUT);
    if (argc > 1 and StringUtil::StartsWith(argv[1], "--pdf-extraction-timeout=")) {
        if (not StringUtil::ToNumber(argv[1] + __builtin_strlen("--pdf-extraction-timeout="), &pdf_extraction_timeout)
            or pdf_extraction_timeout == 0)
                LOG_ERROR("bad value for --pdf-extraction-timeout!");
        ++argv, --argc;
    }

    if (argc != 4)
        Usage();

    long offset;
    if (not StringUtil::ToNumber(argv[1], &offset))
        LOG_ERROR("file offset must be a number!");

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[2], MARC::FileType::BINARY));
    if (not marc_reader->seek(offset, SEEK_SET))
        LOG_ERROR("failed to position " + marc_reader->getPath() + " at offset " + std::to_string(offset) + "!");

    try {
        return ProcessRecord(marc_reader.get(), argv[3], pdf_extraction_timeout) ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception &e) {
        LOG_ERROR("While reading \"" + marc_reader->getPath() + "\" starting at offset \""
              + std::string(argv[1]) + "\", caught exception: " + std::string(e.what()));
    }
}
