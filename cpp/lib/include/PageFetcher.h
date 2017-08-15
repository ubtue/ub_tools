/** \file    PageFetcher.h
 *  \brief   Declaration of class PageFetcher, a simple Web page retrieval class.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Paul Vander Griend
*/

/*
 *  \copyright 2003-2009 Project iVia.
 *  \copyright 2003-2009 The Regents of The University of California.
 *  \copyright 2017 Universitätsbibliothek Tübingen.
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

#ifndef PAGE_FETCHER_H
#define PAGE_FETCHER_H


#include <stdexcept>
#include <string>
#include <vector>
#include <climits>
#include "TimeLimit.h"


// Forward declarations:
class Url;
class HttpHeader;


/** \class  PageFetcher
 *  \brief  Provides a Thread Safe facility to download Web pages using HTTP or HTTPS.
 *
 *  If a page cannot be retrieved from the server, an error is reported.  These errors include network errors (such as not being able to connect to a host)
 *  BUT NOT HTTP STATUS CODE "errors" like the familiar "404 Not Found" message).
 *
 *  This class supports robots.txt protection
 */
class PageFetcher {
    std::string data_, error_message_;
    unsigned last_error_code_;
    std::vector<std::string> redirect_urls_;
    bool transparently_unzip_content_;
    std::string user_agent_;
    std::string acceptable_languages_; // What to set the HTTP Accept-Language header to.
public:
    enum RobotsDotTxtOption { CONSULT_ROBOTS_DOT_TXT, IGNORE_ROBOTS_DOT_TXT };
private:
    static const std::string TIMEOUT_ERROR_MESSAGE;
    static const std::string NO_SUCH_DOMAIN_ERROR_MESSAGE;
    static const std::string ROBOTS_DOT_TXT_ERROR_MESSAGE;
    RobotsDotTxtOption robots_dot_txt_option_;
public:
    PageFetcher(): error_message_("uninitialized!"), last_error_code_(UINT_MAX) { }

    /** \brief  Creates a PageFetcher object.
     *  \param  url                          The URL of the Web page that is to be retrieved.
     *  \param  additional_http_headers      If non-empty, these headers will be included in the HTTP GET request.
     *                                       Used for example for "Cookie:" headers.
     *  \param  time_limit                   The maximum amount of time to be used to retrieve the Web page in
     *                                       milliseconds.
     *  \param  max_redirects                Up to how many redirects to follow.
     *  \param  ignore_redirect_errors       Typically used with "max_redirects" set to 0 in order to process
     *                                       redirects at a higher level.
     *  \param  transparently_unzip_content  If true, translate content and header for pages with content type
     *                                       "application/x-gzip".
     *  \param  user_agent                   The user agent string to send to the Web server.
     *  \param  acceptable_languages         The requested languages encodings to send to the Web server.  Must be
     *                                       empty or a comma-separated list of language codes.  An empty list implies
     *                                       that all language codes will be accepted.
     *  \param  robots_dot_txt_option        Option to consult or ignore robots.txt files.
     */
    explicit PageFetcher(const std::string &url, const std::string &additional_http_headers,
                         const TimeLimit &time_limit = TimeLimit(20000),
                         const unsigned max_redirects = 7, const bool ignore_redirect_errors = false,
                         const bool transparently_unzip_content = true,
                         const std::string &user_agent = "iVia Page Fetcher (http://ivia.ucr.edu/useragents.shtml)",
                         const std::string &acceptable_languages = "",
                         const RobotsDotTxtOption robots_dot_txt_option = IGNORE_ROBOTS_DOT_TXT);

