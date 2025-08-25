/** \file   SmartDownloader.cc
 *  \brief  Implementation of descedants of the SmartDownloader class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2017,2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MediaTypeUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "UBTools.h"
#include "UrlUtil.h"
#include "WebUtil.h"
#include "util.h"


namespace {


bool DownloadHelper(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                    std::string * const http_header_charset, std::string * const error_message,
                    Downloader::Params params = Downloader::Params()) {
    Downloader downloader(url, params, time_limit);
    if (downloader.anErrorOccurred()) {
        *error_message = downloader.getLastErrorMessage();
        return false;
    }

    *document = downloader.getMessageBody();
    *http_header_charset = downloader.getCharset();
    return true;
}


} // unnamed namespace


SmartDownloader::SmartDownloader(const std::string &regex, const bool trace): trace_(trace) {
    std::string err_msg;
    matcher_.reset(RegexMatcher::RegexMatcherFactory(regex, &err_msg));
    if (not matcher_)
        LOG_ERROR("pattern failed to compile \"" + regex + "\"!");
}


bool SmartDownloader::canHandleThis(const std::string &url) const {
    std::string err_msg;
    if (matcher_->matched(url, &err_msg)) {
        if (not err_msg.empty())
            LOG_ERROR("an error occurred while trying to match \"" + url + "\" with \"" + matcher_->getPattern() + "\"! (" + err_msg + ")");

        return true;
    }

    return false;
}


bool SmartDownloader::downloadDoc(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                                  std::string * const http_header_charset, std::string * const error_message) {
    if (downloadDocImpl(url, time_limit, document, http_header_charset, error_message)) {
        ++success_count_;
        return true;
    } else
        return false;
}


bool DSpaceDownloader::canHandleThis(const std::string &url) const {
    return url.find("dspace") != std::string::npos or url.find("xmlui") != std::string::npos or url.find("jspui") != std::string::npos;
}


bool DSpaceDownloader::downloadDocImpl(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                                       std::string * const http_header_charset, std::string * const error_message) {
    document->clear();

    if (not DownloadHelper(url, time_limit, document, http_header_charset, error_message))
        return false;

    static RegexMatcher *matcher;
    if (matcher == nullptr) {
        std::string err_msg;
        matcher = RegexMatcher::RegexMatcherFactory("meta content=\"http(.*)pdf\"", &err_msg);
        if (matcher == nullptr)
            LOG_ERROR("failed to compile regex! (" + err_msg + ")");
    }

    if (not matcher->matched(*document)) {
        *error_message = "no matching DSpace structure found!";
        return false;
    }

    const std::string pdf_link("http" + (*matcher)[1] + "pdf");
    if (not DownloadHelper(pdf_link, time_limit, document, http_header_charset, error_message))
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


bool SimpleSuffixDownloader::downloadDocImpl(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                                             std::string * const http_header_charset, std::string * const error_message) {
    if (trace_)
        LOG_INFO("about to download \"" + url + "\".");
    return (DownloadHelper(url, time_limit, document, http_header_charset, error_message));
}


bool SimplePrefixDownloader::canHandleThis(const std::string &url) const {
    for (const auto &prefix : prefixes_) {
        if (StringUtil::StartsWith(url, prefix, /* ignore_case = */ true))
            return true;
    }

    return false;
}


bool SimplePrefixDownloader::downloadDocImpl(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                                             std::string * const http_header_charset, std::string * const error_message) {
    if (trace_)
        LOG_INFO("about to download \"" + url + "\".");
    return (DownloadHelper(url, time_limit, document, http_header_charset, error_message));
}


bool DigiToolSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                                              std::string * const http_header_charset, std::string * const error_message) {
    static RegexMatcher * const matcher(
        RegexMatcher::RegexMatcherFactory("http://digitool.hbz-nrw.de:1801/webclient/DeliveryManager\\?pid=\\d+"));

    std::string err_msg;
    size_t start_pos, end_pos;
    if (not matcher->matched(url, &err_msg, &start_pos, &end_pos))
        LOG_ERROR("match failed: " + err_msg);

    const std::string normalised_url(url.substr(start_pos, end_pos - start_pos));

    if (trace_)
        LOG_INFO("about to download \"" + url + "\".");
    if (not DownloadHelper(normalised_url, time_limit, document, http_header_charset, error_message))
        return false;

    static const std::string ocr_text("ocr-text:\n");
    if (MediaTypeUtil::GetMediaType(*document) == "text/plain" and StringUtil::StartsWith(*document, ocr_text))
        *document = StringUtil::ISO8859_15ToUTF8(document->substr(ocr_text.length()));

    return true;
}


