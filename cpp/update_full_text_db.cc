/** \brief Utility for augmenting MARC records with links to a local full-text database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2020 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <sstream>
#include "Compiler.h"
#include "FileUtil.h"
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


[[noreturn]] void Usage() {
    ::Usage("[--pdf-extraction-timeout=timeout] [--use-only-open-access-documents] [--store-pdfs-as-html] [--use-separate-entries-per-url]\n"
            "file_offset marc_input marc_output\n"
            "\"--pdf-extraction-timeout\" timeout in seconds (default " + std::to_string(PdfUtil::DEFAULT_PDF_EXTRACTION_TIMEOUT) + ").\n"
            "\"--use-only-open-access-documents\": use only dowload links that that are marked as \"Kostenfrei\"\n"
            "\"--store-pdfs-as-html\": Also store HTML representation of downloaded PDFs\n"
            "\"--use-separate-entries-per-url\": Store individual entries for the fulltext locations in a record\n"
            "\"--include-all-tocs\": Extract TOCs even if they are not matched by the only-open-access-filter\n"
            "\"--only-pdf-fulltexts\": Download real Fulltexts only if the link points to a PDF\n"
            "\"file_offset\" Where to start reading a MARC data set from in marc_input.");
}


// \note Sets "error_message" when it returns false.
bool GetDocumentAndMediaType(const std::string &url, const unsigned timeout, std::string * const document,
                             std::string * const media_type, std::string * const media_subtype,
                             std::string * const http_header_charset, std::string * const error_message)
{
    if (not SmartDownloadResolveFirstRedirectHop(url, timeout, document, http_header_charset, error_message))
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


struct UrlAndTextType {
    std::string url_;
    std::string text_type_;
public:
    bool operator <(const UrlAndTextType &rhs) const {
        if (url_ < rhs.url_)
            return true;
        if (url_ > rhs.url_)
            return false;
        return text_type_ < rhs.text_type_;
    }
};


FullTextCache::TextType GetTextTypes(const std::set<UrlAndTextType> &urls_and_text_types) {
    std::set<FullTextCache::TextType> text_types;
    std::transform(urls_and_text_types.begin(), urls_and_text_types.end(), inserter(text_types, text_types.begin()),
                   [](const UrlAndTextType &url_and_text_type)
                   { return FullTextCache::MapTextDescriptionToTextType(url_and_text_type.text_type_);});
    FullTextCache::TextType joined_text_types(FullTextCache::UNKNOWN);
    for (const auto text_type : text_types)
        joined_text_types |= text_type;
    return joined_text_types;
}


const std::string LOCAL_520_TEXT("LOCAL 520 FIELD");
void GetUrlsAndTextTypes(const MARC::Record &record, std::set<UrlAndTextType> * const urls_and_text_types,
                         const bool use_only_open_access_links, const bool include_all_tocs, const bool only_pdf_fulltexts,
                         const bool skip_reviews)
{
   for (const auto &_856_field : record.getTagRange("856")) {
       const MARC::Subfields _856_subfields(_856_field.getSubfields());

       if (_856_field.getIndicator1() == '7' or not _856_subfields.hasSubfield('u'))
           continue;

       if (use_only_open_access_links and not _856_subfields.hasSubfieldWithValue('z', "Kostenfrei", true /* case insensitive */)
           and not (include_all_tocs and _856_subfields.hasSubfieldWithValue('3', "Inhaltsverzeichnis", true /* case insentitive */))) {
           LOG_WARNING("Skipping entry since not kostenfrei");
           continue;
       }

       if (skip_reviews and IsProbablyAReview(_856_subfields))
           continue;

       // Only get the first item of each category to to avoid superfluous matches that garble up the result
       // For the Only-PDF-Fulltext-mode there is currently no really reliable way to determine the filetype beforehand
       // Thus we must add all candidates to the download list
       const std::string text_type_description(_856_subfields.getFirstSubfieldWithCode('3'));
       FullTextCache::TextType text_type(FullTextCache::MapTextDescriptionToTextType(text_type_description));
       if (GetTextTypes(*urls_and_text_types) and text_type) {
           if (not only_pdf_fulltexts)
               continue;
       }

       urls_and_text_types->emplace(UrlAndTextType({ _856_subfields.getFirstSubfieldWithCode('u'),
                                                     text_type_description } ));
   }

   if (record.hasFieldWithTag("520"))
       urls_and_text_types->emplace(UrlAndTextType({ LOCAL_520_TEXT, "Zusammenfassung" }));
}


