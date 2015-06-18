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

  10535 http://swbplus.bsz-bw.de                  Done!
   4774 http://digitool.hbz-nrw.de:1801           Done!
   2977 http://www.gbv.de                         PDF's
   1070 http://bvbr.bib-bvb.de:8991               Done!
    975 http://deposit.d-nb.de                    HTML
    772 http://d-nb.info                          PDF's (Images => Need to OCR this?)
    520 http://www.ulb.tu-darmstadt.de            (Frau Gwinner arbeitet daran?)
    236 http://media.obvsg.at                     HTML
    167 http://www.loc.gov                        Done!
    133 http://deposit.ddb.de
    127 http://www.bibliothek.uni-regensburg.de
     57 http://nbn-resolving.de
     43 http://www.verlagdrkovac.de
     35 http://search.ebscohost.com
     25 http://idb.ub.uni-tuebingen.de
     22 http://link.springer.com
     18 http://heinonline.org
     15 http://www.waxmann.com
     13 https://www.destatis.de
     10 http://www.tandfonline.com
     10 http://dx.doi.org
      9 http://tocs.ub.uni-mainz.de
      8 http://www.onlinelibrary.wiley.com
      8 http://bvbm1.bib-bvb.de
      6 http://www.wvberlin.de
      6 http://www.jstor.org
      6 http://www.emeraldinsight.com
      6 http://www.destatis.de
      5 http://www.univerlag.uni-goettingen.de
      5 http://www.sciencedirect.com
      5 http://www.netread.com
      5 http://www.gesis.org
      5 http://content.ub.hu-berlin.de

*/
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <kchashdb.h>
#include <libgen.h>
#include <strings.h>
#include "Downloader.h"
#include "ExecUtil.h"
#include "FileUtil.h"
#include "MarcUtil.h"
#include "MediaTypeUtil.h"
#include "PdfUtil.h"
#include "RegexMatcher.h"
#include "SharedBuffer.h"
#include "SmartDownloader.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TextUtil.h"
#include "ThreadManager.h"
#include "ThreadSafeCounter.h"
#include "util.h"



static void Usage() __attribute__((noreturn));


const unsigned DEFAULT_PER_DOCUMENT_TIMEOUT(60);
const unsigned DEFAULT_WORKER_THREAD_COUNT(20);


static void Usage() {
    std::cerr << "Usage: " << progname << "[--worker-thread-count count] [--max-record-count count]\n"
	      << "                         [--skip-count count] [--per-doc-timeout timeout]\n"
	      << "                         marc_input marc_output full_text_db\n\n"
	      << "       --worker-thread-count  The number of worker threads used to process records.\n"
	      << "                              The default is " << DEFAULT_WORKER_THREAD_COUNT << ".\n"
	      << "       --max-record-count     The maximum number of records that will be processed.\n"
	      << "                              The default is " << UINT_MAX << ".\n"
	      << "       --skip-count           The number of initial records that will be skipped.\n"
	      << "                              The default is that no records will be skipped.\n"
	      << "       --per-doc-timeout      The maximum amount of time that will be spent in downloading\n"
	      << "                              a document in seconds.  This includes redirects.\n"
	      << "                              The default is " << DEFAULT_PER_DOCUMENT_TIMEOUT << " seconds.\n";

    std::exit(EXIT_FAILURE);
}


void ThreadSafeComposeAndWriteRecord(FILE * const output, const std::vector<DirectoryEntry> &dir_entries,
				     const std::vector<std::string> &field_data, Leader * const leader)
{
    static std::mutex marc_writer_mutex;
    std::unique_lock<std::mutex> mutex_locker(marc_writer_mutex);
    MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader);
}