bool DiglitSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                                            std::string * const http_header_charset, std::string * const error_message) {
    std::string improved_url(url);
    if (RegexMatcher::Matched("/diglit/", url))
        improved_url = StringUtil::ReplaceString("/diglit/", "/opendigi/", &improved_url);
    if (RegexMatcher::Matched("/opendigi/", improved_url)) {
        /* Since we currently cannot appropriately associate the text on an article level
        / we completely omit OpenDigi documents */
        // improved_url += "/ocr";
        document->clear();
        return true;
    }

    if (trace_ and url != improved_url)
        LOG_INFO("converted url \"" + url + "\" to \"" + improved_url + "\"");

    if (trace_)
        LOG_INFO("about to download \"" + improved_url + "\".");
    if (not DownloadHelper(improved_url, time_limit, document, http_header_charset, error_message)) {
        if (trace_)
            LOG_WARNING("original download failed!");
        return false;
    }

    return not document->empty();
}


bool BszSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                                         std::string * const http_header_charset, std::string * const error_message) {
    const std::string doc_url(url.substr(0, url.size() - 3) + "pdf");
    if (trace_)
        LOG_INFO("about to download \"" + doc_url + "\".");
    return DownloadHelper(doc_url, time_limit, document, http_header_charset, error_message);
}


bool BvbrSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                                          std::string * const http_header_charset, std::string * const error_message) {
    if (trace_)
        LOG_INFO("about to download \"" + url + "\".");
    if (not DownloadHelper(url, time_limit, document, http_header_charset, error_message))
        return false;
    const std::string start_string("<body onload=window.location=\"");
    size_t start_pos(document->find(start_string));
    if (start_pos == std::string::npos) {
        *error_message = "no matching Bvbr structure found!";
        return false;
    }
    start_pos += start_string.size();
    const size_t end_pos(document->find('"', start_pos + 1));
    if (end_pos == std::string::npos) {
        *error_message = "no matching Bvbr structure found! (part 2)";
        return false;
    }
    const std::string doc_url("http://bvbr.bib-bvb.de:8991" + document->substr(start_pos, end_pos - start_pos));
    if (trace_)
        LOG_INFO("about to download \"" + doc_url + "\".");
    return DownloadHelper(doc_url, time_limit, document, http_header_charset, error_message);
}


bool Bsz21SmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                                           std::string * const http_header_charset, std::string * const error_message) {
    if (trace_)
        LOG_INFO("about to download \"" + url + "\".");
    if (not DownloadHelper(url, time_limit, document, http_header_charset, error_message))
        return false;

    if (MediaTypeUtil::GetMediaType(*document) == "application/pdf")
        return true;

    std::string start_string("Persistente URL: <a id=\"pers_url\" href=\"");
    size_t start_pos(document->find(start_string));
    std::string doc_url;
    if (start_pos != std::string::npos) {
        start_pos += start_string.size();
        const size_t end_pos(document->find('"', start_pos + 1));
        if (end_pos == std::string::npos) {
            *error_message = "no matching Bsz2l structure found! (part 1)";
            return false;
        }
        const std::string pers_url(document->substr(start_pos, end_pos - start_pos));
        const size_t last_slash_pos(pers_url.rfind('/'));
        if (last_slash_pos == std::string::npos or last_slash_pos == pers_url.size() - 1) {
            *error_message = "no matching Bsz2l structure found! (part 2)";
            return false;
        }
        doc_url = "http://idb.ub.uni-tuebingen.de/cgi-bin/digi-downloadPdf.fcgi?projectname=" + pers_url.substr(last_slash_pos + 1);
    } else {
        start_pos = document->find("name=\"citation_pdf_url\"");
        if (start_pos == std::string::npos)
            return true;
        start_string = "meta content=\"";
        start_pos = document->rfind(start_string, start_pos);
        if (start_pos == std::string::npos) {
            *error_message = "no matching Bsz2l structure found! (part 3)";
            return false;
        }
        start_pos += start_string.size();
        const size_t end_pos(document->find('"', start_pos + 1));
        if (end_pos == std::string::npos) {
            *error_message = "no matching Bsz2l structure found! (part 4)";
            return false;
        }
        doc_url = document->substr(start_pos, end_pos - start_pos);
    }

    if (trace_)
        LOG_INFO("about to download \"" + doc_url + "\".");
    return DownloadHelper(url, time_limit, document, http_header_charset, error_message);
}


