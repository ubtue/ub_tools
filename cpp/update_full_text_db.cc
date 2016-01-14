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
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <kchashdb.h>
#include "Compiler.h"
#include "DbConnection.h"
#include "DirectoryEntry.h"
#include "ExecUtil.h"
#include "FileLocker.h"
#include "FileUtil.h"
#include "MarcUtil.h"
#include "MediaTypeUtil.h"
#include "PdfUtil.h"
#include "SmartDownloader.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"
#include "VuFind.h"
#include "XmlWriter.h"


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "Usage: " << ::progname << " file_offset counter marc_input marc_output full_text_db\n\n"
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


std::string GetTesseractLanguageCode(const MarcUtil::Record &record) {
    const auto map_iter(marc_to_tesseract_language_codes_map.find(
        record.getLanguageCode()));
    return (map_iter == marc_to_tesseract_language_codes_map.cend()) ? "" : map_iter->second;
}


bool GetTextFromImagePDF(const std::string &document, const std::string &media_type, const std::string &original_url,
			 const MarcUtil::Record &record, const std::string &pdf_images_script, std::string * const extracted_text)
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
    const std::string language_code(GetTesseractLanguageCode(record));
    static constexpr unsigned TIMEOUT(60); // in seconds
    if (ExecUtil::Exec(pdf_images_script, { input_filename, output_filename, language_code }, "", "", "", TIMEOUT)
	!= 0)
    {
        Warning("failed to execute conversion script \"" + pdf_images_script + "\" w/in "
                + std::to_string(TIMEOUT) + " seconds ! (original Url: " + original_url + ")");
        return false;
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


// Checks subfields "3" and "z" to see if they start w/ "Rezension".
bool IsProbablyAReview(const Subfields &subfields) {
    const auto _3_begin_end(subfields.getIterators('3'));
    if (_3_begin_end.first != _3_begin_end.second) {
        if (StringUtil::StartsWith(_3_begin_end.first->second, "Rezension"))
            return true;
    } else {
        const auto z_begin_end(subfields.getIterators('z'));
        if (z_begin_end.first != z_begin_end.second
            and StringUtil::StartsWith(z_begin_end.first->second, "Rezension"))
            return true;
    }

    return false;
}


/** Writes "media_type" and "document" to "db" and returns the unique key that was generated for the write. */
std::string FileLockedWriteDocumentWithMediaType(const std::string &media_type, const std::string &document,
                                                 const std::string &db_filename)
{
    FileLocker file_locker(db_filename, FileLocker::WRITE_ONLY);

    kyotocabinet::HashDB db;
    if (not db.open(db_filename, kyotocabinet::HashDB::OWRITER))
        Error("Failed to open database \"" + db_filename + "\" for writing ("
              + std::string(db.error().message()) + ")!");
    
    const std::string key(std::to_string(db.count() + 1));
    if (not db.add(key, "Content-type: " + media_type + "\r\n\r\n" + document))
        Error("Failed to add key/value pair to database \"" + db_filename + "\" ("
              + std::string(db.error().message()) + ")!");
    
    return key;
}


bool GetExtractedTextFromDatabase(DbConnection * const db_connection, const std::string &url, const std::string &document,
				  std::string * const extracted_text)
{
    const std::string QUERY("SELECT hash,full_text FROM full_text_cache WHERE url=\"" + url + "\"");
    if (not db_connection->query(QUERY))
	throw std::runtime_error("Query \"" + QUERY + "\" failed because: " + db_connection->getLastErrorMessage());

    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty())
	return false;

    assert(result_set.size() == 1);
    DbRow row(result_set.getNextRow());

    const std::string hash(StringUtil::ToHexString(StringUtil::Sha1(document)));
    if (unlikely(hash != row["hash"]))
	return false; // The document must have changed!

    *extracted_text = row["full_text"];

    // Update the timestap:
    const time_t now(std::time(nullptr));
    const std::string current_datetime(SqlUtil::TimeTToDatetime(now));
    const std::string UPDATE_STMT("UPDATE full_text_cache SET last_used=\"" + current_datetime + "\" WHERE url=\"" + url + "\"");
    if (not db_connection->query(UPDATE_STMT))
	throw std::runtime_error("Query \"" + UPDATE_STMT + "\" failed because: " + db_connection->getLastErrorMessage());

    return true;
}


