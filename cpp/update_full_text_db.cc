/** \brief Utility for augmenting MARC records with links to a local full-text database.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2017 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "MarcRecord.h"
#include "MarcReader.h"
#include "MarcWriter.h"
#include "MediaTypeUtil.h"
#include "PdfUtil.h"
#include "Semaphore.h"
#include "SmartDownloader.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "util.h"
#include "VuFind.h"


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


std::string GetTesseractLanguageCode(const MarcRecord &record) {
    const auto map_iter(marc_to_tesseract_language_codes_map.find(
        record.getLanguageCode()));
    return (map_iter == marc_to_tesseract_language_codes_map.cend()) ? "" : map_iter->second;
}


bool GetTextFromImagePDF(const std::string &document, const std::string &media_type, const std::string &original_url,
                         const MarcRecord &record, const std::string &pdf_images_script,
                         std::string * const extracted_text)
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


/** Writes "media_type" and "document" to "db" and returns the unique key that was generated for the write. */
std::string DbLockedWriteDocumentWithMediaType(const std::string &media_type, const std::string &document,
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


bool GetExtractedTextFromDatabase(DbConnection * const db_connection, const std::string &url,
                                  const std::string &document, std::string * const extracted_text)
{
    db_connection->queryOrDie("SELECT hash,full_text FROM full_text_cache WHERE url=\"" + url + "\"");

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
    db_connection->queryOrDie("UPDATE full_text_cache SET last_used=\"" + current_datetime + "\" WHERE url=\""
                              + url + "\"");

    return true;
}


const unsigned CACHE_EXPIRE_TIME_DELTA(84600 * 60); // About 2 months in seconds.


// \return True if we find "url" in the database and the entry is older than now-CACHE_EXPIRE_TIME_DELTA or if "url"
//         is not found in the database, else false.
bool CacheExpired(DbConnection * const db_connection, const std::string &url) {
    const std::string LAST_USED_QUERY("SELECT last_used FROM full_text_cache WHERE url=\"" + url + "\"");
    if (unlikely(not db_connection->query(LAST_USED_QUERY)))
        Error("in CacheExpired, DB query failed: " + LAST_USED_QUERY);
    
    DbResultSet result_set(db_connection->getLastResultSet());
    if (result_set.empty())
        return true;

    const DbRow first_row(result_set.getNextRow());
    const time_t last_used(SqlUtil::DatetimeToTimeT(first_row["last_used"]));
    const time_t now(std::time(nullptr));

    return last_used + CACHE_EXPIRE_TIME_DELTA < now;
}


// Returns true if text has been successfully extracted, else false.
bool ProcessRecord(MarcReader * const marc_reader, const std::string &pdf_images_script,
                   const std::string &db_filename)
{
    MarcRecord record(marc_reader->read());

    size_t _856_index(record.getFieldIndex("856"));
    if (_856_index == MarcRecord::FIELD_NOT_FOUND)
        Error("no 856 tag found (" + record.getControlNumber() + ")!");

    constexpr unsigned PER_DOC_TIMEOUT(40);
    bool succeeded(false);

    for (/* Empty! */; record.getTag(_856_index) == "856"; ++_856_index) {
        Subfields subfields(record.getSubfields(_856_index));
        if (subfields.getIndicator1() == '7' or not subfields.hasSubfield('u'))
            continue;

        if (IsProbablyAReview(subfields))
            continue;

        std::string document, media_type;
        const std::string url(subfields.getFirstSubfieldValue('u'));
        if (not GetDocumentAndMediaType(url, PER_DOC_TIMEOUT, &document, &media_type))
            continue;

        std::string mysql_url;
        VuFind::GetMysqlURL(&mysql_url);
        DbConnection db_connection(mysql_url);

        if (not CacheExpired(&db_connection, url)) {
            Semaphore semaphore("/full_text_cached_counter", Semaphore::ATTACH);
            ++semaphore;
            continue;
        }

        std::string extracted_text, key;
        if (GetExtractedTextFromDatabase(&db_connection, url, document, &extracted_text))
            key = DbLockedWriteDocumentWithMediaType("text/plain", extracted_text, db_filename);
        else if (GetTextFromImagePDF(document, media_type, url, record, pdf_images_script, &extracted_text)) {
            key = DbLockedWriteDocumentWithMediaType("text/plain", extracted_text, db_filename);
            const std::string hash(StringUtil::ToHexString(StringUtil::Sha1(document)));
            const time_t now(std::time(nullptr));
            const std::string current_datetime(SqlUtil::TimeTToDatetime(now));
            db_connection.queryOrDie("REPLACE INTO full_text_cache SET url=\"" + url + "\", hash=\"" + hash
                                     + "\", full_text=\"" + SqlUtil::EscapeBlob(&extracted_text)
                                     + "\", last_used=\"" + current_datetime + "\"");
        } else
            key = DbLockedWriteDocumentWithMediaType(media_type, document, db_filename);

        subfields.addSubfield('e', "http://localhost/cgi-bin/full_text_lookup?id=" + key);
        record.updateField(_856_index, subfields);

        succeeded = true;
    }

    // Safely append the modified MARC data to the MARC output file:
    const std::string marc_output_filename("./fulltext/" + record.getControlNumber() + ".mrc");
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename, MarcWriter::BINARY));
    marc_writer->write(record);

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
    
    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(argv[2], MarcReader::BINARY));
    if (not marc_reader->seek(offset, SEEK_SET))
        Error("failed to position " + marc_reader->getPath() + " at offset " + std::to_string(offset)
              + "! (" + std::to_string(errno) + ")");

    try {
        return ProcessRecord(marc_reader.get(), GetPathToPdfImagesScript(argv[0]), argv[4])
               ? EXIT_SUCCESS : EXIT_FAILURE;
    } catch (const std::exception &e) {
        Error("While reading \"" + marc_reader->getPath() + "\" starting at offset \"" + std::string(argv[1])
              + "\", caught exception: " + std::string(e.what()));
    }
}
