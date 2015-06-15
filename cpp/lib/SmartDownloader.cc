/** \file   SmartDownloader.cc
 *  \brief  Implementation of descedants of the SmartDownloader class.
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
#include "SmartDownloader.h"
#include <iostream>
#include "Downloader.h"
#include "FileUtil.h"
#include "MediaTypeUtil.h"
#include "StringUtil.h"
#include "util.h"


namespace {


inline unsigned ToNearestSecond(const unsigned time_in_milliseconds) {
    return (time_in_milliseconds + 500u) / 1000u;
}


}


SmartDownloader::SmartDownloader(const std::string &regex) {
    std::string err_msg;
    matcher_.reset(RegexMatcher::RegexMatcherFactory(regex, &err_msg));
    if (not matcher_) {
	std::cerr << progname << ": in SmartDownloader::SmartDownloader: pattern failed to compile \""
		  << regex << "\"!\n";
	std::exit(EXIT_FAILURE);
    }
}


bool SmartDownloader::canHandleThis(const std::string &url) const {
    std::string err_msg;
    if (matcher_->matched(url, &err_msg)) {
	if (not err_msg.empty()) {
	    std::cerr << progname
		      << ": in SmartDownloader::canHandleThis: an error occurred while trying to match \""
		      << url << "\" with \"" << matcher_->getPattern() << "\"! (" << err_msg << ")\n";
	    std::exit(EXIT_FAILURE);
	}
	return true;
    }

    return false;
}


bool SmartDownloader::downloadDoc(const std::string &url, const TimeLimit time_limit,
				  std::string * const document)
{
    if (downloadDocImpl(url, time_limit, document)) {
	++success_count_;
	return true;
    } else
	return false;
}


bool SimpleSuffixDownloader::canHandleThis(const std::string &url) const {
    for (const auto &suffix : suffixes_) {
	if (StringUtil::IsProperSuffixOfIgnoreCase(suffix, url))
	    return true;
    }

    return false;
}


bool SimpleSuffixDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
					     std::string * const document)
{
    return Download(url, ToNearestSecond(time_limit.getRemainingTime()), document) == 0;
}


bool SimplePrefixDownloader::canHandleThis(const std::string &url) const {
    for (const auto &prefix : prefixes_) {
	if (StringUtil::StartsWith(url, prefix, /* ignore_case = */ true))
	    return true;
    }

    return false;
}


bool SimplePrefixDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
					     std::string * const document)
{
    return Download(url, ToNearestSecond(time_limit.getRemainingTime()), document) == 0;
}


bool DigiToolSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
					      std::string * const document)
{
    FileUtil::AutoTempFile temp_file;
    if (Download(url, ToNearestSecond(time_limit.getRemainingTime()), document, temp_file.getFilePath()) != 0
	or time_limit.limitExceeded())
	return false;

    std::string start_string("<FRAME SRC=\"");
    size_t start_pos(document->find(start_string));
    if (start_pos == std::string::npos)
	return false;
    start_pos += start_string.length();
    
    size_t end_pos(document->find('"', start_pos));
    if (end_pos == std::string::npos)
	return false;

    if (Download("http://digitool.hbz-nrw.de:1801"
		 + document->substr(start_pos, end_pos - start_pos),
		 ToNearestSecond(time_limit.getRemainingTime()), document, temp_file.getFilePath()) != 0
	or time_limit.limitExceeded())
	return false;

    start_pos = document->find("setLabelMetadataStream");
    if (start_pos == std::string::npos)
	return false;

    end_pos = document->find("\");", start_pos);
    if (end_pos == std::string::npos)
	return false;

    start_pos = document->rfind('"', end_pos - 1);
    if (start_pos == std::string::npos)
	return false;

    if (Download(document->substr(start_pos + 1, end_pos - start_pos - 1),
		 ToNearestSecond(time_limit.getRemainingTime()), document, temp_file.getFilePath()) != 0)
	return false;

    static const std::string OCR_HEADER("ocr-text:\n");
    if (not StringUtil::StartsWith(*document, OCR_HEADER))
	return false;
    *document = document->substr(OCR_HEADER.size());

    return true;
}


bool IdbSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
					 std::string * const document)
{
    const size_t last_slash_pos(url.find_last_of('/'));
    const std::string doc_url("http://idb.ub.uni-tuebingen.de/cgi-bin/digi-downloadPdf.fcgi?projectname="
			      + url.substr(last_slash_pos + 1));
    return Download(doc_url, ToNearestSecond(time_limit.getRemainingTime()), document) == 0;
}


bool BszSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
					 std::string * const document)
{
    const std::string doc_url(url.substr(0, url.size() - 3) + "pdf");
    return Download(doc_url, ToNearestSecond(time_limit.getRemainingTime()), document) == 0;
}


