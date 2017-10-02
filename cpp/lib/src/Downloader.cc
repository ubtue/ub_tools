/** \file    Downloader.cc
 *  \brief   Implementation of class Downloader.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2008 Project iVia.
 *  Copyright 2008 The Regents of The University of California.
 *  Copyright 2017 Universitätsbibliothek Tübingen.
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Downloader.h"
#include <iostream>
#include "ExecUtil.h"
#include "FileUtil.h"
#include "IniFile.h"
#include "MediaTypeUtil.h"
#include "StringUtil.h"
#include "util.h"
#include "WebUtil.h"


namespace {


int GlobalInit() {
    if (unlikely(::curl_global_init(CURL_GLOBAL_ALL) != 0)) {
        const std::string error_message("curl_global_init(3) failed!\n");
        const ssize_t dummy = ::write(STDERR_FILENO, error_message.c_str(), error_message.length());
        (void)dummy;
        ::_exit(EXIT_FAILURE);
    }

    return 0;
}


} // unnamed namespace


int dummy(GlobalInit());


CURLSH *Downloader::share_handle_(nullptr);
unsigned Downloader::instance_count_(0);
std::mutex *Downloader::dns_mutex_(nullptr);
std::mutex *Downloader::cookie_mutex_(nullptr);
std::mutex *Downloader::robots_dot_txt_mutex_(nullptr);
std::unordered_map<std::string, RobotsDotTxt> Downloader::url_to_robots_dot_txt_map_;
const std::string Downloader::DEFAULT_USER_AGENT_STRING("UB Tübingen C++ Downloader");
const std::string Downloader::DEFAULT_ACCEPTABLE_LANGUAGES("en,eng,english");
std::string Downloader::DENIED_BY_ROBOTS_DOT_TXT_ERROR_MSG("Disallowed by robots.txt.");
std::string Downloader::default_user_agent_string_;


Downloader::Params::Params(const std::string &user_agent, const std::string &acceptable_languages,
                           const long max_redirect_count, const long dns_cache_timeout,
                           const bool honour_robots_dot_txt, const TextTranslationMode text_translation_mode,
                           const PerlCompatRegExps &banned_reg_exps, const bool debugging,
                           const bool follow_redirects, bool ignore_ssl_certificates,
                           const std::string &proxy_host_and_port, const std::vector<std::string> &additional_headers)
    : user_agent_(user_agent), acceptable_languages_(acceptable_languages), max_redirect_count_(max_redirect_count),
      dns_cache_timeout_(dns_cache_timeout), honour_robots_dot_txt_(honour_robots_dot_txt),
      text_translation_mode_(text_translation_mode), banned_reg_exps_(banned_reg_exps), debugging_(debugging),
      follow_redirects_(follow_redirects), ignore_ssl_certificates_(ignore_ssl_certificates),
      proxy_host_and_port_(proxy_host_and_port), additional_headers_(additional_headers)
{
    max_redirect_count_ = follow_redirects_ ? max_redirect_count_ : 0 ;

    if (unlikely(max_redirect_count_ < 0 or max_redirect_count_ > MAX_MAX_REDIRECT_COUNT))
        throw std::runtime_error("in Downloader::Params::Params: max_redirect_count (= "
                                 + StringUtil::ToString(max_redirect_count) + " must be between 1 and "
                                 + StringUtil::ToString(MAX_MAX_REDIRECT_COUNT) + "!");
}


Downloader::Downloader(const Url &url, const Params &params, const TimeLimit &time_limit)
    : multi_mode_(false), additional_http_headers_(nullptr), params_(params)
{
    init();
    newUrl(url, time_limit);
}


Downloader::Downloader(const std::string &url, const Params &params, const TimeLimit &time_limit, bool multimode)
    : multi_mode_(multimode), additional_http_headers_(nullptr), params_(params)
{
    init();
    newUrl(url, time_limit);
}


Downloader::~Downloader() {
    --instance_count_;

    if (additional_http_headers_ != nullptr)
        ::curl_slist_free_all(additional_http_headers_);
    if (likely(easy_handle_ != nullptr))
        ::curl_easy_cleanup(easy_handle_);
}


namespace {


void SplitHttpHeaders(const std::string &possible_combo_header, std::vector<std::string> * const headers) {
    headers->clear();
    if (possible_combo_header.empty())
        return;

    // First we try CR/LF/CR/LF sequences as per spec:
    StringUtil::Split(possible_combo_header, "\r\n\r\n", headers);
    if (not headers->empty()) {
        for (std::vector<std::string>::iterator header(headers->begin()); header != headers->end(); ++header)
            *header += "\r\n\r\n";
        return;
    }

    // Sometimes we get HTTP headers that end in LF/LF sequences:
    StringUtil::Split(possible_combo_header, "\n\n", headers);
    for (std::vector<std::string>::iterator header(headers->begin()); header != headers->end(); ++header)
        *header += "\n\n";
}


} // unnamed namespace


bool Downloader::newUrl(const Url &url, const TimeLimit &time_limit) {
    redirect_urls_.clear();
    current_url_ = url;
    last_error_message_.clear();
    if (url.isValidWebUrl() and params_.honour_robots_dot_txt_ and not allowedByRobotsDotTxt(url, time_limit)) {
        last_error_message_ = DENIED_BY_ROBOTS_DOT_TXT_ERROR_MSG;
        return false;
    }

    header_.clear();
    body_.clear();

    for (;;) {
        // Too many redirects?
        if (getRemainingNoOfRedirects() < 1) {
            last_error_message_ = "Too many redirects (> " + StringUtil::ToString(params_.max_redirect_count_) + ")!";
            return false;
        }

        if (not params_.banned_reg_exps_.empty() and params_.banned_reg_exps_.matchAny(current_url_)) {
            last_error_message_ = "URL banned by regular expression!";
            return false;
        }

        if (::curl_easy_setopt(easy_handle_, CURLOPT_MAXREDIRS, getRemainingNoOfRedirects()) != CURLE_OK)
            throw std::runtime_error("in Downloader::newUrlWithHttpEquivRedirect: curl_easy_setopt() failed!");

        if (not internalNewUrl(current_url_, time_limit))
            return false;

        std::string redirect_url;
        if (current_url_.isValidWebUrl() ) {
            if (getHttpEquivRedirect(&redirect_url)) {
                current_url_ = Url(redirect_url, current_url_);
                continue;
            }

            // If we have a Web page we attempt a translation to Latin-9 if requested:
            if (params_.text_translation_mode_ == MAP_TO_LATIN9 and not header_.empty()) {
                std::vector<std::string> headers;
                SplitHttpHeaders(header_, &headers);
                body_ = WebUtil::ConvertToLatin9(HttpHeader(headers.back()), body_);
            }
        }

        return true;
    }
}


std::string Downloader::getMessageHeader() const {
    if (header_.empty())
        return "";

    std::vector<std::string> headers;
    SplitHttpHeaders(header_, &headers);
    return headers.back();
}


std::string Downloader::getMediaType(const bool auto_simplify) const {
    return MediaTypeUtil::GetMediaType(getMessageHeader(), body_, auto_simplify);
}


const std::string &Downloader::getLastErrorMessage() const {
    if (curl_error_code_ != CURLE_OK and last_error_message_.empty())
        last_error_message_ = ::curl_easy_strerror(curl_error_code_);

    return last_error_message_;
}


void Downloader::init() {
    ++instance_count_;

    last_error_message_.clear();

    Downloader::InitCurlEasyHandle(params_.dns_cache_timeout_, error_buffer_, params_.debugging_, WriteFunction,
                                   LockFunction, UnlockFunction, HeaderFunction, DebugFunction, &easy_handle_,
                                   &params_.user_agent_, params_.follow_redirects_);

    if (not params_.acceptable_languages_.empty())
        additional_http_headers_ = curl_slist_append(additional_http_headers_,
                                                     ("Accept-Language: " + params_.acceptable_languages_).c_str());

    for (const auto &additional_header : params_.additional_headers_)
        additional_http_headers_ = curl_slist_append(additional_http_headers_, additional_header.c_str());

    if (unlikely(::curl_easy_setopt(easy_handle_, CURLOPT_WRITEDATA, reinterpret_cast<void *>(this)) != CURLE_OK))
        throw std::runtime_error("in Downloader::init: curl_easy_setopt() failed (1)!");

    if (unlikely(::curl_easy_setopt(easy_handle_, CURLOPT_WRITEHEADER, reinterpret_cast<void *>(this)) != CURLE_OK))
        throw std::runtime_error("in Downloader::init: curl_easy_setopt() failed (2)!");

    if (params_.debugging_) {
        if (unlikely(::curl_easy_setopt(easy_handle_, CURLOPT_DEBUGDATA, reinterpret_cast<void *>(this)) != CURLE_OK))
            throw std::runtime_error("in Downloader::init: curl_easy_setopt() failed (3)!");
    }

    if (params_.ignore_ssl_certificates_) {
        if (unlikely(::curl_easy_setopt(easy_handle_, CURLOPT_SSL_VERIFYPEER, 0L) != CURLE_OK))
            throw std::runtime_error("in Downloader::init: curl_easy_setopt() failed (4)!");
        if (unlikely(::curl_easy_setopt(easy_handle_, CURLOPT_SSL_VERIFYHOST, 0L) != CURLE_OK))
            throw std::runtime_error("in Downloader::init: curl_easy_setopt() failed (5)!");
    }

    if (not params_.proxy_host_and_port_.empty())  {
        if (unlikely(::curl_easy_setopt(easy_handle_, CURLOPT_PROXY, params_.proxy_host_and_port_.c_str())
                     != CURLE_OK))
            throw std::runtime_error("in Downloader::init: curl_easy_setopt() failed (6)!");
    }
}


void Downloader::InitCurlEasyHandle(const long dns_cache_timeout, const char * const error_buffer,
                                    const bool debugging, WriteFunc write_func, LockFunc lock_func,
                                    UnlockFunc unlock_func, HeaderFunc header_func,
                                    DebugFunc debug_func, CURL ** const easy_handle, std::string * const user_agent,
                                    const bool follow_redirects)
{
    *easy_handle = ::curl_easy_init();
    if (unlikely(*easy_handle == nullptr))
        throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_init() failed!");

    if (share_handle_ == nullptr) {
        share_handle_ = ::curl_share_init( );
        if (unlikely(share_handle_ == nullptr))
            throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_share_init() failed!");
        dns_mutex_ = new std::mutex;
        cookie_mutex_ = new std::mutex;
        if (unlikely(::curl_share_setopt(share_handle_, CURLSHOPT_LOCKFUNC, lock_func) != 0))
            throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_share_setopt() failed (1)!");
        if (unlikely(::curl_share_setopt(share_handle_, CURLSHOPT_UNLOCKFUNC, unlock_func) != 0))
            throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_share_setopt() failed (2)!");
        if (unlikely(::curl_share_setopt(share_handle_, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS) != 0))
            throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_share_setopt() failed (3)!");
        if (unlikely(::curl_share_setopt(share_handle_, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE) != 0))
            throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_share_setopt() failed (4)!");
        robots_dot_txt_mutex_ = new std::mutex;
    }

    if (unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_SHARE, share_handle_) != CURLE_OK))
        throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (0)!");

    if (debugging and unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_VERBOSE, 1L) != CURLE_OK))
        throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (1)!");

    // Do not include headers in the data provided to the CURLOPT_WRITEDATA callback:
    if (unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_HEADER, 0L) != CURLE_OK))
        throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (2)!");

    if (unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_NOPROGRESS, 1L) != CURLE_OK))
        throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (3)!");

    if (unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_NOSIGNAL, 1L) != CURLE_OK))
        throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (4)!");

    if (unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_WRITEFUNCTION, write_func) != CURLE_OK))
        throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (5)!");

    // Enabling the following option seems to greatly slow down the downloading of Web pages!
    //  if (unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_IGNORE_CONTENT_LENGTH, 1L) != CURLE_OK))
    //          throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (6)!");

    long should_follow = follow_redirects ? 1L : 0L;
    if (unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_FOLLOWLOCATION, should_follow) != CURLE_OK))
        throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (7)!");

    if (unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_DNS_CACHE_TIMEOUT, dns_cache_timeout) != CURLE_OK))
        throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (8)!");

    if (unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_HEADERFUNCTION, header_func) != CURLE_OK))
        throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (9)!");

    // User agent information:
    *user_agent = Downloader::DEFAULT_USER_AGENT_STRING;
    if (unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_USERAGENT, user_agent->c_str()) != CURLE_OK))
        throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (10)!");

    // Disable `passive' FTP operation:
    if (unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_FTPPORT, "-") != CURLE_OK))
        throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (11)!");

    if (unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_ERRORBUFFER, error_buffer) != CURLE_OK))
        throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (12)!");

    // Enable automatic setting of the "Referer" HTTP-header field when following a "Location:" redirect:
    if (unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_AUTOREFERER, 1L) != CURLE_OK))
        throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (13)!");

    if (debugging) {
        if (unlikely(::curl_easy_setopt(*easy_handle, CURLOPT_DEBUGFUNCTION, debug_func) != CURLE_OK))
            throw std::runtime_error("in Downloader::InitCurlEasyHandle: curl_easy_setopt() failed (14)!");
    }
}


bool Downloader::internalNewUrl(const Url &url, const TimeLimit &time_limit) {
    body_.clear();

    redirect_urls_.push_back(url);

    if ((curl_error_code_ = ::curl_easy_setopt(easy_handle_, CURLOPT_URL, url.c_str())) != CURLE_OK)
        return false;
    if ((curl_error_code_ = ::curl_easy_setopt(easy_handle_, CURLOPT_TIMEOUT, time_limit.getRemainingTime())))
        return false;

    // Add additional HTTP headers:
    if (url.isValidWebUrl() and additional_http_headers_ != nullptr)
        ::curl_easy_setopt(easy_handle_, CURLOPT_HTTPHEADER, additional_http_headers_);

    if (not multi_mode_) {
        curl_error_code_ = ::curl_easy_perform(easy_handle_);
        return curl_error_code_ == CURLE_OK;
    } else
        return false;
}


size_t Downloader::writeFunction(void *data, size_t size, size_t nmemb) {
    const size_t total_size(size * nmemb);
    body_.append(reinterpret_cast<char *>(data), total_size);
    return total_size;
}


size_t Downloader::WriteFunction(void *data, size_t size, size_t nmemb, void *this_pointer) {
    Downloader *downloader(reinterpret_cast<Downloader *>(this_pointer));
    return downloader->writeFunction(data, size, nmemb);
}


void Downloader::LockFunction(CURL */* handle */, curl_lock_data data, curl_lock_access /* access */,
                              void */* unused */)
{
    if (data == CURL_LOCK_DATA_DNS)
        Downloader::dns_mutex_->lock();
    else if (data == CURL_LOCK_DATA_COOKIE)
        Downloader::cookie_mutex_->lock();
}