void ExtractUrlsFromUrlsAndTextTypes(const std::set<UrlAndTextType> &urls_and_text_types, std::set<std::string> * const urls) {
     std::transform(urls_and_text_types.begin(), urls_and_text_types.end(), std::inserter(*urls, urls->begin()),
                    [](const UrlAndTextType &url_and_text_type) { return url_and_text_type.url_;});
}


bool ProcessRecordUrls(MARC::Record * const record, const unsigned pdf_extraction_timeout,
                       const bool use_only_open_access_links, const bool store_pdfs_as_html,
                       const bool use_separate_entries_per_url = false,
                       const bool include_all_tocs = false,
                       const bool only_pdf_fulltexts = false,
                       const bool skip_reviews = false)
{
    const std::string ppn(record->getControlNumber());
    std::set<UrlAndTextType> urls_and_text_types;
    GetUrlsAndTextTypes(*record, &urls_and_text_types, use_only_open_access_links, include_all_tocs, only_pdf_fulltexts, skip_reviews);
    std::set<std::string> urls;
    ExtractUrlsFromUrlsAndTextTypes(urls_and_text_types, &urls);;
    FullTextCache cache;
    const std::string semaphore_id("full_text_cached_counter");

    if (not use_separate_entries_per_url) {
        std::string combined_text_final;
        if (not cache.entryExpired(ppn, std::vector<std::string>(urls.begin(), urls.end())) or
            (only_pdf_fulltexts and not cache.dummyEntryExists(ppn)))
        {
            cache.getFullText(ppn, &combined_text_final);
            Semaphore semaphore(semaphore_id, Semaphore::ATTACH);
            ++semaphore;
            if (not combined_text_final.empty())
                record->insertField("FUL", { { 'e', "http://localhost/cgi-bin/full_text_lookup?id=" + ppn } });
            return true;
        } else
            cache.deleteEntry(ppn);
    } else {
        bool at_least_one_expired(false);
        if (only_pdf_fulltexts and cache.dummyEntryExists(ppn)) {
            Semaphore semaphore(semaphore_id, Semaphore::ATTACH);
            ++semaphore;
            return true;
        }
        for (auto url_and_text_type(urls_and_text_types.begin()); url_and_text_type != urls_and_text_types.end();/* intentionally empty */) {
            const bool expired(cache.singleUrlExpired(ppn, url_and_text_type->url_));
            if (not expired) {
                url_and_text_type = urls_and_text_types.erase(url_and_text_type);
                Semaphore semaphore(semaphore_id, Semaphore::ATTACH);
                ++semaphore;
            } else {
                at_least_one_expired |= expired;
                ++url_and_text_type;
            }

        }
        if (not at_least_one_expired)
            return true;
    }
    FullTextCache::Entry entry;
    std::vector<FullTextCache::EntryUrl> entry_urls;
    constexpr unsigned PER_DOC_TIMEOUT(30000); // in milliseconds
    bool at_least_one_error(false);
    std::stringstream combined_text_buffer;
    unsigned already_present_text_types(0);
    for (const auto &url_and_text_type : urls_and_text_types) {
        FullTextCache::EntryUrl entry_url;
        const std::string url(url_and_text_type.url_);
        entry_url.id_ = ppn;
        entry_url.url_ = url;
        std::string domain;
        cache.getDomainFromUrl(url, &domain);
        entry_url.domain_ = domain;
        std::string document, media_type, media_subtype, http_header_charset, error_message, extracted_text;
        FullTextCache::TextType text_type(FullTextCache::MapTextDescriptionToTextType(url_and_text_type.text_type_));

        if (url_and_text_type.url_ == LOCAL_520_TEXT)
            extracted_text = GetTextFrom520a(*record);
        else {
            if ((not GetDocumentAndMediaType(url, PER_DOC_TIMEOUT, &document, &media_type, &media_subtype, &http_header_charset,
                                             &error_message)))
            {
                LOG_WARNING("URL " + url + ": could not get document and media type! (" + error_message + ")");
                entry_url.error_message_ = "could not get document and media type! (" + error_message + ")";
                at_least_one_error = true;
                entry_urls.push_back(entry_url);
                continue;
            }

            // In Only-PDF-Fulltext-Mode we get all download candidates
            // So only go on if a text of this category is not already present
            if (only_pdf_fulltexts and (not StringUtil::StartsWith(media_type, "application/pdf") or
                                       (text_type and already_present_text_types) or cache.hasUrlWithTextType(ppn, text_type)))
                continue;
            extracted_text = ConvertToPlainText(media_type, media_subtype, http_header_charset, GetTesseractLanguageCode(*record),
                                                document, pdf_extraction_timeout, &error_message);
            if (unlikely(extracted_text.empty())) {
                LOG_WARNING("URL " + url + ": failed to extract text from the downloaded document! (" + error_message + ")");
                entry_url.error_message_ = "failed to extract text from the downloaded document! (" + error_message + ")";
                at_least_one_error = true;
                entry_urls.push_back(entry_url);
                continue;
            }
        }

        // Store immediately
        if (use_separate_entries_per_url) {
            cache.insertEntry(ppn,
                              TextUtil::CollapseAndTrimWhitespace(&extracted_text),
                              { entry_url },
                              FullTextCache::MapTextDescriptionToTextType(url_and_text_type.text_type_));
        } else
            combined_text_buffer << ((combined_text_buffer.tellp() != std::streampos(0)) ? " " : "") << extracted_text;

        if (store_pdfs_as_html and StringUtil::StartsWith(media_type, "application/pdf") and not PdfUtil::PdfDocContainsNoText(document)) {
            const FileUtil::AutoTempFile auto_temp_file("/tmp/fulltext_pdf");
            const std::string temp_pdf_path(auto_temp_file.getFilePath());
            FileUtil::WriteStringOrDie(temp_pdf_path, document);
            if (not PdfUtil::PdfFileContainsNoText(temp_pdf_path))
                cache.extractAndImportHTMLPages(ppn, temp_pdf_path,
                                                use_only_open_access_links ? FullTextCache::MapTextDescriptionToTextType(url_and_text_type.text_type_)
                                                : FullTextCache::UNKNOWN);
        }

        entry_urls.push_back(entry_url);
        already_present_text_types |= text_type;
    }

    // If we are in only_fulltext_pdfs-mode each record without PDF-links would be downloaded on each create_full_text_db run
    // only to be discarded because it does not match our rules.
    // So, if we are in this mode and no text has been stored in the cache insert a dummy entry to save time and bandwidth
    if (only_pdf_fulltexts and entry_urls.empty()) {
        FullTextCache::EntryUrl dummy_entry_url;
        dummy_entry_url.id_ = ppn;
        dummy_entry_url.url_ = FullTextCache::DUMMY_URL;
        dummy_entry_url.domain_ = FullTextCache::DUMMY_DOMAIN;
        dummy_entry_url.error_message_ = FullTextCache::DUMMY_ERROR;
        cache.insertEntry(ppn, "", { dummy_entry_url });
    }

    if (not use_separate_entries_per_url) {
        std::string combined_text_final = TextUtil::CollapseAndTrimWhitespace(combined_text_buffer.str());
        auto text_types(GetTextTypes(urls_and_text_types));
        cache.insertEntry(ppn, combined_text_final, entry_urls, text_types);
    }

    return (not at_least_one_error);
}