bool BvbrSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
					  std::string * const document)
{
    std::string html;
    if (Download(url, ToNearestSecond(time_limit.getRemainingTime()), &html) != 0
	or time_limit.limitExceeded())
	return false;
    const std::string start_string("<body onload=window.location=\"");
    size_t start_pos(html.find(start_string));
    if (start_pos == std::string::npos)
	return false;
    start_pos += start_string.size();
    const size_t end_pos(html.find('"', start_pos + 1));
    if (end_pos == std::string::npos)
	return false;
    const std::string doc_url("http://bvbr.bib-bvb.de:8991" + html.substr(start_pos, end_pos - start_pos));
    return Download(doc_url, ToNearestSecond(time_limit.getRemainingTime()), document) == 0;
}


bool Bsz21SmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
					   std::string * const document)
{
    if (Download(url, ToNearestSecond(time_limit.getRemainingTime()), document) != 0
	or time_limit.limitExceeded())
	return false;
    if (MediaTypeUtil::GetMediaType(*document) == "application/pdf")
	return true;

    std::string start_string("Persistente URL: <a id=\"pers_url\" href=\"");
    size_t start_pos(document->find(start_string));
    std::string doc_url;
    if (start_pos != std::string::npos) {
	start_pos += start_string.size();
	const size_t end_pos(document->find('"', start_pos + 1));
	if (end_pos == std::string::npos)
	    return false;
	const std::string pers_url(document->substr(start_pos, end_pos - start_pos));
	const size_t last_slash_pos(pers_url.rfind('/'));
	if (last_slash_pos == std::string::npos or last_slash_pos == pers_url.size() - 1)
	    return false;
	doc_url = "http://idb.ub.uni-tuebingen.de/cgi-bin/digi-downloadPdf.fcgi?projectname="
	          + pers_url.substr(last_slash_pos + 1);
    } else {
	start_pos = document->find("name=\"citation_pdf_url\"");
	if (start_pos == std::string::npos)
	    return true;
	start_string = "meta content=\"";
	start_pos = document->rfind(start_string, start_pos);
	if (start_pos == std::string::npos)
	    return false;
	start_pos += start_string.size();
	const size_t end_pos(document->find('"', start_pos + 1));
        if (end_pos == std::string::npos)
            return false;
	doc_url = document->substr(start_pos, end_pos - start_pos);
    }

    return Download(doc_url, ToNearestSecond(time_limit.getRemainingTime()), document) == 0;
}


bool LocGovSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
					    std::string * const document)
{
    if (url.length() < 11)
	return false;
    const std::string doc_url("http://catdir" + url.substr(10));
    std::string html;
    const int retcode = Download(doc_url, ToNearestSecond(time_limit.getRemainingTime()), &html);

    if (retcode != 0)
	return false;
    size_t toc_start_pos(StringUtil::FindCaseInsensitive(html, "<TITLE>Table of contents"));
    if (toc_start_pos == std::string::npos)
	return false;
    const size_t pre_start_pos(StringUtil::FindCaseInsensitive(html, "<pre>"));
    if (pre_start_pos == std::string::npos)
	return false;
    const size_t pre_end_pos(StringUtil::FindCaseInsensitive(html, "</pre>"));
    if (pre_end_pos == std::string::npos)
	return false;
    *document = html.substr(pre_start_pos + 5, pre_end_pos - pre_start_pos - 5);
    return true;
}


bool SmartDownload(const std::string &url, std::string * const document) {
    document->clear();

    static std::vector<SmartDownloader *> smart_downloaders{
	new SimpleSuffixDownloader({ ".pdf", ".jpg", ".jpeg", ".txt" }),
	new SimplePrefixDownloader({ "http://www.bsz-bw.de/cgi-bin/ekz.cgi?" }),
	new SimplePrefixDownloader({ "http://deposit.d-nb.de/cgi-bin/dokserv?" }),
	new SimplePrefixDownloader({ "http://media.obvsg.at/" }),
	new SimplePrefixDownloader({ "http://d-nb.info/" }),
	new DigiToolSmartDownloader(),
	new IdbSmartDownloader(),
	new BszSmartDownloader(),
	new BvbrSmartDownloader(),
	new Bsz21SmartDownloader(),
	new LocGovSmartDownloader()
    };

    const unsigned TIMEOUT_IN_MILLISECS(10000); // Don't wait any longer than this.
    for (auto &smart_downloader : smart_downloaders) {
	if (smart_downloader->canHandleThis(url))
	    return smart_downloader->downloadDoc(
                url, smart_downloader->getName() == "DigiToolSmartDownloader" ? 60000 : TIMEOUT_IN_MILLISECS,
		document);
    }

    return false;
}