void Downloader::UnlockFunction(CURL */* handle */, curl_lock_data data, void */* unused */) {
    if (data == CURL_LOCK_DATA_DNS)
        Downloader::dns_mutex_->unlock();
    else if (data == CURL_LOCK_DATA_COOKIE)
        Downloader::cookie_mutex_->unlock();
}


size_t Downloader::headerFunction(void *data, size_t size, size_t nmemb) {
    const size_t total_size(size * nmemb);
    const std::string chunk(reinterpret_cast<char *>(data), total_size);
    header_ += chunk;

    // Look for "Location:" fields when dealing with HTTP or HTTPS:
    if (current_url_.isValidWebUrl()) {
        const char * const location_plus_colon(::strcasestr(chunk.c_str(), "Location:"));
        if (location_plus_colon != nullptr) {
            std::string redirect_url(location_plus_colon + 9);
            StringUtil::TrimWhite(&redirect_url);
            if (not redirect_url.empty())
                redirect_urls_.push_back(Url(redirect_url, redirect_urls_.back()).toString());
        }
    }

    return total_size;
}


size_t Downloader::HeaderFunction(void *data, size_t size, size_t nmemb, void *this_pointer) {
    Downloader *downloader(reinterpret_cast<Downloader *>(this_pointer));
    return downloader->headerFunction(data, size, nmemb);
}


