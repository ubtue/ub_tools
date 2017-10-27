/** \brief Utility for augmenting MARC records with links to a local full-text database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MarcRecord.h"
#include "MarcReader.h"
#include "MarcUtil.h"
#include "MarcWriter.h"
#include "PdfUtil.h"
#include "Semaphore.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TextUtil.h"
#include "util.h"
#include "VuFind.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " file_offset marc_input marc_output full_text_db error_log_path\n\n"
              << "       file_offset  Where to start reading a MARC data set from in marc_input.\n\n";
    std::exit(EXIT_FAILURE);
}


// \note Sets "error_message" when it returns false.
bool GetDocumentAndMediaType(const std::string &url, const unsigned timeout, std::string * const document,
                             std::string * const media_type, std::string * const error_message)
{
    Downloader::Params params;
    Downloader downloader(url, params, timeout);
    if (downloader.anErrorOccurred()) {
        *error_message = downloader.getLastErrorMessage();
        return false;
    }

    *document = downloader.getMessageBody();
    *media_type = downloader.getMediaType();
    if (media_type->empty()) {
        *error_message = "failed to determine the media type!";
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


std::string GetTesseractLanguageCode(const MarcRecord &record) {
    const auto map_iter(marc_to_tesseract_language_codes_map.find(
        record.getLanguageCode()));
    return (map_iter == marc_to_tesseract_language_codes_map.cend()) ? "" : map_iter->second;
}


// Checks subfields "3" and "z" to see if they start w/ "Rezension".
bool IsProbablyAReview(const Subfields &subfields) {
    const auto _3_begin_end(subfields.getIterators('3'));
    if (_3_begin_end.first != _3_begin_end.second) {
        if (StringUtil::StartsWith(_3_begin_end.first->value_, "Rezension"))
            return true;
    } else {
        const auto z_begin_end(subfields.getIterators('z'));
        if (z_begin_end.first != z_begin_end.second
            and StringUtil::StartsWith(z_begin_end.first->value_, "Rezension"))
            return true;
    }

    return false;
}


// \return The concatenated contents of all 520$a subfields.
std::string GetTextFrom520a(const MarcRecord &record) {
    std::string concatenated_text;

    std::vector<size_t> field_indices;
    record.getFieldIndices("520", &field_indices);
    for (const auto index : field_indices) {
        const Subfields subfields(record.getFieldData(index));
        if (subfields.hasSubfield('a')) {
            if (not concatenated_text.empty())
                concatenated_text += ' ';
            concatenated_text += subfields.getFirstSubfieldValue('a');
        }
    }

    return concatenated_text;
}


std::string ConvertToPlainText(const std::string &media_type, const std::string &tesseract_language_code,
                               const std::string &document)
{
    if (media_type == "text/plain")
        return document;
    if (media_type == "text/html")
        return TextUtil::ExtractTextFromHtml(document);
    if (StringUtil::StartsWith(media_type, "application/pdf")) {
        if (PdfUtil::PdfDocContainsNoText(document)) {
            std::string extracted_text;
            if (not PdfUtil::GetTextFromImagePDF(document, tesseract_language_code, &extracted_text)) {
                std::cerr << "Warning: Failed to extract text from an image PDF!\n";
                return "";
            }
            return extracted_text;
        }
        return PdfUtil::ExtractText(document);

    }
    logger->warning("in ConvertToPlainText: don't know how to handle media type: " + media_type);
    return "";
}


// Returns true if text has been successfully extracted, else false.
bool ProcessRecord(MarcReader * const marc_reader, const std::string &marc_output_filename,
                   const std::string &full_text_db_path)
{
    MarcRecord record(marc_reader->read());
    const std::string ppn(record.getControlNumber());

    std::string mysql_url;
    VuFind::GetMysqlURL(&mysql_url);
    DbConnection db_connection(mysql_url);

    bool success(false);
    if (not FullTextCache::CacheEntryExpired(&db_connection, full_text_db_path, ppn)) {
        Semaphore semaphore("/full_text_cached_counter", Semaphore::ATTACH);
        ++semaphore;
        success = true;
    } else {
        std::string combined_text(GetTextFrom520a(record));

        std::vector<size_t> _856_field_indices;
        record.getFieldIndices("856", &_856_field_indices);
    
        constexpr unsigned PER_DOC_TIMEOUT(10000); // in milliseconds

        std::string error_message, url;
        for (const size_t _856_field_index : _856_field_indices) {
            const Subfields _856_subfields(record.getSubfields(_856_field_index));
            if (_856_subfields.getIndicator1() == '7' or not _856_subfields.hasSubfield('u'))
                continue;

            if (IsProbablyAReview(_856_subfields))
                continue;

            url = _856_subfields.getFirstSubfieldValue('u');
            std::string document, media_type;
            if ((not GetDocumentAndMediaType(url, PER_DOC_TIMEOUT, &document, &media_type, &error_message)))
                break;
            else {
                std::string extracted_text(ConvertToPlainText(media_type, GetTesseractLanguageCode(record),
                                                              document));
                if (not extracted_text.empty()) {
                    if (combined_text.empty())
                        combined_text.swap(extracted_text);
                    else
                        combined_text += " " + extracted_text;
                } else {
                    error_message = "failed to extract text from the downloaded document!";
                    break;
                }
            }
        }

        std::string data;
        if (error_message.empty()) {
            url.clear();
            data = "Content-Type: text/plain\r\n\r\n" + combined_text;
        }
        FullTextCache::InsertIntoCache(&db_connection, full_text_db_path, ppn, url, data, error_message);

        if (not combined_text.empty()) {
            record.insertSubfield("FUL", 'e', "http://localhost/cgi-bin/full_text_lookup?id=" + ppn);
            success = true;
        }
    }

    // Safely append the modified MARC data to the MARC output file:
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename, MarcWriter::BINARY,
                                                                MarcWriter::APPEND));
    MarcUtil::FileLockedComposeAndWriteRecord(marc_writer.get(), &record);

    return success;
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 5)
        Usage();

    long offset;
    if (not StringUtil::ToNumber(argv[1], &offset))
        logger->error("file offset must be a number!");

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[2], MarcReader::BINARY));
    if (not marc_reader->seek(offset, SEEK_SET))
        logger->error("failed to position " + marc_reader->getPath() + " at offset " + std::to_string(offset)
                      + "! (" + std::to_string(errno) + ")");

    try {
        return ProcessRecord(marc_reader.get(), argv[3], argv[4]) ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception &e) {
        logger->error("While reading \"" + marc_reader->getPath() + "\" starting at offset \""
                      + std::string(argv[1]) + "\", caught exception: " + std::string(e.what()));
    }
}
