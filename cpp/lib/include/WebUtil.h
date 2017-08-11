/** \file    WebUtil.h
 *  \brief   WWW related utility functions.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Artur Kedzierski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2002-2009 Project iVia.
 *  Copyright 2002-2009 The Regents of The University of California.
 *  Copyright 2017 Universitätsbibliothek Tübingen
 *
 *  This file is part of the libiViaCore package.
 *
 *  The libiViaCore package is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  libiViaCore is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with libiViaCore; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef WEB_UTIL_H
#define WEB_UTIL_H


#include <map>
#include <set>
#include <string>
#include "FileUtil.h"
#include "TimeLimit.h"
#include "Url.h"


// Forward declaration:
class HttpHeader;


using StringMap = std::map<std::string, std::string>;


namespace WebUtil {


/** The default timeout (in milliseconds) for WebUtil functions that perform Internet operations. */
const unsigned DEFAULT_DOWNLOAD_TIMEOUT(20000);


/** \brief  www-form-urlencodes a list of name/value pairs.
 *  \param  post_args                                         The name/value pairs to be encoded.
 *  \param  generate_content_type_and_content_length_headers  If true generates the "Content-Type" and "Content-Length"
 *                                                            headers.
 */
std::string WwwFormUrlEncode(const StringMap &post_args, const bool generate_content_type_and_content_length_headers = true);


/** \brief  Posts data via HTTP POST and retrieves the returned data.
 *  \param  username_password    A colon-separated username/password pair.  Currently we only support "Basic"
 *                               authorization!
 *  \param  address              The IP address or domain name.
 *  \param  port                 The TCP port number (typically 80).
 *  \param  time_limit           Up to how long to wait for the Web server to respond (in milliseconds).
 *  \param  path                 URL path to the form we would like to execute.
 *  \param  post_args            A list of name/value pairs.
 *  \param  document_source      The output of the CGI script.
 *  \param  error_message        If an error occurs, a description of the error will be stored here..
 *  \param  accept               List of comman separated media types which are acceptable for the response.
 *  \param  include_http_header  Prepend the HTTP header to the document source if this is "true".
 *  \return True if no error occurred, otherwise false.
 */
bool ProcessPOST(const std::string &username_password, const std::string &address, const unsigned short port,
                 const TimeLimit &time_limit, const std::string &path, const StringMap &post_args,
                 std::string * const document_source, std::string * const error_message,
                 const std::string &accept = "text/html,text/xhtml,text/plain,www/source", const bool include_http_header = false);


/** \brief  Excutes a CGI script via POST.
 *  \param  address              The IP address or domain name.
 *  \param  port                 The TCP port number (typically 80).
 *  \param  time_limit           Up to how long to wait for the Web server to respond (in milliseconds).
 *  \param  path                 URL path to the form we would like to execute.
 *  \param  post_args            A list of name/value pairs.
 *  \param  document_source      The output of the CGI script.
 *  \param  error_message        If an error occurs, a description of the error will be stored here..
 *  \param  accept               List of comman separated media types which are acceptable for the response.
 *  \param  include_http_header  Prepend the HTTP header to the document source if this is "true".
 *  \return True if no error occurred, otherwise false.
 */
inline bool ProcessPOST(const std::string &address, const unsigned short port, const TimeLimit &time_limit,
                        const std::string &path, const StringMap &post_args, std::string * const document_source,
                        std::string * const error_message,
                        const std::string &accept = "text/html,text/xhtml,text/plain,www/source",
                        const bool include_http_header = false)
{
        return ProcessPOST("", address, port, time_limit, path, post_args, document_source, error_message, accept,
                           include_http_header);
}


/**  \brief  Attempts to convert common Web date/time formats to a time_t.
 *   \param  possible_web_date_and_time  The mess we're trying to understand.
 *   \return If the parse succeeded, the converted time, otherwise TimeUtil::BAD_TIME_T.
 */
time_t ParseWebDateAndTime(const std::string &possible_web_date_and_time);


std::string ConvertToLatin9(const HttpHeader &http_header, const std::string &original_document);