void Downloader::debugFunction(CURL */* handle */, curl_infotype infotype, char *data, size_t size) {
    switch (infotype) {
    case CURLINFO_TEXT:
        std::cerr << "Informational text:\n" << std::string(data, size) << std::endl;
        break;
    case CURLINFO_HEADER_OUT:
        std::cerr << "Sent header:\n" << std::string(data, size) << std::endl;
        break;
    default:
        break;
    }
}


int Downloader::DebugFunction(CURL *handle, curl_infotype infotype, char *data, size_t size, void *this_pointer) {
    Downloader *downloader(reinterpret_cast<Downloader *>(this_pointer));
    downloader->debugFunction(handle, infotype, data, size);

    return 0;
}


bool Downloader::allowedByRobotsDotTxt(const Url &url, const TimeLimit &time_limit) {
    if (not url.isValid())
        return false;

    // If the protocol is not HTTP or HTTPS we won't check robots.txt:
    if (not url.isValidWebUrl())
        return true;

    const std::string robots_txt_url(url.getRobotsDotTxtUrl());
    if (robots_txt_url.empty() or ::strcasecmp(robots_txt_url.c_str(), url.c_str()) == 0)
        return true;

    {
        // Check to see if we already have a robots.txt object for the current URL:
        std::lock_guard<std::mutex> mutex_locker(*robots_dot_txt_mutex_);
        const std::unordered_map<std::string, RobotsDotTxt>::const_iterator
            url_and_robots_dot_txt(url_to_robots_dot_txt_map_.find(robots_txt_url));
        if (url_and_robots_dot_txt != url_to_robots_dot_txt_map_.end())
            return url_and_robots_dot_txt->second.accessAllowed(getUserAgent(), url);
    }

    //
    // If we made it here we don't yet have a robots.txt object for the current URL!
    //

    // Site doesn't have a robots.txt or for some reason we couldn't get it?
    if (not internalNewUrl(Url(robots_txt_url), time_limit)) {
        std::lock_guard<std::mutex> mutex_locker(*robots_dot_txt_mutex_);
        url_to_robots_dot_txt_map_.insert(std::make_pair(robots_txt_url, RobotsDotTxt()));
        return true;
    }

    RobotsDotTxt new_robots_txt(body_);
    std::lock_guard<std::mutex> mutex_locker(*robots_dot_txt_mutex_);
    url_to_robots_dot_txt_map_.insert(std::make_pair(robots_txt_url, new_robots_txt));

    return new_robots_txt.accessAllowed(params_.user_agent_, url.getPath());
}


