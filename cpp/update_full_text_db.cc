/** \brief Utility for augmenting MARC records with links to a local full-text database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "DbConnection.h"
#include "Downloader.h"
#include "FullTextCache.h"
#include "MARC.h"
#include "PdfUtil.h"
#include "Semaphore.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"
#include "VuFind.h"


namespace {


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " file_offset marc_input marc_output\n\n"
              << "       file_offset  Where to start reading a MARC data set from in marc_input.\n\n";
    std::exit(EXIT_FAILURE);
}


// \note Sets "error_message" when it returns false.
bool GetDocumentAndMediaType(const std::string &url, const unsigned timeout, std::string * const document,
                             std::string * const media_type, std::string * const http_header_charset,
                             std::string * const error_message)
{
    Downloader::Params params;
    Downloader downloader(url, params, timeout);
    if (downloader.anErrorOccurred()) {
        *error_message = downloader.getLastErrorMessage();
        return false;
    }

    *document = downloader.getMessageBody();
    // Get media type including enconding
    *media_type = downloader.getMediaType();
    if (media_type->empty()) {
        *error_message = "failed to determine the media type!";
        return false;
    }

    *http_header_charset = downloader.getCharset();

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


std::string ConvertToPlainText(const std::string &media_type, const std::string &http_header_charset,
                               const std::string &tesseract_language_code, const std::string &document,
                               std::string * const error_message)
{
    if (media_type == "text/plain") {
        if (IsUTF8(http_header_charset))
            return document;

        std::string error_msg;
        std::unique_ptr<TextUtil::EncodingConverter> encoding_converter(TextUtil::EncodingConverter::Factory(http_header_charset,
                                                                                                             "utf8", &error_msg));
        if (encoding_converter.get() == nullptr) {
            WARNING("can't convert from \"" + http_header_charset + "\" to UTF-8! (" + error_msg + ")");
            return document;
        }

        std::string utf8_document;
        if (unlikely(not encoding_converter->convert(document, &utf8_document)))
            WARNING("conversion error while converting text from \"" + http_header_charset + "\" to UTF-8!");
        return utf8_document;
    }

    if (media_type == "text/html")
        return TextUtil::ExtractTextFromHtml(document, http_header_charset);

    if (StringUtil::StartsWith(media_type, "application/pdf")) {
        if (PdfUtil::PdfDocContainsNoText(document)) {
            std::string extracted_text;
            if (not PdfUtil::GetTextFromImagePDF(document, tesseract_language_code, &extracted_text)) {
                *error_message = "Failed to extract text from an image PDF!";
                WARNING(*error_message);
                return "";
            }
            return extracted_text;
        }
        return PdfUtil::ExtractText(document);

    }
    *error_message = "Don't know how to handle media type: " + media_type;
    WARNING(*error_message);
    return "";
}


// Returns true if text has been successfully extracted, else false.
bool ProcessRecord(MARC::Record * const record, const std::string &marc_output_filename) {
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
        constexpr unsigned PER_DOC_TIMEOUT(10000); // in milliseconds
        success = true;

        for (const auto &url : urls) {
            INFO("processing URL: \"" + url + "\".");
            FullTextCache::EntryUrl entry_url;
            entry_url.id_ = ppn;
            entry_url.url_ = url;
            std::string domain;
            cache.getDomainFromUrl(url, &domain);
            entry_url.domain_ = domain;
            std::string document, media_type, http_header_charset, error_message;
            if ((not GetDocumentAndMediaType(url, PER_DOC_TIMEOUT, &document, &media_type, &http_header_charset,
                                             &error_message))) {
                entry_url.error_message_ = "could not get document and media type! (" + error_message + ")";
                success = false;
            } else {
                std::string extracted_text(ConvertToPlainText(media_type, http_header_charset, GetTesseractLanguageCode(*record),
                                                              document, &error_message));

                if (unlikely(extracted_text.empty())) {
                    entry_url.error_message_ = "failed to extract text from the downloaded document! (" + error_message + ")";
                    success = false;
                } else {
                    if (combined_text.empty())
                        combined_text.swap(extracted_text);
                    else
                        combined_text += " " + extracted_text;
                }
            }

            if (success == true)
                success = entry_url.error_message_.empty();

            entry_urls.push_back(entry_url);
        }

        if (success) {
            combined_text_final = combined_text;
            TextUtil::CollapseAndTrimWhitespace(&combined_text_final);
        }

        cache.insertEntry(ppn, combined_text_final, entry_urls);
    }

    if (not combined_text_final.empty())
        record->insertField("FUL", { { 'e', "http://localhost/cgi-bin/full_text_lookup?id=" + ppn } });

    // Safely append the modified MARC data to the MARC output file:
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename, MARC::Writer::BINARY,
                                                                    MARC::Writer::APPEND));
    MARC::FileLockedComposeAndWriteRecord(marc_writer.get(), *record);

    return success;
}


// Returns true if text has been successfully extracted, else false.
bool ProcessRecord(MARC::Reader * const marc_reader, const std::string &marc_output_filename) {
    MARC::Record record(marc_reader->read());
    try {
        return ProcessRecord(&record, marc_output_filename);
    } catch (const std::exception &x) {
        throw std::runtime_error(x.what() + std::string(" (PPN: ") + record.getControlNumber() + ")");
    }
}


} // unnamed namespace


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 4)
        Usage();

    long offset;
    if (not StringUtil::ToNumber(argv[1], &offset))
        ERROR("file offset must be a number!");

    std::unique_ptr<MARC::Reader> marc_reader(MARC::Reader::Factory(argv[2], MARC::Reader::BINARY));
    if (not marc_reader->seek(offset, SEEK_SET))
        ERROR("failed to position " + marc_reader->getPath() + " at offset " + std::to_string(offset)
              + "! (" + std::to_string(errno) + ")");

    try {
        return ProcessRecord(marc_reader.get(), argv[3]) ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception &e) {
        ERROR("While reading \"" + marc_reader->getPath() + "\" starting at offset \""
              + std::string(argv[1]) + "\", caught exception: " + std::string(e.what()));
    }
}
