/** \file   SmartDownloader.cc
 *  \brief  Implementation of descedants of the SmartDownloader class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2017 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "IdbPager.h"
#include "MediaTypeUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


namespace {


inline unsigned ToNearestSecond(const unsigned time_in_milliseconds) {
    return (time_in_milliseconds + 500u) / 1000u;
}


}


SmartDownloader::SmartDownloader(const std::string &regex, const bool trace): trace_(trace) {
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


bool DSpaceDownloader::canHandleThis(const std::string &url) const {
    return url.find("dspace") != std::string::npos;
}


bool DSpaceDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
                                       std::string * const document)
{
    document->clear();

    std::string html_document_candidate;
    if (Download(url, ToNearestSecond(time_limit.getRemainingTime()), &html_document_candidate) != 0)
        return false;

    static RegexMatcher *matcher;
    if (matcher == nullptr) {
        std::string err_msg;
        matcher = RegexMatcher::RegexMatcherFactory("meta content=\"http(.*)pdf\"", &err_msg);
        if (matcher == nullptr)
            Error("in DSpaceDownloader::downloadDocImpl: failed to compile regex! (" + err_msg + ")");
    }

    if (not matcher->matched(html_document_candidate))
        return false;

    const std::string pdf_link("http" + (*matcher)[1] + "pdf");
    if (Download(pdf_link, ToNearestSecond(time_limit.getRemainingTime()), document) != 0)
        return false;

    return true;
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
    if (trace_)
        std::cerr << "in SimpleSuffixDownloader::downloadDocImpl: about to download \"" << url << "\".\n";
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
    if (trace_)
        std::cerr << "in SimplePrefixDownloader::downloadDocImpl: about to download \"" << url << "\".\n";
    return Download(url, ToNearestSecond(time_limit.getRemainingTime()), document) == 0;
}


bool DigiToolSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
                                              std::string * const document)
{
    static RegexMatcher * const matcher(
        RegexMatcher::RegexMatcherFactory("http://digitool.hbz-nrw.de:1801/webclient/DeliveryManager\\?pid=\\d+"));

    std::string err_msg;
    size_t start_pos, end_pos;
    if (not matcher->matched(url, &err_msg, &start_pos, &end_pos))
        Error("in DigiToolSmartDownloader::downloadDocImpl: match failed: " + err_msg);

    const std::string normalised_url(url.substr(start_pos, end_pos - start_pos));
    FileUtil::AutoTempFile temp_file;


    if (trace_)
        std::cerr << "in DigiToolSmartDownloader::downloadDocImpl: about to download \"" << url << "\".\n";
    if (Download(normalised_url, ToNearestSecond(time_limit.getRemainingTime()), document,
                 temp_file.getFilePath()) != 0
        or time_limit.limitExceeded())
        return false;

    static const std::string ocr_text("ocr-text:\n");
    if (MediaTypeUtil::GetMediaType(*document) == "text/plain"
        and StringUtil::StartsWith(*document, ocr_text))
        *document = StringUtil::ISO8859_15ToUTF8(document->substr(ocr_text.length()));

    return true;
}


bool DiglitSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
                                            std::string * const document)
{
    if (trace_)
        std::cerr << "in DiglitSmartDownloader::downloadDocImpl: about to download \"" << url << "\".\n";
    if (Download(url, ToNearestSecond(time_limit.getRemainingTime()), document) != 0)
        return false;
    const std::string start_string("<input type=\"hidden\" name=\"projectname\" value=\"");
    size_t start_pos(document->find(start_string));
    if  (start_pos == std::string::npos)
        return false;
    start_pos += start_string.length();
    const size_t end_pos(document->find('"', start_pos));
    if  (end_pos == std::string::npos)
        return false;
    const std::string projectname(document->substr(start_pos, end_pos - start_pos));
    document->clear();
    std::string page;

    RomanPageNumberGenerator roman_page_number_generator;
    IdbPager roman_pager(projectname, &roman_page_number_generator);
    while (roman_pager.getNextPage(time_limit, &page)) {
        if (time_limit.limitExceeded())
            return false;
        document->append(page);
    }

    ArabicPageNumberGenerator arabic_page_number_generator;
    IdbPager arabic_pager(projectname, &arabic_page_number_generator);
    while (arabic_pager.getNextPage(time_limit, &page)) {
        if (time_limit.limitExceeded())
            return false;
        document->append(page);
    }

    return not document->empty();
}


bool BszSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
                                         std::string * const document)
{
    const std::string doc_url(url.substr(0, url.size() - 3) + "pdf");
    if (trace_)
        std::cerr << "in BszSmartDownloader::downloadDocImpl: about to download \"" << doc_url << "\".\n";
    return Download(doc_url, ToNearestSecond(time_limit.getRemainingTime()), document) == 0;
}


bool BvbrSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
                                          std::string * const document)
{
    std::string html;
    if (trace_)
        std::cerr << "in BvbrSmartDownloader::downloadDocImpl: about to download \"" << url << "\".\n";
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
    if (trace_)
        std::cerr << "in BvbrSmartDownloader::downloadDocImpl: about to download \"" << doc_url << "\".\n";
    return Download(doc_url, ToNearestSecond(time_limit.getRemainingTime()), document) == 0;
}


bool Bsz21SmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
                                           std::string * const document)
{
    if (trace_)
        std::cerr << "in Bsz21SmartDownloader::downloadDocImpl: about to download \"" << url << "\".\n";
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

    if (trace_)
        std::cerr << "in Bsz21SmartDownloader::downloadDocImpl: about to download \"" << doc_url << "\".\n";
    return Download(doc_url, ToNearestSecond(time_limit.getRemainingTime()), document) == 0;
}


bool LocGovSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit time_limit,
                                            std::string * const document)
{
    if (url.length() < 11)
        return false;
    const std::string doc_url("http://catdir" + url.substr(10));
    std::string html;
    if (trace_)
        std::cerr << "in LocGovSmartDownloader::downloadDocImpl: about to download \"" << doc_url << "\".\n";
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


bool SmartDownload(const std::string &url, const unsigned max_download_time, std::string * const document,
                   const bool trace)
{
    document->clear();

    static std::vector<SmartDownloader *> smart_downloaders{
        new DSpaceDownloader(trace),
        new SimpleSuffixDownloader({ ".pdf", ".jpg", ".jpeg", ".txt" }, trace),
            new SimplePrefixDownloader({ "http://www.bsz-bw.de/cgi-bin/ekz.cgi?" }, trace),
            new SimplePrefixDownloader({ "http://deposit.d-nb.de/cgi-bin/dokserv?" }, trace),
            new SimplePrefixDownloader({ "http://media.obvsg.at/" }, trace),
            new SimplePrefixDownloader({ "http://d-nb.info/" }, trace),
        new DigiToolSmartDownloader(trace),
        new DiglitSmartDownloader(trace),
        new BszSmartDownloader(trace),
        new BvbrSmartDownloader(trace),
        new Bsz21SmartDownloader(trace),
        new LocGovSmartDownloader(trace)
    };

    const unsigned TIMEOUT_IN_MILLISECS(max_download_time * 1000); // Don't wait any longer than this.
    for (auto &smart_downloader : smart_downloaders) {
        if (smart_downloader->canHandleThis(url))
            return smart_downloader->downloadDoc(
                url, smart_downloader->getName() == "DigiToolSmartDownloader" ? TIMEOUT_IN_MILLISECS * 3
                                                                              : TIMEOUT_IN_MILLISECS,
                document);
    }

    return false;
}
