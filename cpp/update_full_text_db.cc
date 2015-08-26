/** \brief Utility for augmenting MARC records with links to a local full-text database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include <cstdio>
#include <cstdlib>
#include <kchashdb.h>
#include "DirectoryEntry.h"
#include "ExecUtil.h"
#include "FileLocker.h"
#include "FileUtil.h"
#include "MarcUtil.h"
#include "MediaTypeUtil.h"
#include "PdfUtil.h"
#include "SmartDownloader.h"
#include "StringUtil.h"
#include "util.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " file_offset marc_input marc_output full_text_db\n\n"
              << "       file_offset  Where to start reading a MARC data set from in marc_input.\n\n";
    std::exit(EXIT_FAILURE);
}


bool GetDocumentAndMediaType(const std::string &url, const unsigned timeout,
                             std::string * const document, std::string * const media_type)
{
    if (not SmartDownload(url, timeout, document)) {
        std::cerr << "Failed to download the document for " << url << " (timeout: " << timeout << " sec)\n";
        return false;
    }

    *media_type = MediaTypeUtil::GetMediaType(*document, /* auto_simplify = */ false);
    if (media_type->empty())
        return false;

    return true;
}


static std::map<std::string, std::string> marc_to_tesseract_language_codes_map {
    { "fre", "fra" },
    { "eng", "eng" },
    { "ger", "deu" },
    { "ita", "ita" },
    { "dut", "nld" },
    { "swe", "swe" },
    { "dan", "dan" },
    { "nor", "nor" },
    { "rus", "rus" },
    { "fin", "fin" },
    { "por", "por" },
    { "pol", "pol" },
    { "slv", "slv" },
    { "hun", "hun" },
    { "cze", "ces" },
    { "bul", "bul" },
};


std::string GetTesseractLanguageCode(const std::vector<DirectoryEntry> &dir_entries,
                                     const std::vector<std::string> &field_data)
{
    const auto map_iter(marc_to_tesseract_language_codes_map.find(
        MarcUtil::GetLanguageCode(dir_entries, field_data)));
    return (map_iter == marc_to_tesseract_language_codes_map.cend()) ? "" : map_iter->second;
}


bool GetTextFromImagePDF(const std::string &document, const std::string &media_type, const std::string &original_url,
                         const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &field_data,
                         const std::string &pdf_images_script, std::string * const extracted_text)
{
    extracted_text->clear();

    if (not StringUtil::StartsWith(media_type, "application/pdf") or not PdfDocContainsNoText(document))
        return false;

    std::cerr << "Found a PDF w/ no text.\n";

    const FileUtil::AutoTempFile auto_temp_file;
    const std::string &input_filename(auto_temp_file.getFilePath());
    if (not FileUtil::WriteString(input_filename, document))
        Error("failed to write the PDF to a temp file!");

    const FileUtil::AutoTempFile auto_temp_file2;
    const std::string &output_filename(auto_temp_file2.getFilePath());
    const std::string language_code(GetTesseractLanguageCode(dir_entries, field_data));
    const unsigned TIMEOUT(60); // in seconds
    if (ExecUtil::Exec(pdf_images_script, { input_filename, output_filename, language_code }, "",
		       TIMEOUT) != 0)
    {
        Warning("failed to execute conversion script \"" + pdf_images_script + "\" w/in "
                + std::to_string(TIMEOUT) + " seconds ! (original Url: " + original_url + ")");
        return true;
    }

    std::string plain_text;
    if (not ReadFile(output_filename, extracted_text))
        Error("failed to read OCR output!");

    if (extracted_text->empty())
        std::cerr << "Warning: OCR output is empty!\n";
    else
        std::cerr << "Whoohoo, got OCR'ed text.\n";

    return true;
}
/*

static std::atomic_uint relevant_links_count, failed_count, records_with_relevant_links_count, active_thread_count;


void ProcessRecord(ssize_t _856_index, std::shared_ptr<Leader> leader, std::vector<DirectoryEntry> &dir_entries,
                   std::vector<std::string> &field_data, const unsigned per_doc_timeout,
                   const std::string pdf_images_script, FILE * const output, kyotocabinet::HashDB * const db)
{
    ++active_thread_count;

    bool found_at_least_one(false);
    for (/ * Empty! * /;
        static_cast<size_t>(_856_index) < dir_entries.size() and dir_entries[_856_index].getTag() == "856";
        ++_856_index)
    {
        Subfields subfields(field_data[_856_index]);
        const auto u_begin_end(subfields.getIterators('u'));
        if (u_begin_end.first == u_begin_end.second) // No subfield 'u'.
            continue;

        if (IsProbablyAReview(subfields))
            continue;

        // If we get here, we have an 856u subfield that is not a review.
        ++relevant_links_count;
        if (not found_at_least_one) {
            ++records_with_relevant_links_count;
            found_at_least_one = true;
        }

        std::string document, media_type;
        const std::string url(u_begin_end.first->second);
        if (not GetDocumentAndMediaType(url, per_doc_timeout, &document, &media_type)) {
            ++failed_count;
            continue;
        }

        std::string extracted_text, key;
        if (GetTextFromImagePDF(document, media_type, url, dir_entries, field_data,
                                pdf_images_script, &extracted_text))
            key = ThreadSafeWriteDocumentWithMediaType("text/plain", extracted_text, db);
        else
            key = ThreadSafeWriteDocumentWithMediaType(media_type, document, db);

        subfields.addSubfield('e', "http://localhost/cgi-bin/full_text_lookup?id=" + key);
        const std::string new_856_field(subfields.toString());
        MarcUtil::UpdateField(_856_index, new_856_field, leader, &dir_entries, &field_data);
    }

    ThreadSafeComposeAndWriteRecord(output, dir_entries, field_data, leader);

    --active_thread_count;
}
*/


void ProcessRecord(FILE * const input) {
    std::shared_ptr<Leader> leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    std::string err_msg;
    if (not MarcUtil::ReadNextRecord(input, leader, &dir_entries, &field_data, &err_msg))
	Error("failed to read MARC record!");
}


const std::string BASH_HELPER("pdf_images_to_text.sh");


std::string GetPathToPdfImagesScript(const char * const argv0) {
    char path[std::strlen(argv0) + 1];
    std::strcpy(path, argv0);
    const std::string pdf_images_script_path(ExecUtil::Which(BASH_HELPER));
    if (::access(pdf_images_script_path.c_str(), X_OK) != 0)
        Error("can't execute \"" + pdf_images_script_path + "\"!");
    return pdf_images_script_path;
}


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 5)
	Usage();

    long offset;
    if (not StringUtil::ToNumber(argv[1], &offset))
	Error("file offset must be a number!");
    
    const std::string marc_input_filename(argv[2]);
    FILE * const marc_input(std::fopen(marc_input_filename.c_str(), "rb"));
    if (marc_input == nullptr)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[3]);
    FILE * const marc_output(std::fopen(marc_output_filename.c_str(), "wb"));
    if (marc_output == nullptr)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    if (std::fseek(marc_input, offset, SEEK_SET) == -1)
	Error("failed to position " + marc_input_filename + " at offset " + std::to_string(offset)
	      + "! (" + std::to_string(errno) + ")");

    kyotocabinet::HashDB db;
    if (not db.open(argv[4], kyotocabinet::HashDB::OWRITER | kyotocabinet::HashDB::OCREATE
                             | kyotocabinet::HashDB::OTRUNCATE))
        Error("Failed to open database \"" + std::string(argv[4]) + "\" for writing ("
              + std::string(db.error().message()) + ")!");

    try {
        ProcessRecord(marc_input);
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