    /** \brief  Creates a PageFetcher object.
     *  \param  url                          The URL of the Web page that is to be retrieved.
     *  \param  additional_http_headers      If non-empty, these headers will be included in the HTTP GET request.
     *                                       Used for example for "Cookie:" headers.
     *  \param  proxy_host                   The optional hostname of a Web proxy.
     *  \param  proxy_port                   The optional port of a Web proxy.
     *  \param  time_limit                   The maximum amount of time to be used to retrieve the Web page in
     *                                       milliseconds.
     *  \param  max_redirects                Up to how many redirects to follow.
     *  \param  ignore_redirect_errors       Typically used with "max_redirects" set to 0 in order to process
     *                                       redirects at a higher level.
     *  \param  transparently_unzip_content  If true, translate content and header for pages with content type
     *                                       "application/x-gzip".
     *  \note   If "proxy_host" is non-empty all requests will be forwarded via "proxy_host" and "proxy_port".
     *  \param  user_agent                   The user agent string to send to the web server.
     *  \param  acceptable_languages         The requested languages encodings to send to the Web server.  Must be
     *                                       empty or a comma-separated list of language codes.  An empty list implies
     *                                       that all language codes will be accepted.
     *  \param  robots_dot_txt_option        Option to consult or ignore robots.txt files.
     */
    PageFetcher(const std::string &url, const std::string &additional_http_headers = "",
                const std::string &proxy_host = "", const unsigned short proxy_port = 0,
                const TimeLimit &time_limit = TimeLimit(20000), const unsigned max_redirects = 7,
                const bool ignore_redirect_errors = false, const bool transparently_unzip_content = true,
                const std::string &user_agent = "UB Tools Page Fetcher (http://ivia.ucr.edu/useragents.shtml)",
                const std::string &acceptable_languages = "",
                const RobotsDotTxtOption robots_dot_txt_option = IGNORE_ROBOTS_DOT_TXT);

    const std::string &getData() const throw(std::exception);
    bool anErrorOccurred() const { return not error_message_.empty(); }
    unsigned getLastErrorCode() const { return last_error_code_; }
    bool ignoringRobotsDotTxt() const { return robots_dot_txt_option_ == IGNORE_ROBOTS_DOT_TXT; }

    /** Returns a meaningful textual message describing an error if anErrorOccurred returned true. */
    const std::string &getErrorMsg() const { return error_message_; }

    /** Returns the timeout error message so external programs and verify a timout occured instead of a more fatal
        error. */
    const std::string &getTimeoutErrorMsg() const  { return TIMEOUT_ERROR_MESSAGE; }

    /** Returns the timeout error message so external programs and verify that a DNS lookup failed.  (But not due to a
        timeout!) */
    const std::string &getNoSuchDomainErrorMsg() const  { return NO_SUCH_DOMAIN_ERROR_MESSAGE; }

    /** Returns the robots.txt error message so external programs and verify that an error occurred due to robots.txt
        instead of a more fatal error. */
    const std::string &getRobotsDotErrorMsg() const  { return ROBOTS_DOT_TXT_ERROR_MESSAGE; }

    /** Returns how many redirections where encountered when attempting to download the requested resource. */
    unsigned getRedirectCount() const { return redirect_urls_.size() - 1; }

    /** Returns the final URL of a chain of redirections. */
    std::string getRedirectedUrl() const { return redirect_urls_.back(); }

    /** \note Returns \em{all} URLs encountered in downloading the last document, including the original URL. */
    const std::vector<std::string> &getRedirectUrls() const { return redirect_urls_; }

    const std::string getUserAgent() const { return user_agent_; }

    /** \brief   Split the HTTP Message Headers from the Message Body of a document.
     *  \param   header_and_body  The PageFetcher HTTP response, including one or more headers
     *                            and a body.
     *  \param   all_headers      A pointer to a string that will hold the headers.
     *  \param   message_body     A pointer to a string that will hold the message body if the pointer is non-NULL.
     *  \return  True if we were able to parse the headers correctly, false otherwise.
     *
     *  \note    When pages are redirected, PageFetcher provides multiple sets of headers for each document.  This
     *           function separates all the headers from the body.
     *  \note    When we cannot parse the headers correctly, we assume that no headers were returned.
     */
    static bool SplitHttpHeadersFromBody(const std::string &header_and_body, std::string * const all_headers,
                                         std::string * const message_body = nullptr);

private:
    void fetchPage(const std::string &url, const std::string &proxy_host, const unsigned short proxy_port,
                   const TimeLimit &time_limit, const unsigned max_redirects, const bool ignore_redirect_errors,
                   const std::string &additional_http_headers, const bool consult_robots_dot_txt);
    bool getPage(const std::string &url, const std::string &referrer, const std::string &proxy_host,
                 const unsigned short proxy_port, const TimeLimit &time_limit,
                 const std::string &additional_http_headers, std::string * const data);
    bool deniedByRobotsDotTxt(const Url &url, const std::string &proxy_host, const unsigned short proxy_port,
                              const TimeLimit &time_limit);
    bool getHttpEquivRedirect(const Url &current_url, const HttpHeader &current_header,
                              const std::string &current_body, std::string * const redirect_url) const;
};


#endif // ifndef PAGE_FETCHER_H
