/** Test harness for dealing with the most common domain names.

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
#include "MediaTypeUtil.h"
#include "RegexMatcher.h"
#include "SimpleDB.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " url output_or_db_filename [db_key]\n";
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


int Output(const std::string &output_or_db_filename, const std::string &db_key, const std::string &document) {
    if (not db_key.empty()) {
	const std::string content_type(
	    "Content-type: " + MediaTypeUtil::GetMediaType(document, /* auto_simplify = */ false) + "\r\n\r\n");

	SimpleDB db(output_or_db_filename, SimpleDB::OPEN_CREATE_READ_WRITE);
	db.binaryPutData(db_key, content_type + document);
	return 0;
    } else
	return WriteString(document, output_or_db_filename) ? 0 : -1;
}


int SmartDownload(std::string url, const std::string &output_or_db_filename, const std::string &db_key,
		  MatcherAndStats * const bsz_matcher,
		  MatcherAndStats * const idb_matcher, MatcherAndStats * const bvbr_matcher,
		  MatcherAndStats * const bsz21_matcher)
{
    const unsigned TIMEOUT_IN_SECS(5); // Don't wait any longer than this.

    if (StringUtil::IsProperSuffixOfIgnoreCase(".pdf", url) or StringUtil::IsProperSuffixOfIgnoreCase(".jpg", url)
	or StringUtil::IsProperSuffixOfIgnoreCase(".jpeg", url) or StringUtil::IsProperSuffixOfIgnoreCase(".txt", url))
	; // Do nothing!
    else if (StringUtil::IsPrefixOfIgnoreCase("http://www.bsz-bw.de/cgi-bin/ekz.cgi?", url)) {
	std::string html_document;
	int retcode;
	if ((retcode = Download(url, TIMEOUT_IN_SECS, &html_document)) != 0)
	    return retcode;
	html_document = StringUtil::UTF8ToISO8859_15(html_document);
	std::string plain_text(TextUtil::ExtractText(html_document));
	plain_text = StringUtil::ISO8859_15ToUTF8(plain_text);

	const size_t start_pos(plain_text.find("zugehörige Werke:"));
	if (start_pos != std::string::npos)
	    plain_text = plain_text.substr(0, start_pos);

	return Output(output_or_db_filename, db_key, plain_text);
    } else if (StringUtil::StartsWith(url, "http://digitool.hbz-nrw.de:1801", /* ignore_case = */ true)) {
	const size_t pid_pos(url.rfind("pid="));
	if (pid_pos == std::string::npos)
	    return -1;
	const std::string pid(url.substr(pid_pos + 4));
	url = "http://digitool.hbz-nrw.de:1801/webclient/DeliveryManager?pid=" + pid;
	std::string plain_text;
	int retcode;
	if ((retcode = Download(url, TIMEOUT_IN_SECS, &plain_text)) != 0)
	    return retcode;
	std::vector<std::string> lines;
	StringUtil::SplitThenTrimWhite(plain_text, '\n', &lines);
	std::string cleaned_up_text;
	cleaned_up_text.reserve(plain_text.size());
	for (const auto &line : lines) {
	    if (unlikely(line == "ocr-text:"))
		continue;
	    const std::string last_word(GetLastWordAfterSpace(line));
	    if (unlikely(last_word.empty())) {
		cleaned_up_text += line;
	    } else {
		if (likely(TextUtil::IsUnsignedInteger(last_word) or TextUtil::IsRomanNumeral(last_word)))
		    cleaned_up_text += line.substr(0, line.size() - last_word.size() - 1);
		else
		    cleaned_up_text += line;
	    }
	    cleaned_up_text += '\n';
	}

	return Output(output_or_db_filename, db_key, cleaned_up_text);
    } else if (bsz_matcher->matched(url))
	url = url.substr(0, url.size() - 3) + "pdf";
    else if (idb_matcher->matched(url)) {
	const size_t last_slash_pos(url.find_last_of('/'));
	url = "http://idb.ub.uni-tuebingen.de/cgi-bin/digi-downloadPdf.fcgi?projectname="
              + url.substr(last_slash_pos + 1);
    } else if (bvbr_matcher->matched(url)) {
	std::string html;
	const int retcode(Download(url, TIMEOUT_IN_SECS, &html));
	if (retcode != 0)
	    return retcode;
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

    std::string document;
    if (Download(url, TIMEOUT_IN_SECS, &document) != 0)
	return -1;
    return Output(output_or_db_filename, db_key, document);
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc != 3 and argc != 4)
	Usage();

    try {
	MatcherAndStats bsz_matcher("http://swbplus.bsz-bw.de/bsz.*\\.htm");
	MatcherAndStats idb_matcher("http://idb.ub.uni-tuebingen.de/diglit/.+");
	MatcherAndStats bvbr_matcher("http://bvbr.bib-bvb.de:8991/.+");
	MatcherAndStats bsz21_matcher("http://nbn-resolving.de/urn:nbn:de:bsz:21.+");
	if (SmartDownload(argv[1], argv[2], argc == 3 ? "" : argv[3], &bsz_matcher, &idb_matcher,
			  &bvbr_matcher, &bsz21_matcher) != 0)
	{
	    std::cerr << progname << ": Download failed!\n";
	    std::exit(EXIT_FAILURE);
	}
    } catch (const std::exception &e) {
	Error("Caught exception: " + std::string(e.what()));
    }
}