bool ProcessRecord(MARC::Record * const record, const std::string &marc_output_filename, const unsigned pdf_extraction_timeout,
                   const bool use_only_open_access_links, const bool extract_html_from_pdfs, const bool use_separate_entries_per_url,
                   const bool include_all_tocs, const bool only_pdf_fulltexts)
{
    bool success(false);
    try {
        success = ProcessRecordUrls(record, pdf_extraction_timeout, use_only_open_access_links,
                                    extract_html_from_pdfs, use_separate_entries_per_url, include_all_tocs, only_pdf_fulltexts);
    } catch (const std::exception &x) {
        LOG_WARNING("caught exception: " + std::string(x.what()));
    }

    // On failure pass writing out the record to create_full_text_db
    if (not success)
	return success;

    // Safely append the modified MARC data to the MARC output file:
    std::unique_ptr<MARC::Writer> marc_writer(MARC::Writer::Factory(marc_output_filename, MARC::FileType::BINARY,
                                                                    MARC::Writer::APPEND));
    MARC::FileLockedComposeAndWriteRecord(marc_writer.get(), *record);

    return success;
}


// Returns true if text has been successfully extracted, else false.
bool ProcessRecord(MARC::Reader * const marc_reader, const std::string &marc_output_filename, const unsigned pdf_extraction_timeout,
                   const bool use_only_open_access_links, const bool extract_html_from_pdfs, const bool use_separate_entries_per_url,
                   const bool include_all_tocs, const bool only_pdf_fulltexts)
{
    MARC::Record record(marc_reader->read());
    try {
        LOG_INFO("processing record " + record.getControlNumber());
        return ProcessRecord(&record, marc_output_filename, pdf_extraction_timeout, use_only_open_access_links,
                             extract_html_from_pdfs, use_separate_entries_per_url, include_all_tocs, only_pdf_fulltexts);
    } catch (const std::exception &x) {
        throw std::runtime_error(x.what() + std::string(" (PPN: ") + record.getControlNumber() + ")");
    }
}


} // unnamed namespace