/** \brief  Attempt to guess the file type of "url".
 *  \param  url  The URL for the resource whose file type we would like to determine.
 *  \return The guessed file type.
 */
FileUtil::FileType GuessFileType(const std::string &url);


/** \brief   Attempt to guess the media type of an URL.
 *  \param   url  The URL of the resource whose file type we would like to guess.
 *  \return  The guessed media type, or an empty string for none.
 *
 *  The media type (a.k.a. mime type) is defined here:
 *  http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.7
 */
std::string GuessMediaType(const std::string &url);


/** \brief  Parse all arguments from HTTP POST (via std::cin) into a multimap. */
void GetPostArgs(std::multimap<std::string, std::string> * const post_args);


/** \brief  Parse all multipart arguments (via std::cin) into a multimap. */
void GetMultiPartArgs(std::multimap<std::string, std::string> * const post_args, const bool save_file_to_disk = true);


/** \brief  Parse all arguments from HTTP GET (via std::cin) into a multimap. */
void GetGetArgs(std::multimap<std::string, std::string> * const get_args);


/** \brief  Parse all arguments from the command line into a multimap. */
void GetArgvArgs(const int argc, char * argv[], std::multimap<std::string, std::string> * const argv_args);


/** \brief  Obtains all arguments from CGI (submitted in GET or POST methods or provided on the command line).
 *  \param  cgi_args  The map that holds that variable -> value relation upon exit.
 *  \param  argc      The argument count as provided to the main function.
 *  \param  argv      The argument list as provided to the main function.
 *  \note   If "argc" and "argv" are set to their default values only HTTP GET and POST arguments will be extracted!
*/
void GetAllCgiArgs(std::multimap<std::string, std::string> * const cgi_args, int argc = 1, char *argv[] = NULL);


/** \brief  Excutes a CGI script via POST.
 *  \param  username_password    A colon-separated username/password pair.  Currently we only support "Basic"
 *                               authorization!
 *  \param  url                  Nomen est omen.
 *  \param  time_limit           Up to how long to wait for the Web server to respond (in milliseconds).
 *  \param  post_args            A list of name/value pairs.
 *  \param  document_source      The output of the CGI script.
 *  \param  error_message        If an error occurs, a description of the error will be stored here..
 *  \param  accept               List of comman separated media types which are acceptable for the response.
 *  \param  include_http_header  Prepend the HTTP header to the document source if this is "true".
 *  \return True if no error occurred, otherwise false.
 */
bool ExecPostHTTPRequest(const std::string &username_password, const Url &url, const TimeLimit &time_limit,
                         const StringMap &post_args, std::string * const document_source,
                         std::string * const error_message,
                         const std::string &accept = "text/html,text/xhtml,text/plain,www/source",
                         const bool include_http_header = false);


/** \brief  Excutes a CGI script via POST.
 *  \param  url                  Nomen est omen.
 *  \param  time_limit           Up to how long to wait for the Web server to respond (in milliseconds).
 *  \param  post_args            A list of name/value pairs.
 *  \param  document_source      The output of the CGI script.
 *  \param  error_message        If an error occurs, a description of the error will be stored here..
 *  \param  accept               List of comman separated media types which are acceptable for the response.
 *  \param  include_http_header  Prepend the HTTP header to the document source if this is "true".
 *  \return True if no error occurred, otherwise false.
 */
inline bool ExecPostHTTPRequest(const Url &url, const TimeLimit &time_limit, const StringMap &post_args,
                                std::string * const document_source, std::string * const error_message,
                                const std::string &accept = "text/html,text/xhtml,text/plain,www/source",
                                const bool include_http_header = false)
{
    return ExecPostHTTPRequest("", url, time_limit, post_args, document_source, error_message, accept,
                               include_http_header);
}


/** \brief  Excutes a CGI script via GET.
 *  \param  username_password    A colon-separated username/password pair.  Currently we only support "Basic"
 *                               authorization!
 *  \param  url                  Nomen est omen.
 *  \param  time_limit           Up to how long to wait for the Web server to respond (in milliseconds).
 *  \param  args                 A list of name/value pairs.
 *  \param  document_source      The output of the CGI script.
 *  \param  error_message        If an error occurs, a description of the error will be stored here..
 *  \param  accept               List of comman separated media types which are acceptable for the response.
 *  \param  include_http_header  Prepend the HTTP header to the document source if this is "true".
 *  \return True if no error occurred, otherwise false.
 */