void Downloader::GlobalCleanup(const bool forever) {
    if (unlikely(GetInstanceCount() != 0))
        throw std::runtime_error("in Downloader::GlobalCleanup: can't cleanup with existing instances of class Downloader!");

    if (share_handle_ != nullptr) {
        ::curl_share_cleanup(share_handle_);
        share_handle_ = nullptr;
    }

    delete dns_mutex_,  dns_mutex_ = nullptr;
    delete cookie_mutex_,  cookie_mutex_ = nullptr;
    delete robots_dot_txt_mutex_,  robots_dot_txt_mutex_ = nullptr;

    if (forever)
        ::curl_global_cleanup();
}


const PerlCompatRegExps &Downloader::GetBannedUrlRegExps() {
    static bool initialised(false);
    static PerlCompatRegExps ini_file_reg_exps(PerlCompatRegExp::DONT_OPTIMIZE_FOR_MULTIPLE_USE, PCRE_CASELESS);
    if (not initialised) {
        initialised = true;
        const IniFile ini_file(ETC_DIR "/BannedUrlRegExps.conf");
        const std::list<std::string> entry_names(ini_file.getSectionEntryNames(""));
        for (std::list<std::string>::const_iterator entry_name(entry_names.begin()); entry_name != entry_names.end();
             ++entry_name)
            ini_file_reg_exps.addPattern(ini_file.getString("", *entry_name));
    }

    return ini_file_reg_exps;
}