/** Writes "media_type" and "document" to "db" and returns the unique key that was generated for the write. */
std::string ThreadSafeWriteDocumentWithMediaType(const std::string &media_type, const std::string &document,
						 kyotocabinet::HashDB * const db)
{
    static std::mutex simple_db_writer_mutex;
    std::unique_lock<std::mutex> mutex_locker(simple_db_writer_mutex);
    static unsigned key;
    ++key;
    const std::string key_as_string(std::to_string(key));
    db->add(key_as_string, "Content-type: " + media_type + "\r\n\r\n" + document);
    return key_as_string;
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


bool GetDocumentAndMediaType(const std::string &url, const unsigned timeout,
			     std::string * const document, std::string * const media_type)
{
    if (not SmartDownload(url, timeout, document)) {
	std::cerr << "Failed to download the document for " << url << "\n";
	return false;
    }

    *media_type = MediaTypeUtil::GetMediaType(*document, /* auto_simplify = */ false);
    if (media_type->empty())
	return false;

    return true;
}


bool GetTextFromImagePDF(const std::string &document, const std::string &media_type, const std::string &original_url,
			 const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &field_data,
			 const std::string &pdf_images_script, std::string * const extracted_text)
{
    if (not StringUtil::StartsWith(media_type, "application/pdf") or not PdfDocContainsNoText(document))
	return false;

    extracted_text->clear();
    std::cerr << "Found a PDF w/ no text.\n";

    const FileUtil::AutoTempFile auto_temp_file;
    const std::string &input_filename(auto_temp_file.getFilePath());
    if (not FileUtil::WriteString(input_filename, document))
	Error("failed to write the PDF to a temp file!");

    const FileUtil::AutoTempFile auto_temp_file2;
    const std::string &output_filename(auto_temp_file2.getFilePath());
    const std::string language_code(GetTesseractLanguageCode(dir_entries, field_data));
    const unsigned TIMEOUT(20); // in seconds
    if (Exec(pdf_images_script, { input_filename, output_filename, language_code }, "",
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

    std::cerr << "Whoohoo, got OCR'ed text.\n";

    return true;
}


static ThreadSafeCounter<unsigned> relevant_links_count, failed_count, records_with_relevant_links_count;


void ProcessRecord(ssize_t _856_index, Leader * const leader, std::vector<DirectoryEntry> &dir_entries,
		   std::vector<std::string> &field_data, const unsigned per_doc_timeout,
		   const std::string pdf_images_script, FILE * const output, kyotocabinet::HashDB * const db)
{
    bool found_at_least_one(false);
    for (/* Empty! */;
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
    delete leader;
}


struct ThreadData {
    ssize_t _856_index_;
    Leader * const leader_;
    std::vector<DirectoryEntry> dir_entries_;
    std::vector<std::string> field_data_;
    const unsigned per_doc_timeout_;
    const std::string &pdf_images_script_;
    FILE * const output_;
    kyotocabinet::HashDB * const db_;
};


void *WorkerThread(void *thread_data) {
    int old_state;
    if (::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state) != 0)
	Error("consumer thread failed to enable cancelability!");

    SharedBuffer<ThreadData> * const work_queue(reinterpret_cast<SharedBuffer<ThreadData> * const>(thread_data));
    for (;;) {
	ThreadData data(work_queue->pop_front());
	ProcessRecord(data._856_index_, data.leader_, data.dir_entries_, data.field_data_, data.per_doc_timeout_,
		      data.pdf_images_script_, data.output_, data.db_);
    }
}


void ProcessRecords(const unsigned worker_thread_count, const unsigned max_record_count, const unsigned skip_count,
		    const unsigned per_doc_timeout, const std::string &pdf_images_script, FILE * const input,
		    FILE * const output, kyotocabinet::HashDB * const db)
{
    Leader *leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    std::string err_msg;
    unsigned total_record_count(0);

    SharedBuffer<ThreadData> work_queue(worker_thread_count);
    ThreadManager thread_manager(worker_thread_count, WorkerThread, &work_queue);

    while (MarcUtil::ReadNextRecord(input, &leader, &dir_entries, &field_data, &err_msg)) {
	if (total_record_count == max_record_count)
	    break;
	++total_record_count;
	if (total_record_count <= skip_count)
	    continue;

	std::cout << "Processing record #" << total_record_count << ".\n";

	ssize_t _856_index(MarcUtil::GetFieldIndex(dir_entries, "856"));
	if (_856_index == -1) {
	    ThreadSafeComposeAndWriteRecord(output, dir_entries, field_data, leader);
	    continue;
	}

	ThreadData thread_data{ _856_index, leader, dir_entries, field_data, per_doc_timeout,
	                        pdf_images_script, output, db };
	work_queue.push_back(thread_data);
    }

    while (not work_queue.empty())
	::sleep(1);

    if (not err_msg.empty())
	Error(err_msg);
    std::cerr << "Read " << total_record_count << " records.\n";
    std::cerr << "Found " << records_with_relevant_links_count.get() << " records w/ relevant 856u fields.\n";
    std::cerr << failed_count.get() << " failed downloads, media type determinations or text extractions.\n";
    std::cerr << (100.0 * (relevant_links_count.get() - failed_count.get()) / double(relevant_links_count.get()))
	      << "% successes.\n";

    std::fclose(input);
    std::fclose(output);
}


const std::string BASH_HELPER("pdf_images_to_text.sh");


std::string GetPathToPdfImagesScript(const char * const argv0) {
    char path[std::strlen(argv0) + 1];
    std::strcpy(path, argv0);
    const std::string pdf_images_script_path(std::string(::dirname(path)) + "/" + BASH_HELPER);
    if (::access(pdf_images_script_path.c_str(), X_OK) != 0)
	Error("can't execute \"" + pdf_images_script_path + "\"!");
    return pdf_images_script_path;
}


bool GetOptionalArg(const std::string &option_name, char * const *argv, unsigned * const value) {
    if (*argv != option_name)
	return false;
    ++argv;
    if (argv == NULL)
	Error("missing value for " + option_name + "!");
    if (not StringUtil::ToUnsigned(*argv, value))
	Error("value for " + option_name + " must be an unsigned integer!");
    if (*(argv + 1) == NULL)
	Usage();
    return true;
}


char * const *ProcessOptionalArgs(char * const *argv, unsigned * const worker_thread_count,
				  unsigned * const max_record_count, unsigned * const skip_count,
				  unsigned * const timeout)
{
    *worker_thread_count = DEFAULT_WORKER_THREAD_COUNT;
    *max_record_count = UINT_MAX;                     // Read all records.
    *skip_count       = 0;                            // Don't skip any records.
    *timeout          = DEFAULT_PER_DOCUMENT_TIMEOUT; // seconds

    while (*argv) {
	if (GetOptionalArg("--worker-thread-count", argv, worker_thread_count))
	    argv += 2;
	else if (GetOptionalArg("--max-record-count", argv, max_record_count))
	    argv += 2;
	else if (GetOptionalArg("--skip-count", argv, skip_count))
	    argv += 2;
	else if (GetOptionalArg("--per-doc-timeout", argv, timeout))
	    argv += 2;
	else if (StringUtil::StartsWith(*argv, "--"))
	    Error("unrecognised argument: " + std::string(*argv));
	else
	    return argv;
    }

    return argv;
}


int main(int /*argc*/, char *argv[]) {
    progname = argv[0];

    unsigned worker_thread_count, max_record_count, skip_count, timeout;
    char * const * remaining_args(ProcessOptionalArgs(argv + 1, &worker_thread_count, &max_record_count,
						      &skip_count, &timeout));
    if (remaining_args == NULL)
	Usage();

    const std::string marc_input_filename(*remaining_args++);
    FILE *marc_input = std::fopen(marc_input_filename.c_str(), "rb");
    if (marc_input == NULL)
	Error("can't open \"" + marc_input_filename + "\" for reading!");
    if (remaining_args == NULL)
	Usage();

    const std::string marc_output_filename(*remaining_args++);
    FILE *marc_output = std::fopen(marc_output_filename.c_str(), "wb");
    if (marc_output == NULL)
	Error("can't open \"" + marc_output_filename + "\" for writing!");
    if (remaining_args == NULL)
	Usage();

    kyotocabinet::HashDB db;
    if (not db.open(*remaining_args++,
		    kyotocabinet::HashDB::OWRITER | kyotocabinet::HashDB::OCREATE
		    | kyotocabinet::HashDB::OTRUNCATE))
	Error("Failed to open database \"" + std::string(argv[1]) + "\" for writing ("
	      + std::string(db.error().message()) + ")!");
    if (*remaining_args != NULL)
	Usage();

    try {
	ProcessRecords(worker_thread_count, max_record_count, skip_count, timeout,
		       GetPathToPdfImagesScript(argv[0]), marc_input, marc_output, &db);
    } catch (const std::exception &e) {
	Error("Caught exception: " + std::string(e.what()));
    }
}