bool ExecGetHTTPRequest(const std::string &username_password, const Url &url, const TimeLimit &time_limit,
                        const StringMap &args, std::string * const document_source, std::string * const error_message,
                        const std::string &accept = "text/html,text/xhtml,text/plain,www/source",
                        const bool include_http_header = false);


/** \brief  Excutes a CGI script via GET.
 *  \param  url                  Nomen est omen.
 *  \param  time_limit           Up to how long to wait for the Web server to respond (in milliseconds).
 *  \param  args                 A list of name/value pairs.
 *  \param  document_source      The output of the CGI script.
 *  \param  error_message        If an error occurs, a description of the error will be stored here..
 *  \param  accept               List of comman separated media types which are acceptable for the response.
 *  \param  include_http_header  Prepend the HTTP header to the document source if this is "true".
 *  \return True if no error occurred, otherwise false.
 */
inline bool ExecGetHTTPRequest(const Url &url, const TimeLimit &time_limit, const StringMap &args,
                               std::string * const document_source, std::string * const error_message,
                               const std::string &accept = "text/html,text/xhtml,text/plain,www/source",
                               const bool include_http_header = false)
{
    return ExecGetHTTPRequest("", url, time_limit, args, document_source, error_message, accept, include_http_header);
}


/** \brief   Identify the "top" site that this page is part of
 *  \param   url  The URL whose site we want.
 *  \return  A string identifying the "major site" relevant to this URL.
 *
 *  This function takes a URL and returns an identifier for its "major" site; that is based on the topmost registered
 *  domain in its path.  For example, "http://www.somewhere.com" would return "somewhere.com" and
 *  "http://news.fred.co.nz" would return "fred.co.nz".
 */
std::string GetMajorSite(const Url &url);


/** \enum   ExtractedUrlForm
 *  \brief  The form of the URLs to be extracted with ExtractURLs
 */
enum ExtractedUrlForm {
    RAW_URLS,        //< The URL's as they appear in the document.
    ABSOLUTE_URLS,   //< The raw URL's converted to absolute form.
    CLEAN_URLS,      //< The absolute URL's "cleaned up".
    CANONIZED_URLS   //< The absolute URL's in Canonical form (will cause all URL's to be pre-cached).
};


/** ExtractURLs flag: Do no report blacklisted URL's. */
const unsigned IGNORE_BLACKLISTED_URLS                  = 1u << 1u;
/** ExtractURLs flag: Do no report any URL more than once. */
const unsigned IGNORE_DUPLICATE_URLS                    = 1u << 2u;
/** ExtractURLs flag: Do no report URL's that are anchored by IMG tags. */
const unsigned IGNORE_LINKS_IN_IMG_TAGS                 = 1u << 3u;
/** ExtractURLs flag: Do no report URL's on the same conceptual site, as reported by Url::getSite(). */
const unsigned IGNORE_LINKS_TO_SAME_SITE                = 1u << 4u;
/** ExtractURLs flag: Do no report URL's on the same conceptual site, as reported by WebUtil::getMajorSite(). */
const unsigned IGNORE_LINKS_TO_SAME_MAJOR_SITE          = 1u << 5u;
/** ExtractURLs flag: Remove page anchors froom URL's (i.e. anything after the final '#' character). */
const unsigned REMOVE_DOCUMENT_RELATIVE_ANCHORS         = 1u << 6u;
/** ExtractURLs flag: Ignore robots.txt files when downloading pages for the purpose of canonization (this is usually very impolite).*/
const unsigned IGNORE_ROBOTS_DOT_TXT                    = 1u << 7u;
/** ExtractURLs flag: Only return URL's whose pages can actually be downloaded (requires CANONIZED_URLS form).*/
const unsigned REQUIRE_URLS_FOR_DOWNLOADABLE_PAGES_ONLY = 1u << 8u;
/** ExtractURLs flag: Clean up the anchor text. */
const unsigned CLEAN_UP_ANCHOR_TEXT                     = 1u << 9u;
/** ExtractURLs flag: Ignore https. */
const unsigned IGNORE_PROTOCOL_HTTPS                    = 1u << 10u;
/** ExtractURLs flag: Only return onsite links. */
const unsigned KEEP_LINKS_TO_SAME_SITE_ONLY             = 1u << 11u;
/** ExtractURLs flag: Only return links pointing to the same major site. */
const unsigned KEEP_LINKS_TO_SAME_MAJOR_SITE_ONLY       = 1u << 12u;
/** ExtractURLs flag: Do our best to get URL's hidden in JavaScript code. */
const unsigned ATTEMPT_TO_EXTRACT_JAVASCRIPT_URLS       = 1u << 13u;