const std::string &Downloader::GetDefaultUserAgentString() {
    if (default_user_agent_string_.empty())
        default_user_agent_string_ = Downloader::DEFAULT_USER_AGENT_STRING;

    return default_user_agent_string_;
}


bool Downloader::getHttpEquivRedirect(std::string * const redirect_url) const {
    redirect_url->clear();

    if (not current_url_.isValidWebUrl() or header_.empty())
        return false;

    std::vector<std::string> headers;
    SplitHttpHeaders(header_, &headers);

    // Only look for redirects in Web pages:
    const std::string media_type(MediaTypeUtil::GetMediaType(HttpHeader(headers.back()), body_));
    if (media_type != "text/html" and media_type != "text/xhtml")
        return false;

    // Look for HTTP-EQUIV "Refresh" meta tags:
    std::list< std::pair<std::string, std::string> > refresh_meta_tags;
    HttpEquivExtractor http_equiv_extractor(body_, "refresh", &refresh_meta_tags);
    http_equiv_extractor.parse();
    if (refresh_meta_tags.empty())
        return false;

    std::string delay, url_and_possible_junk;
    if (not StringUtil::SplitOnStringThenTrimWhite(refresh_meta_tags.front().second, ";", &delay,
                                                   &url_and_possible_junk))
        return false;

    const char * const url_and_equal_sign(::strcasestr(url_and_possible_junk.c_str(), "url="));
    if (url_and_equal_sign != nullptr)
        *redirect_url = url_and_equal_sign + 4;
    else
        *redirect_url = url_and_possible_junk;
    StringUtil::Trim(redirect_url);

    return not redirect_url->empty();
}


const std::string WGET("/usr/bin/wget");


int Download(const std::string &url, const std::string &output_filename, const unsigned timeout,
             const std::string &cookie_file)
{
    std::vector<std::string> args;
    args.push_back("--quiet");
    args.push_back(url);
    args.push_back("-O");
    args.push_back(output_filename);
    args.push_back("--tries=1");
    if (timeout != 0)
        args.push_back("--timeout=" + std::to_string(timeout));
    if (not cookie_file.empty()) {
        args.push_back("--load-cookies");
        args.push_back(cookie_file);
        args.push_back("--save-cookies");
        args.push_back(cookie_file);
        args.push_back("--keep-session-cookies");
    }

    return ExecUtil::Exec(WGET, args);
}


int Download(const std::string &url, const unsigned timeout, std::string * const output,
             const std::string &cookie_file)
{
    const FileUtil::AutoTempFile auto_temp_file;
    const std::string &output_filename(auto_temp_file.getFilePath());
    const int retval = Download(url, output_filename, timeout, cookie_file);
    if (retval == 0) {
        if (not FileUtil::ReadString(output_filename, output))
            return -1;
    }

    return retval;
}
