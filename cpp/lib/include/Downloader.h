/** \file   Downloader.h
 *  \brief  Functions for downloading of web resources.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2005-2008 Project iVia.
 *  \copyright 2005-2008 The Regents of The University of California.
 *  \copyright 2015-2017 Universitätsbibliothek Tübingen.
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
#ifndef DOWNLOADER_H
#define DOWNLOADER_H


#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <curl/curl.h>
#include "Compiler.h"
#include "HttpHeader.h"
#include "RobotsDotTxt.h"
#include "PerlCompatRegExp.h"
#include "TimeLimit.h"
#include "Url.h"


/** \class  Downloader
 *  \brief  Implements an object that can download Web pages, grab files off of FTP servers etc.
 */
class Downloader {
    CURL *easy_handle_;
    static CURLSH *share_handle_;
    static unsigned instance_count_;
    static std::mutex *dns_mutex_;
    static std::mutex *cookie_mutex_;
    static std::mutex *robots_dot_txt_mutex_;
    static std::unordered_map<std::string, RobotsDotTxt> url_to_robots_dot_txt_map_;
protected:
    bool multi_mode_;
public:
    static const long DEFAULT_MAX_REDIRECTS     = 10;
    static const long MAX_MAX_REDIRECT_COUNT    = 20;
    static const long DEFAULT_DNS_CACHE_TIMEOUT = 600;
    static const std::string DEFAULT_USER_AGENT_STRING;
    static const std::string DEFAULT_ACCEPTABLE_LANGUAGES;
    static const unsigned DEFAULT_TIME_LIMIT    = 20000; // In ms.
private:
    CURLcode curl_error_code_;
    mutable std::string last_error_message_;
    std::string header_;
    std::string body_;
    std::vector<std::string> redirect_urls_;
    char error_buffer_[CURL_ERROR_SIZE];
    Url current_url_;
    curl_slist *additional_http_headers_;
    static std::string default_user_agent_string_;
public:
    enum TextTranslationMode {
        TRANSPARENT   = 0, //< If set, perform no character set translations.
        MAP_TO_LATIN9 = 1  //< If set, attempt to convert from whatever to Latin-9.  Note: Currently only used for HTTP and HTTPS!
    };

    static std::string DENIED_BY_ROBOTS_DOT_TXT_ERROR_MSG;

    struct Params {
        std::string user_agent_;
        std::string acceptable_languages_;
        long max_redirect_count_; // Must always be between 0 and MAX_MAX_REDIRECT_COUNT.
        long dns_cache_timeout_;  // How long to keep cache entries around.  (In seconds.)  -1 means forever
        bool honour_robots_dot_txt_;
        TextTranslationMode text_translation_mode_;
        PerlCompatRegExps banned_reg_exps_; // Do not download anything matching these regular expressions.
        bool debugging_;
        bool follow_redirects_;

    public:
        explicit Params(const std::string &user_agent = DEFAULT_USER_AGENT_STRING,
                        const std::string &acceptable_languages = DEFAULT_ACCEPTABLE_LANGUAGES,
                        const long max_redirect_count = DEFAULT_MAX_REDIRECTS,
                        const long dns_cache_timeout = DEFAULT_DNS_CACHE_TIMEOUT,
                        const bool honour_robots_dot_txt = false,
                        const TextTranslationMode text_translation_mode = MAP_TO_LATIN9,
                        const PerlCompatRegExps &banned_reg_exps = PerlCompatRegExps(), const bool debugging = false,
                        const bool follow_redirects = true);
    } params_;

    typedef size_t (*WriteFunc)(void *data, size_t size, size_t nmemb, void *this_pointer);
    typedef void (*LockFunc)(CURL *handle, curl_lock_data data, curl_lock_access access, void */* unused */);
    typedef void (*UnlockFunc)(CURL *handle, curl_lock_data data, void */* unused */);
    typedef size_t (*HeaderFunc)(void *data, size_t size, size_t nmemb, void *this_pointer);
    typedef int (*DebugFunc)(CURL *handle, curl_infotype infotype, char *data, size_t size, void *this_pointer);
public:
    explicit Downloader(const Params &params = Params()): multi_mode_(false), additional_http_headers_(nullptr),
                                                          params_(params) { init(); }
    explicit Downloader(const Url &url, const Params &params = Params(),
                        const TimeLimit &time_limit = DEFAULT_TIME_LIMIT);
    explicit Downloader(const std::string &url, const Params &params = Params(),
                        const TimeLimit &time_limit = DEFAULT_TIME_LIMIT, bool multimode = false);
    virtual ~Downloader();

