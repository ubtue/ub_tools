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
#include <string>
#include "FileUtil.h"
#include "TimeLimit.h"
#include "Url.h"


// Forward declaration:
class HttpHeader;


using StringMap = std::map<std::string, std::string>;


namespace WebUtil {


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


} // namespace WebUtil


#endif // define WEB_UTIL_H