int Main(int argc, char *argv[]) {
    unsigned pdf_extraction_timeout(PdfUtil::DEFAULT_PDF_EXTRACTION_TIMEOUT);
    if (argc > 1 and StringUtil::StartsWith(argv[1], "--pdf-extraction-timeout=")) {
        if (not StringUtil::ToNumber(argv[1] + __builtin_strlen("--pdf-extraction-timeout="), &pdf_extraction_timeout)
            or pdf_extraction_timeout == 0)
                LOG_ERROR("bad value for --pdf-extraction-timeout!");
        ++argv, --argc;
    }

    bool use_only_open_access_documents(false);
    if (argc > 1 and StringUtil::StartsWith(argv[1], "--use-only-open-access-documents")) {
        use_only_open_access_documents = true;
        ++argv, --argc;
    }


    bool store_html_from_pdfs(false);
    if (argc > 1 and StringUtil::StartsWith(argv[1], "--store-pdfs-as-html")) {
        store_html_from_pdfs = true;
        ++argv, --argc;
    }

    bool use_separate_entries_per_url(false);
    if (argc > 1 and StringUtil::StartsWith(argv[1], "--use-separate-entries-per-url")) {
        use_separate_entries_per_url = true;
        ++argv, --argc;
    }

    bool include_all_tocs(false);
    if (argc > 1 and StringUtil::StartsWith(argv[1], "--include-all-tocs")) {
        include_all_tocs = true;
        ++argv, --argc;
    }


    bool only_pdf_fulltexts(false);
    if (argc > 1 and StringUtil::StartsWith(argv[1], "--only-pdf-fulltexts")) {
        only_pdf_fulltexts = true;
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
        return ProcessRecord(marc_reader.get(), argv[3], pdf_extraction_timeout, use_only_open_access_documents, store_html_from_pdfs,
                             use_separate_entries_per_url, include_all_tocs, only_pdf_fulltexts) ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception &e) {
        LOG_ERROR("While reading \"" + marc_reader->getPath() + "\" starting at offset \""
              + std::string(argv[1]) + "\", caught exception: " + std::string(e.what()));
    }

    return EXIT_SUCCESS;
}