    bool newUrl(const Url &url, const TimeLimit &time_limit = DEFAULT_TIME_LIMIT);
    bool newUrl(const std::string &url, const TimeLimit &time_limit = DEFAULT_TIME_LIMIT)
        { return newUrl(Url(url), time_limit); }

    std::string getMessageHeader() const;
    const std::string &getMessageBody() const { return body_; }

    /** \brief  Tries its best to get the MIME type of the most recently downloaded document.
     *  \param  auto_simplify  If true, anything following the first semicolon, or if that can't be located, the first
     *                         space will be removed together with the semicolon or space, if present.
     *  \return A good guess as to the MIME type of the most recently downloaded document on success or an empty
     *          string upon failure.
     */
    std::string getMediaType(const bool auto_simplify = true) const;

    /** \note Returns \em{all} URLs encountered in downloading the last document, including the original URL. */
    const std::vector<std::string> &getRedirectUrls() const { return redirect_urls_; }

    bool anErrorOccurred() const { return curl_error_code_ != CURLE_OK; }
    const std::string &getLastErrorMessage() const;
    const std::string &getUserAgent() const { return params_.user_agent_; }

    static unsigned GetInstanceCount() { return instance_count_; }

    /** \brief    Get's rid of all memory allocations related to Downloader instances.
     *  \param    forever  If true, no new instances can be created in the current process!
     *  \warning  Must only be called when there are no more existing Downloader instances!
     */
    static void GlobalCleanup(const bool forever);

    /** \brief  Returns the default list of banned URL regular expressions as found in BannedUrlRegExps.conf.
     */
    static const PerlCompatRegExps &GetBannedUrlRegExps();

    static const std::string &GetDefaultUserAgentString();

protected:
    void setMultiMode(const bool multi) { multi_mode_ = multi; }

    bool getMultiMode() const { return multi_mode_ ; }

    CURL *getEasyHandle() const { return easy_handle_; }

private:
    static void InitCurlEasyHandle(const long dns_cache_timeout, const char * const error_buffer,
                                   const bool debugging, WriteFunc write_func, LockFunc lock_func,
                                   UnlockFunc unlock_func, HeaderFunc header_func,
                                   DebugFunc debug_func, CURL ** const easy_handle, std::string * const user_agent,
                                   const bool follow_redirect);
    void init();
    bool internalNewUrl(const Url &url, const TimeLimit &time_limit);
    size_t writeFunction(void *data, size_t size, size_t nmemb);
    static size_t WriteFunction(void *data, size_t size, size_t nmemb, void *this_pointer);
    static void LockFunction(CURL *handle, curl_lock_data data, curl_lock_access access, void */* unused */);
    static void UnlockFunction(CURL *handle, curl_lock_data data, void */* unused */);
    size_t headerFunction(void *data, size_t size, size_t nmemb);
    static size_t HeaderFunction(void *data, size_t size, size_t nmemb, void *this_pointer);
    void debugFunction(CURL *handle, curl_infotype infotype, char *data, size_t size);
    static int DebugFunction(CURL *handle, curl_infotype infotype, char *data, size_t size, void *this_pointer);
    bool allowedByRobotsDotTxt(const Url &url, const TimeLimit &time_limit);
    bool getHttpEquivRedirect(std::string * const redirect_url) const;
    long getRemainingNoOfRedirects() const
        { return params_.max_redirect_count_ - static_cast<long>(redirect_urls_.size()); }
};


/** \brief Downloads a Web document.
 *  \param url              The address.
 *  \param output_filename  Where to store the downloaded document.
 *  \param timeout          Max. amount of time to try to download a document in seconds.
 *  \param cookie_file      Cookies will be read before the attempted download and later stored here.
 *  \return Exit code of the child process.  0 upon success.
 */
int Download(const std::string &url, const std::string &output_filename, const unsigned timeout,
             const std::string &cookie_file = "");


/** \brief Downloads a Web document.
 *  \param url      The address.
 *  \param timeout  Max. amount of time to try to download a document in seconds.
 *  \param output   Where to store the downloaded document.
 *  \param cookie_file      Cookies will be read before the attempted download and later stored here.
 *  \return Exit code of the child process.  0 upon success.
 */
int Download(const std::string &url, const unsigned timeout, std::string * const output,
             const std::string &cookie_file = "");


#endif // ifndef DOWNLOADER_H
