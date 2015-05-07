/** Utility for augmenting MARC records with links to a local full-text database.

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
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <strings.h>
#include "Downloader.h"
#include "MarcUtil.h"
#include "MediaTypeUtil.h"
#include "RegexMatcher.h"
#include "SharedBuffer.h"
#include "SimpleDB.h"
#include "SmartDownloader.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TextUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << "[--max-record-count count] marc_input marc_output full_text_db\n";
    std::exit(EXIT_FAILURE);
}


bool WriteString(const std::string &contents, const std::string &output_filename) {
    FILE *output = std::fopen(output_filename.c_str(), "wb");
    if (output == NULL)
	return false;

    if (std::fwrite(contents.data(), 1, contents.size(), output) != contents.size())
	return false;

    std::fclose(output);

    return true;
}


// Here "word" simply means a sequence of characters not containing a space.
std::string GetLastWordAfterSpace(const std::string &text) {
    const size_t last_space_pos(text.rfind(' '));
    if (last_space_pos == std::string::npos)
	return "";

    const std::string last_word(text.substr(last_space_pos + 1));
    return last_word;
}


bool SmartDownload(const std::string &url, std::vector<SmartDownloader *> &smart_downloaders,
		   std::string * const document)
{
    document->clear();

    const unsigned TIMEOUT_IN_SECS(5); // Don't wait any longer than this.
    for (auto &smart_downloader : smart_downloaders) {
	if (smart_downloader->canHandleThis(url))
	    return smart_downloader->downloadDoc(url, TIMEOUT_IN_SECS, document);
    }

    return false;
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
						 SimpleDB * const db)
{
    static std::mutex simple_db_writer_mutex;
    std::unique_lock<std::mutex> mutex_locker(simple_db_writer_mutex);
    static unsigned key;
    ++key;
    const std::string key_as_string(std::to_string(key));
    db->binaryPutData(key_as_string, "Content-type: " + media_type + "\r\n\r\n" + document);
    return key_as_string;
}


void ProcessRecords(const unsigned long max_record_count, FILE * const input, FILE * const output,
		    SimpleDB * const db) {
    std::vector<SmartDownloader *> smart_downloaders{
	new SimpleSuffixDownloader({ ".pdf", ".jpg", ".jpeg", ".txt" }),
	new SimplePrefixDownloader({ "http://www.bsz-bw.de/cgi-bin/ekz.cgi?" }),
	new DigiToolSmartDownloader(),
	new IdbSmartDownloader(),
	new BszSmartDownloader(),
	new BvbrSmartDownloader(),
	new Bsz21SmartDownloader(),
	new LocGovSmartDownloader()
    };

    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    std::string err_msg;
    unsigned long count(0), matched_count(0), failed_count(0);

    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
	if (count == max_record_count)
	    break;
	++count;

	std::cout << "Processing record #" << count << ".\n";
	std::unique_ptr<Leader> leader(raw_leader);

	const ssize_t _856_index(MarcUtil::GetFieldIndex(dir_entries, "856"));
	if (_856_index == -1) {
	    ThreadSafeComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	    continue;
	}

	Subfields subfields(field_data[_856_index]);
	const auto u_begin_end(subfields.getIterators('u'));
	if (u_begin_end.first == u_begin_end.second) { // No subfield 'u'.
	    ThreadSafeComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	    continue;
	}

	// Skip 8563 subfields starting with "Rezension":
	const auto _3_begin_end(subfields.getIterators('3'));
	if (_3_begin_end.first != _3_begin_end.second
	    and StringUtil::StartsWith(_3_begin_end.first->second, "Rezension"))
	{
	    ThreadSafeComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	    continue;
	}

	// If we get here, we have an 856u subfield that is not a review.
	++matched_count;

	std::string document;
	if (not SmartDownload(u_begin_end.first->second, smart_downloaders, &document)) {
	    std::cerr << "Failed to download the document for " << u_begin_end.first->second << "\n";
	    ++failed_count;
	    ThreadSafeComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	    continue;
	}

	const std::string media_type(MediaTypeUtil::GetMediaType(document, /* auto_simplify = */ false));
	if (media_type.empty()) {
	    ++failed_count;
	    ThreadSafeComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	    continue;
	}

	// Write the key/value-pair to our simple keyed data store:
	const std::string key(ThreadSafeWriteDocumentWithMediaType(media_type, document, db));

	subfields.addSubfield('e', "http://localhost/cgi-bin/db_lookup?id=" + key);
	const std::string new_856_field(subfields.toString());
	MarcUtil::UpdateField(_856_index, new_856_field, leader.get(), &dir_entries, &field_data);
	ThreadSafeComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
    }

    if (not err_msg.empty())
	Error(err_msg);
    std::cerr << "Read " << count << " records.\n";
    std::cerr << "Matched " << matched_count << " records w/ relevant 856u fields.\n";
    std::cerr << failed_count << " failed downloads.\n";

    std::fclose(input);
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc != 4 and argc != 6)
	Usage();

    unsigned long max_record_count(ULONG_MAX);
    if (argc == 6) {
	if (std::strcmp(argv[1], "--max-record-count") != 0)
	    Usage();
	if (not StringUtil::ToUnsignedLong(argv[2], &max_record_count))
	    Error(std::string(argv[2]) + " is not a valid max. record count!");
	argc -= 2, argv += 2;
    }

    const std::string marc_input_filename(argv[1]);
    FILE *marc_input = std::fopen(marc_input_filename.c_str(), "rb");
    if (marc_input == NULL)
	Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[2]);
    FILE *marc_output = std::fopen(marc_output_filename.c_str(), "wb");
    if (marc_output == NULL)
	Error("can't open \"" + marc_output_filename + "\" for writing!");

    try {
	SimpleDB db(argv[3], SimpleDB::OPEN_CREATE_READ_WRITE);
	ProcessRecords(max_record_count, marc_input, marc_output, &db);
    } catch (const std::exception &e) {
	Error("Caught exception: " + std::string(e.what()));
    }
}
