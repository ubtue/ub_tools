/** Utility for augmenting MARC records with links to a local full-text database.

  10535 http://swbplus.bsz-bw.de                  Done!
   4774 http://digitool.hbz-nrw.de:1801           Done!
   2977 http://www.gbv.de                         PDF's
   1070 http://bvbr.bib-bvb.de:8991               Done!
    975 http://deposit.d-nb.de                    HTML
    772 http://d-nb.info                          PDF's (Images => Need to OCR this?)
    520 http://www.ulb.tu-darmstadt.de            (Frau Gwinner arbeitet daran?)
    236 http://media.obvsg.at                     HTML
    167 http://www.loc.gov
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
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <strings.h>
#include "Downloader.h"
#include "MarcUtil.h"
#include "MediaTypeUtil.h"
#include "RegexMatcher.h"
#include "SimpleDB.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TextUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << "marc_input marc_output full_text_db\n";
    std::exit(EXIT_FAILURE);
}


class MatcherAndStats {
    std::unique_ptr<RegexMatcher> matcher_;
    unsigned match_count_;
public:
    explicit MatcherAndStats(const std::string &pattern);
    bool matched(const std::string &url);

    /** \brief Returns how often matched() was called and it returned true. */
    unsigned getMatchCount() const { return match_count_; }
};


MatcherAndStats::MatcherAndStats(const std::string &pattern) {
    std::string err_msg;
    matcher_.reset(RegexMatcher::RegexMatcherFactory(pattern, &err_msg));
    if (not matcher_) {
	std::cerr << progname << ": pattern failed to compile \"" << pattern << "\"!\n";
	std::exit(EXIT_FAILURE);
    }
}