bool LocGovSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                                            std::string * const http_header_charset, std::string * const error_message) {
    if (url.length() < 11) {
        *error_message = "LocGov URL too short!";
        return false;
    }
    const std::string doc_url("http://catdir" + url.substr(10));
    if (trace_)
        LOG_INFO("about to download \"" + doc_url + "\".");

    return DownloadHelper(doc_url, time_limit, document, http_header_charset, error_message);
}


bool OJSSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                                         std::string * const http_header_charset, std::string * const error_message) {
    // Try to extract the PDF URL from embedded metadata
    std::string html_page;
    if (not DownloadHelper(url, time_limit, &html_page, http_header_charset, error_message))
        return false;
    std::list<std::pair<std::string, std::string>> citation_pdf_urls;
    MetaTagExtractor meta_tag_extractor(html_page, "citation_pdf_url", &citation_pdf_urls);
    meta_tag_extractor.parse();
    if (citation_pdf_urls.empty()) {
        *error_message = "Could not extract citation_pdf_url metatag for \"" + url + '"';
        return false;
    }
    const auto &[name, pdf_url] = citation_pdf_urls.front();
    if (not DownloadHelper(pdf_url, time_limit, document, http_header_charset, error_message))
        return false;
    return true;
}


bool UCPSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                                         std::string * const http_header_charset, std::string * const error_message) {
    LOG_INFO("Entering UCPSmartDownloader");
    static RegexMatcher * const ucp_matcher(RegexMatcher::RegexMatcherFactoryOrDie("(.*journals.uchicago.edu/doi/)(10\\.1086/.*)"));
    if (ucp_matcher->matched(url)) {
        const std::string pdf_url = (*ucp_matcher)[1] + "pdf/" + (*ucp_matcher)[2];
        LOG_INFO("PDF_URL: " + pdf_url);
        Downloader::Params params;
        params.use_cookies_txt_ = true;
        params.follow_redirects_ = false;
        std::string redirected_url;
        // Two stage approach - Get redirect url and cookies if not present
        if (not GetRedirectUrlWithCustomParams(pdf_url, time_limit, &redirected_url, params))
            return false;
        if (not DownloadHelper(pdf_url, time_limit, document, http_header_charset, error_message, params))
            return false;
        return true;
    }
    return false;
}


bool BibelwissenschaftDotDeSmartDownloader::downloadDocImpl(const std::string &url, const TimeLimit &time_limit,
                                                            std::string * const document, std::string * const http_header_charset,
                                                            std::string * const error_message) {
    static RegexMatcher * const bibwissde_matcher(RegexMatcher::RegexMatcherFactoryOrDie("bibelwissenschaft.de/"));
    if (bibwissde_matcher->matched(url)) {
        Downloader::Params params;
        params.use_cookies_txt_ = true;
        params.follow_redirects_ = false;
        std::string redirected_url;
        if (not GetRedirectUrlWithCustomParams(url, time_limit, &redirected_url, params)) {
            LOG_WARNING("Could not get Redirect URL for \"" + url + "\" using original url");
            redirected_url = url;
        }
        if (not DownloadHelper(redirected_url, time_limit, document, http_header_charset, error_message))
            return false;

        std::vector<WebUtil::UrlAndAnchorTexts> url_and_anchor_tags;
        WebUtil::ExtractURLs(*document, "www.bibelwissenschaft.de", WebUtil::ExtractedUrlForm::ABSOLUTE_URLS, &url_and_anchor_tags);
        for (const auto &url_and_anchor_tag : url_and_anchor_tags) {
            const auto pdf_url_and_anchor(url_and_anchor_tag.getAnchorTexts().find("Artikel als PDF"));
            if (pdf_url_and_anchor != url_and_anchor_tag.end()) {
                if (not DownloadHelper(url_and_anchor_tag.getUrl(), time_limit, document, http_header_charset, error_message)) {
                    LOG_WARNING("Could not download document: " + url_and_anchor_tag.getUrl());
                    return false;
                }
                return true;
            }
        }
        return true;
    }
    return false;
}