// Returns true if text has been successfully extrcated, else false.
bool ProcessRecord(File * const input, const std::string &marc_output_filename,
		   const std::string &pdf_images_script, const std::string &db_filename)
{
    MarcUtil::Record record = MarcUtil::Record::XmlFactory(input);

    ssize_t _856_index(record.getFieldIndex("856"));
    if (_856_index == -1)
	Error("no 856 tag found!");

    constexpr unsigned PER_DOC_TIMEOUT(20);
    bool succeeded(false);

    const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
    const std::vector<std::string> &fields(record.getFields());
    const ssize_t dir_entry_count(static_cast<ssize_t>(dir_entries.size()));
    for (/* Empty! */; _856_index < dir_entry_count and dir_entries[_856_index].getTag() == "856"; ++_856_index) {
        Subfields subfields(fields[_856_index]);
        const auto u_begin_end(subfields.getIterators('u'));
        if (u_begin_end.first == u_begin_end.second) // No subfield 'u'.
            continue;

        if (IsProbablyAReview(subfields))
            continue;

        std::string document, media_type;
        const std::string url(u_begin_end.first->second);
        if (not GetDocumentAndMediaType(url, PER_DOC_TIMEOUT, &document, &media_type))
            continue;

	std::string mysql_url;
	VuFind::GetMysqlURL(&mysql_url);
	DbConnection db_connection(mysql_url);

        std::string extracted_text, key;
	if (GetExtractedTextFromDatabase(&db_connection, url, document, &extracted_text))
            key = FileLockedWriteDocumentWithMediaType("text/plain", extracted_text, db_filename);
	else if (GetTextFromImagePDF(document, media_type, url, record, pdf_images_script, &extracted_text)) {
            key = FileLockedWriteDocumentWithMediaType("text/plain", extracted_text, db_filename);
            const std::string hash(StringUtil::ToHexString(StringUtil::Sha1(document)));
	    const time_t now(std::time(nullptr));
	    const std::string current_datetime(SqlUtil::TimeTToDatetime(now));
	    const std::string INSERT_STMT("REPLACE INTO full_text_cache SET url=\"" + url + "\", hash=\"" + hash
					  + "\", full_text=\"" + SqlUtil::EscapeBlob(&extracted_text)
					  + "\", last_used=\"" + current_datetime + "\"");
	    if (not db_connection.query(INSERT_STMT))
		throw std::runtime_error("Query \"" + INSERT_STMT + "\" failed because: " + db_connection.getLastErrorMessage());
        } else
            key = FileLockedWriteDocumentWithMediaType(media_type, document, db_filename);

        subfields.addSubfield('e', "http://localhost/cgi-bin/full_text_lookup?id=" + key);
        const std::string new_856_field(subfields.toString());
        record.updateField(_856_index, new_856_field);

	succeeded = true;
    }

    // Safely append the modified MARC data to the MARC output file:
    FileLocker file_locker(marc_output_filename, FileLocker::WRITE_ONLY);
    File marc_output(marc_output_filename, "ab");
    if (not marc_output)
        Error("can't open \"" + marc_output_filename + "\" for appending!");
    XmlWriter xml_writer(&marc_output);
    record.write(&xml_writer);

    return succeeded;
}


const std::string BASH_HELPER("pdf_images_to_text.sh");


std::string GetPathToPdfImagesScript(const char * const argv0) {
    #pragma GCC diagnostic ignored "-Wvla"
    char path[std::strlen(argv0) + 1];
    #pragma GCC diagnostic warning "-Wvla"
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
    File marc_input(marc_input_filename, "rm");
    if (not marc_input)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[3]);

    if (not marc_input.seek(offset, SEEK_SET))
	Error("failed to position " + marc_input_filename + " at offset " + std::to_string(offset)
	      + "! (" + std::to_string(errno) + ")");

    try {
        return ProcessRecord(&marc_input, marc_output_filename, GetPathToPdfImagesScript(argv[0]), argv[4])
	       ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception &e) {
        Error("Caught exception: " + std::string(e.what()));
    }
}