bool MatcherAndStats::matched(const std::string &url) {
    std::string err_msg;
    if (matcher_->matched(url, &err_msg)) {
	if (not err_msg.empty()) {
	    std::cerr << progname << ": an error occurred while trying to match \"" << url << "\" with \""
		      << matcher_->getPattern() << "\"! (" << err_msg << ")\n";
	    std::exit(EXIT_FAILURE);
	}
	++match_count_;
	return true;
    }

    return false;
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


bool SmartDownload(std::string url, MatcherAndStats * const bsz_matcher, MatcherAndStats * const idb_matcher,
		   MatcherAndStats * const bvbr_matcher, MatcherAndStats * const bsz21_matcher,
		   std::string * const document)
{
    document->clear();

    const unsigned TIMEOUT_IN_SECS(5); // Don't wait any longer than this.

    if (StringUtil::IsProperSuffixOfIgnoreCase(".pdf", url)
	or StringUtil::IsProperSuffixOfIgnoreCase(".jpg", url)
	or StringUtil::IsProperSuffixOfIgnoreCase(".jpeg", url)
	or StringUtil::IsProperSuffixOfIgnoreCase(".txt", url))
	; // Do nothing!
    else if (StringUtil::IsPrefixOfIgnoreCase("http://www.bsz-bw.de/cgi-bin/ekz.cgi?", url)) {
	std::string html_document;
	if (Download(url, TIMEOUT_IN_SECS, &html_document) != 0)
	    return false;
	html_document = StringUtil::UTF8ToISO8859_15(html_document);
	std::string plain_text(TextUtil::ExtractText(html_document));
	plain_text = StringUtil::ISO8859_15ToUTF8(plain_text);

	const size_t start_pos(plain_text.find("zugehörige Werke:"));
	if (start_pos != std::string::npos)
	    *document = plain_text.substr(0, start_pos);

	return true;
    } else if (StringUtil::StartsWith(url, "http://digitool.hbz-nrw.de:1801", /* ignore_case = */ true)) {
	const size_t pid_pos(url.rfind("pid="));
	if (pid_pos == std::string::npos)
	    return false;
	const std::string pid(url.substr(pid_pos + 4));
	url = "http://digitool.hbz-nrw.de:1801/webclient/DeliveryManager?pid=" + pid;
	std::string plain_text;
	if (Download(url, TIMEOUT_IN_SECS, &plain_text) != 0)
	    return false;
	std::vector<std::string> lines;
	StringUtil::SplitThenTrimWhite(plain_text, '\n', &lines);
	document->reserve(plain_text.size());
	for (const auto &line : lines) {
	    if (unlikely(line == "ocr-text:"))
		continue;
	    const std::string last_word(GetLastWordAfterSpace(line));
	    if (unlikely(last_word.empty())) {
		*document += line;
	    } else {
		if (likely(TextUtil::IsUnsignedInteger(last_word) or TextUtil::IsRomanNumeral(last_word)))
		    *document += line.substr(0, line.size() - last_word.size() - 1);
		else
		    *document += line;
	    }
	    *document += '\n';
	}

	return true;
    } else if (bsz_matcher->matched(url))
	url = url.substr(0, url.size() - 3) + "pdf";
    else if (idb_matcher->matched(url)) {
	const size_t last_slash_pos(url.find_last_of('/'));
	url = "http://idb.ub.uni-tuebingen.de/cgi-bin/digi-downloadPdf.fcgi?projectname="
              + url.substr(last_slash_pos + 1);
    } else if (bvbr_matcher->matched(url)) {
	std::string html;
	if (Download(url, TIMEOUT_IN_SECS, &html) != 0)
	    return false;
	const std::string start_string("<body onload=window.location=\"");
	size_t start_pos(html.find(start_string));
	if (start_pos == std::string::npos)
	    return -2;
	start_pos += start_string.size();
	const size_t end_pos(html.find('"', start_pos + 1));
	if (end_pos == std::string::npos)
            return -3;
	url = "http://bvbr.bib-bvb.de:8991" + html.substr(start_pos, end_pos - start_pos);
    } else if (bsz21_matcher->matched(url)) {
	std::string html;
	const int retcode = Download(url, TIMEOUT_IN_SECS, &html);
	if (retcode != 0)
	    return retcode;
	const std::string start_string("<meta content=\"https://publikationen.uni-tuebingen.de/xmlui/bitstream/");
	size_t start_pos(html.find(start_string));
	if (start_pos == std::string::npos)
	    return -2;
	start_pos += start_string.size() - 55;
	const size_t end_pos(html.find('"', start_pos + 1));
	if (end_pos == std::string::npos)
            return -3;
	url = html.substr(start_pos, end_pos - start_pos);
    }

    return Download(url, TIMEOUT_IN_SECS, document) == 0;
}


void ProcessRecords(FILE * const input, FILE * const output, SimpleDB * const db) {
    MatcherAndStats bsz_matcher("http://swbplus.bsz-bw.de/bsz.*\\.htm");
    MatcherAndStats idb_matcher("http://idb.ub.uni-tuebingen.de/diglit/.+");
    MatcherAndStats bvbr_matcher("http://bvbr.bib-bvb.de:8991/.+");
    MatcherAndStats bsz21_matcher("http://nbn-resolving.de/urn:nbn:de:bsz:21.+");

    Leader *raw_leader;
    std::vector<DirectoryEntry> dir_entries;
    std::vector<std::string> field_data;
    std::string err_msg;
    unsigned count(0), matched_count(0), failed_count(0);
    unsigned key(0);

    while (MarcUtil::ReadNextRecord(input, &raw_leader, &dir_entries, &field_data, &err_msg)) {
	++count;
	std::cout << "Processing record #" << count << ".\n";
	std::unique_ptr<Leader> leader(raw_leader);

	const ssize_t _856_index(MarcUtil::GetFieldIndex(dir_entries, "856"));
	if (_856_index == -1) {
	    MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	    continue;
	}

	Subfields subfields(field_data[_856_index]);
	const auto u_begin_end(subfields.getIterators('u'));
	if (u_begin_end.first == u_begin_end.second) { // No subfield 'u'.
	    MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	    continue;
	}

	// Skip 8563 subfields starting with "Rezension":
	const auto _3_begin_end(subfields.getIterators('3'));
	if (_3_begin_end.first != _3_begin_end.second
	    and StringUtil::StartsWith(_3_begin_end.first->second, "Rezension"))
	{
	    MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	    continue;
	}

	// If we get here, we have an 856u subfield that is not a review.
	++matched_count;

	std::string document;
	if (not SmartDownload(u_begin_end.first->second, &bsz_matcher, &idb_matcher, &bvbr_matcher,
			      &bsz21_matcher, &document)) {
	    ++failed_count;
	    MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	    continue;
	}

	const std::string media_type(MediaTypeUtil::GetMediaType(document, /* auto_simplify = */ false));
	if (media_type.empty()) {
	    ++failed_count;
	    MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
	    continue;
	}

	// Write the key/value-pair to our simple keyed data store:
	++key;
	std::cout << "About to store record #" << key << " in the key/value database.\n";
	db->binaryPutData(std::to_string(key),  "Content-type: " + media_type + "\r\n\r\n" + document);

	subfields.addSubfield('e', "http://localhost/cgi-bin/db_lookup?id=" + std::to_string(key));
	const std::string new_856_field(subfields.toString());
	MarcUtil::UpdateField(_856_index, new_856_field, leader.get(), &dir_entries, &field_data);
	MarcUtil::ComposeAndWriteRecord(output, dir_entries, field_data, leader.get());
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

    if (argc != 4)
	Usage();

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
	ProcessRecords(marc_input, marc_output, &db);
    } catch (const std::exception &e) {
	Error("Caught exception: " + std::string(e.what()));
    }
}