/** The default flags for the ExtractUrls function. */
const unsigned DEFAULT_EXTRACT_URL_FLAGS(IGNORE_DUPLICATE_URLS | IGNORE_LINKS_IN_IMG_TAGS
                                         | REMOVE_DOCUMENT_RELATIVE_ANCHORS | CLEAN_UP_ANCHOR_TEXT
                                         | IGNORE_PROTOCOL_HTTPS | ATTEMPT_TO_EXTRACT_JAVASCRIPT_URLS);

class UrlAndAnchorTexts {
    std::string url_;
    std::set<std::string> anchor_texts_;
public:
    typedef std::set<std::string>::const_iterator const_iterator;
public:
    explicit UrlAndAnchorTexts(const std::string &url): url_(url) { }
    UrlAndAnchorTexts(const std::string &url, const std::string &anchor_text)
        : url_(url) { anchor_texts_.insert(anchor_text); }
    const std::string &getUrl() const { return url_; }
    void setUrl(const std::string &new_url) { url_ = new_url; }
    void addAnchorText(const std::string &new_anchor_text) { anchor_texts_.insert(new_anchor_text); }
    const std::set<std::string> &getAnchorTexts() const { return anchor_texts_; }
    const_iterator begin() const { return anchor_texts_.begin(); }
    const_iterator end() const { return anchor_texts_.end(); }
};


/** \brief  Extracts all links from an HTML document.
 *  \param  document_source         The string containing the HTMl source.
 *  \param  default_base_url        Used to turn relative URL's into absolute URL's if requested unless a
 *                                  \<base href=...\> tag is found in "document_source".
 *  \param  extracted_url_form      The form of the extracted URL's.
 *  \param  urls                    The list of extracted URL's and their associated anchor text.
 *  \param  flags                   Behaviour modifying flags.
 *  \param  user_agent_string       The user agent string to use when downloading (CANONIZED_URLS only).
 *  \param  page_cacher_max_fanout  The number of pages to fetch in parallel CANONIZED_URLS only).
 *  \param  page_cacher_timeout     The per Web page retrieval timeout (in milliseconds, CANONIZED_URLS only)).
 *  \param  overall_timeout         Don't spend more than this amount of milliseconds time in this routine.  NULL
 *                                  means don't ever time out.
 *
 *  It is very important to pass in the correct base URL.  A common error is to pass in a URL like
 *  "http://example.org/about" which would normally be redirected to "http://example.org/about/" (w/ trailing slash).
 *  Although both URLs appear to represent the same page, they are functionally different.
 *  The version with the trailing slash is correct: if you use the version without a trailing slash, then all
 *  relative URLs will resolve incorrectly.
 *
 *  \note  If the requested URL form is CLEAN_URLS or CANONIZED_URLS and an extracted URL cannot be converted to this
 *         format, the URL will be ignored.
 */
void ExtractURLs(const std::string &document_source, std::string default_base_url,
                 const ExtractedUrlForm extracted_url_form,
                 std::vector<UrlAndAnchorTexts> * const urls_and_anchor_texts,
                 const unsigned flags = DEFAULT_EXTRACT_URL_FLAGS, unsigned long * const overall_timeout = nullptr);


} // namespace WebUtil


#endif // define WEB_UTIL_H