bool DefaultDownloader::downloadDocImpl(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                                        std::string * const http_header_charset, std::string * const error_message) {
    return DownloadHelper(url, time_limit, document, http_header_charset, error_message);
}


bool GetRedirectedUrl(const std::string &url, const TimeLimit &time_limit, std::string * const redirected_url) {
    Downloader::Params params;
    params.follow_redirects_ = false;
    return GetRedirectUrlWithCustomParams(url, time_limit, redirected_url, params);
}


bool GetRedirectUrlWithCustomParams(const std::string &url, const TimeLimit &time_limit, std::string * const redirected_url,
                                    const Downloader::Params &custom_params) {
    Downloader::Params params(custom_params);
    params.follow_redirects_ = false;
    Downloader downloader(url, params, time_limit);
    return downloader.getHttpRedirectedUrl(redirected_url);
}


bool SmartDownload(const std::string &url, const TimeLimit &time_limit, std::string * const document,
                   std::string * const http_header_charset, std::string * const error_message, const bool trace) {
    document->clear();
    static std::vector<SmartDownloader *> smart_downloaders{ new DSpaceDownloader(trace),
                                                             new SimpleSuffixDownloader({ ".pdf", ".jpg", ".jpeg", ".txt" }, trace),
                                                             new SimplePrefixDownloader({ "http://www.bsz-bw.de/cgi-bin/ekz.cgi?" }, trace),
                                                             new SimplePrefixDownloader({ "http://deposit.d-nb.de/cgi-bin/dokserv?" },
                                                                                        trace),
                                                             new SimplePrefixDownloader({ "http://media.obvsg.at/" }, trace),
                                                             new SimplePrefixDownloader({ "http://d-nb.info/" }, trace),
                                                             new DigiToolSmartDownloader(trace),
                                                             new DiglitSmartDownloader(trace),
                                                             new BszSmartDownloader(trace),
                                                             new BvbrSmartDownloader(trace),
                                                             new Bsz21SmartDownloader(trace),
                                                             new LocGovSmartDownloader(trace),
                                                             new PublicationsTueSmartDownloader(trace),
                                                             new OJSSmartDownloader(trace),
                                                             new UCPSmartDownloader(trace),
                                                             new BibelwissenschaftDotDeSmartDownloader(trace),
                                                             new DefaultDownloader(trace) };

    for (auto &smart_downloader : smart_downloaders) {
        if (smart_downloader->canHandleThis(url)) {
            LOG_DEBUG("Downloading url " + url + " using " + smart_downloader->getName());
            return smart_downloader->downloadDoc(url, time_limit, document, http_header_charset, error_message);
        }
    }

    *error_message = "No downloader available for URL: " + url;
    return false;
}

bool SmartDownloadResolveFirstRedirectHop(const std::string &url, const TimeLimit &time_limit, const bool use_web_proxy,
                                          std::string * const document, std::string * const http_header_charset,
                                          std::string * const error_message, const bool trace) {
    std::string redirected_url(url);
    Downloader::Params params;
    if (use_web_proxy)
        params.proxy_host_and_port_ = UBTools::GetUBWebProxyURL();

    if (not GetRedirectUrlWithCustomParams(url, time_limit, &redirected_url, params))
        redirected_url = url; // Make sure redirected_url was not changed internally
    // If the redirection was just from http to https make another try (occurs e.g. with doi.dx requests)
    if (UrlUtil::URLIdenticalButDifferentScheme(url, redirected_url)) {
        if (not GetRedirectUrlWithCustomParams(redirected_url, time_limit, &redirected_url, params))
            LOG_WARNING("Could not resolve redirection for " + redirected_url);
    }
    return SmartDownload(redirected_url, time_limit, document, http_header_charset, error_message, trace);
}
