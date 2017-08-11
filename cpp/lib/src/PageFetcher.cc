/** \file    PageFetcher.cc
 *  \brief   Implementation of class PageFetcher.
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

#include <PageFetcher.h>
#include <memory>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include "FileDescriptor.h"
#include "GzStream.h"
#include "HttpHeader.h"
#include "MediaTypeUtil.h"
#include "RobotsDotTxt.h"
#include "SocketUtil.h"
#include "SslConnection.h"
#include "StringUtil.h"
#include "Url.h"


const std::string PageFetcher::TIMEOUT_ERROR_MESSAGE("timed out");
const std::string PageFetcher::NO_SUCH_DOMAIN_ERROR_MESSAGE("no such domain");
const std::string PageFetcher::ROBOTS_DOT_TXT_ERROR_MESSAGE("original URL or redirected URL blocked by robots.txt");


PageFetcher::PageFetcher(const std::string &url, const std::string &additional_http_headers,
                         const TimeLimit &time_limit, const unsigned max_redirects, const bool ignore_redirect_errors,
                         const bool transparently_unzip_content, const std::string &user_agent,
			 const std::string &acceptable_languages, const RobotsDotTxtOption robots_dot_txt_option)
	: last_error_code_(0), transparently_unzip_content_(transparently_unzip_content), user_agent_(user_agent),
	  acceptable_languages_(acceptable_languages), robots_dot_txt_option_(robots_dot_txt_option)
{
	fetchPage(url, "", 0, time_limit, max_redirects, ignore_redirect_errors, additional_http_headers,
		  robots_dot_txt_option_ == CONSULT_ROBOTS_DOT_TXT);
}


PageFetcher::PageFetcher(const std::string &url, const std::string &additional_http_headers,
                         const std::string &proxy_host, const unsigned short proxy_port, const TimeLimit &time_limit,
                         const unsigned max_redirects, const bool ignore_redirect_errors,
			 const bool transparently_unzip_content, const std::string &user_agent,
                         const std::string &acceptable_languages, const RobotsDotTxtOption robots_dot_txt_option)
	: last_error_code_(0), transparently_unzip_content_(transparently_unzip_content), user_agent_(user_agent),
	  acceptable_languages_(acceptable_languages), robots_dot_txt_option_(robots_dot_txt_option)
{
    fetchPage(url, proxy_host, proxy_port, time_limit, max_redirects, ignore_redirect_errors, additional_http_headers,
              robots_dot_txt_option_ == CONSULT_ROBOTS_DOT_TXT);
}


void PageFetcher::fetchPage(const std::string &url, const std::string &proxy_host, const unsigned short proxy_port,
                            const TimeLimit &time_limit, const unsigned max_redirects,
                            const bool ignore_redirect_errors, const std::string &additional_http_headers,
			    const bool consult_robots_dot_txt)
{
    unsigned redirect_count(0);
    Url old_url, new_url(url);

    do {
        redirect_urls_.push_back(new_url);
        ++redirect_count;

        if (consult_robots_dot_txt and deniedByRobotsDotTxt(new_url, proxy_host, proxy_port, time_limit)) {
            error_message_   = ROBOTS_DOT_TXT_ERROR_MESSAGE;
            last_error_code_ = UINT_MAX;
            return;
        }
        error_message_.clear();

        std::string new_data;
        if (not getPage(new_url, old_url, proxy_host, proxy_port, time_limit, additional_http_headers, &new_data))
            return;

        std::string raw_header, body;
        if (not PageFetcher::SplitHttpHeadersFromBody(new_data, &raw_header, &body)) {
            error_message_   = "can't extract header for URL \"" + url + "\"!";
            last_error_code_ = UINT_MAX;
            return;
        }
        HttpHeader http_header(raw_header);
        if (not http_header.isValid()) {
            error_message_   = "can't parse header for URL \"" + url + "\"!";
            last_error_code_ = UINT_MAX;
            return;
        }
        last_error_code_ = http_header.getStatusCode();

        if (last_error_code_ > 399) {
            error_message_ = "bad HTTP status code " + StringUtil::ToString(last_error_code_) + "!";
            return;
        }

        if (last_error_code_ > 299) { // We must have an HTTP redirect.
            if (http_header.getLocation().empty() and http_header.getUri().empty()) {
                error_message_ = "can't extract redirection URI from HTTP header for URL \"" + old_url.toString()
                                 + "\"!";
                return;
            }

            old_url = new_url;
            new_url = Url(http_header.getLocation().empty() ? http_header.getUri() : http_header.getLocation(),
                          old_url);
            data_ += new_data;
        } else { // last_error_code_ < 300.
            if (http_header.getContentEncoding() == "gzip" or
                (transparently_unzip_content_ and http_header.getContentType() == "application/x-gzip"))
            {
                std::string ungzipped_body(GzStream::DecompressString(body, GzStream::GUNZIP));
                std::swap(body, ungzipped_body);
                http_header.setContentLength(body.size());
                if (http_header.getContentEncoding() == "gzip")
                    http_header.setContentEncoding("");
                else
                    http_header.setContentType(MediaTypeUtil::GetMediaType(body));
                data_ = http_header.toString() + "\r\n" + body;
            } else
                data_ = new_data;

            // Deal with HTML meta-tag redirects:
            std::string redirect_url;
            if (getHttpEquivRedirect(new_url, http_header, body, &redirect_url)) {
                old_url = new_url;
                new_url = Url(redirect_url, old_url);
                last_error_code_ = 306; // Need a 3xx code for the bottom of the do-loop.
            } else
                return;
        }

        ++redirect_count;
    } while (last_error_code_ >= 300 and last_error_code_ <= 399 and redirect_count <= max_redirects);

    if (redirect_count > max_redirects and not ignore_redirect_errors)
        error_message_ = "too many redirects (> " + StringUtil::ToString(max_redirects) + ")!";
}


const std::string &PageFetcher::getData() const throw(std::exception) {
    if (not error_message_.empty())
        throw std::runtime_error("in PageFetcher::getData: trying to retrieve data after an error occurred!");

    return data_;
}


bool PageFetcher::getPage(const std::string &_url, const std::string &referrer, const std::string &proxy_host,
                          const unsigned short proxy_port, const TimeLimit &time_limit,
                          const std::string &additional_http_headers, std::string * const data)
{
    Url url(_url);
    if (not url.isValid() and not url.makeValid()) {
        error_message_ = "\"" + _url + "\" is not a valid URL and can't be made valid!";
        return false;
    }
    const std::string scheme(url.getScheme());
    if (scheme != "http" and scheme != "https") {
        error_message_ = "\"" + _url + "\" is not a Web URL!";
        return false;
    }

    errno = 0;
    FileDescriptor socket_fd;
    std::string tcp_connect_error_message;
    if (not proxy_host.empty())
        socket_fd = SocketUtil::TcpConnect(proxy_host, proxy_port, time_limit, &tcp_connect_error_message,
                                           SocketUtil::DISABLE_NAGLE, SocketUtil::REUSE_ADDR);
    else
        socket_fd = SocketUtil::TcpConnect(url.getAuthority(), url.getPort(), time_limit, &tcp_connect_error_message,
                                           SocketUtil::DISABLE_NAGLE, SocketUtil::REUSE_ADDR);
    if (socket_fd == -1) {
        if (errno == ETIMEDOUT)
            error_message_ = TIMEOUT_ERROR_MESSAGE;
        else if (errno == ENXIO)
            error_message_ = NO_SUCH_DOMAIN_ERROR_MESSAGE;
        else
            error_message_ = "could not open TCP connection: " + tcp_connect_error_message;
        return false;
    }

    std::unique_ptr<SslConnection> ssl_connection;
    if (scheme == "https")
        ssl_connection.reset(new SslConnection(socket_fd, SslConnection::ALL_STREAM_METHODS, SslConnection::CLIENT,
                                               SslConnection::SUPPORT_MULTITHREADING));

    std::string request_uri(url.getPath().empty() ? "/" : url.getPath());
    if (not url.getParams().empty())
        request_uri += ';' + url.getParams();
    if (not url.getQuery().empty())
        request_uri += '?' + url.getQuery();

    std::string request = "GET " + request_uri + " HTTP/1.0\r\n"
                          "User-Agent: " + user_agent_ + "\r\n"
                          "Host: " + url.getAuthority() + "\r\n"
                          "Connection: close\r\n"
                          "Accept-encoding: gzip\r\n"
                          "Accept: */*\r\n";
    if (not acceptable_languages_.empty())
        request += "Accept-Language: " + acceptable_languages_ + "\r\n";
    if (not referrer.empty())
        request += "Referer: " + referrer + "\r\n";
    request += additional_http_headers;
    request += "\r\n\r\n";

    if (SocketUtil::TimedWrite(socket_fd, time_limit, request.c_str(), request.length(), ssl_connection.get()) == -1)
    {
        error_message_ = time_limit.limitExceeded() ? TIMEOUT_ERROR_MESSAGE : "write(2) failed!";
        return false;
    }
    if (not SocketUtil::TimedRead(socket_fd, time_limit, data, ssl_connection.get())) {
        error_message_ = time_limit.limitExceeded() ? TIMEOUT_ERROR_MESSAGE : "read(2) failed ("
                         + std::string(std::strerror(errno)) + ")!";
        return false;
    }

    return true;
}


bool PageFetcher::deniedByRobotsDotTxt(const Url &url, const std::string &proxy_host, const unsigned short proxy_port,
                                       const TimeLimit &time_limit)
{
    // Don't allow any invalid URL:
    if (not url.isValid())
        return true;

    // If the protocol is not HTTP or HTTPS we won't check robots.txt:
    if (not url.isValidWebUrl())
        return false;

    // robots.txt URLs are always allowed otherwise we couldn't retrieve the robots.txt file itself:
    const std::string robots_dot_txt_url(url.getRobotsDotTxtUrl());
    if (robots_dot_txt_url.empty() or ::strcasecmp(robots_dot_txt_url.c_str(), url.c_str()) == 0)
        return false;

    // Check to see if we already have a robots.txt object for the current URL:
    const std::string hostname(url.getAuthority());
    RobotsDotTxtCache &robots_dot_txt_cache(RobotsDotTxtCache::GetInstance());
    if (robots_dot_txt_cache.hasHostname(hostname))
        return not robots_dot_txt_cache.getRobotsDotTxt(hostname)->accessAllowed(user_agent_, url);

    fetchPage(robots_dot_txt_url, proxy_host, proxy_port, time_limit, /* max_redirects = */ 1,
              /* ignore_redirect_errors = */ false, /* additional_http_headers = */ "",
              /* consult_robots_dot_txt = */ false);
    if (not anErrorOccurred()) {
        std::string header, body;
        SplitHttpHeadersFromBody(data_, &header, &body);
        robots_dot_txt_cache.insert(hostname, body);
        return not robots_dot_txt_cache.getRobotsDotTxt(hostname)->accessAllowed(user_agent_, url);
    } else {
        robots_dot_txt_cache.insert(hostname, "");
        return false;
    }
}


bool PageFetcher::SplitHttpHeadersFromBody(const std::string &header_and_body, std::string * const all_headers,
                                           std::string * const message_body)
{
    all_headers->clear();
    if (message_body != NULL)
        message_body->clear();
    if (header_and_body.empty())
        return false;

    // Some broken web servers use '\n' to terminate lines instead of '\r\n'.
    std::string::size_type pos;
    pos = header_and_body.find("\r\n\r\n");
    const std::string header_line_terminator(pos == std::string::npos ? "\n" : "\r\n");

    // We're going to use an FSM to parse the headers.  The State type represents how far through the headers we are,
    // and the LineType represents what sort of line we're expecting to read next.  We start in the START state, and
    // either end in the END state (on success) or the ERROR state (on failure).
    enum State { START, STATUS_READ, HEADER_READ, CONTINUATION_READ, SEPARATOR_READ, END, ERROR };

    // According to the HTTP spec the headers lines can either be regular HEADER lines, CONTINUATIONs of those lines,
    // or empty SEPARATOR lines.  Since PageFetcher doesn't remove HTTP_STATUS lines at the start of each header
    // block we also have to deal with those.  Any lines other than these are NON_HEADER lines and we generally assume
    // they are part of the message body.
    enum LineType { HTTP_STATUS, HEADER, CONTINUATION, SEPARATOR, NON_HEADER, UNKNOWN };

    std::string::size_type start_of_line = 0;
    std::string::size_type end_of_line   = 0;
    std::string::size_type colon_pos     = 0;
    std::string::size_type header_end    = 0;
    const std::string::size_type length  = header_and_body.size();

    // Parse the headers
    State state = START;
    LineType last_line_type = UNKNOWN, next_line_type;

    while (state != END and state != ERROR) {
        // Get the type of the next line
        next_line_type = UNKNOWN;
        end_of_line = header_and_body.find(header_line_terminator, start_of_line);

        // If we could not find a valid terminator, then this isn't a header line.
        if (end_of_line == std::string::npos)
            next_line_type = NON_HEADER;
        // A separator line is empty (nothing separates the start and the end).
        else if (end_of_line == start_of_line)
            next_line_type = SEPARATOR;
        // A continuation line follows a Header line, and starts with SP or TAB.
        else if (header_and_body[start_of_line] == ' ' or header_and_body[start_of_line] == '\t') {
            if (last_line_type == HEADER or last_line_type == CONTINUATION)
                next_line_type = CONTINUATION;
            else
                next_line_type = NON_HEADER;
        }
        // A header line must contain a colon
        else {
            colon_pos = header_and_body.find(":", start_of_line);
            if (colon_pos != std::string::npos and colon_pos < end_of_line)
                // There is a colon on this line, so it is a header
                next_line_type = HEADER;
            else if (header_and_body.find("HTTP/", start_of_line) == start_of_line)
                // PageFetcher has put a message like "HTTP/1.1 200 OK" on this line
                next_line_type = HTTP_STATUS;
            else
                next_line_type = NON_HEADER;
        }

        // Change the "state" of our parser.
        switch (state) {
        case START:
            if (next_line_type == HTTP_STATUS)
                state = STATUS_READ;
            else
                state = ERROR;
            break;
        case STATUS_READ:
            if (next_line_type == HEADER)
                state = HEADER_READ;
            else
                state = ERROR;
            header_end = 0;
            break;
        case HEADER_READ:
        case CONTINUATION_READ:
            if (next_line_type == HEADER)
                state = HEADER_READ;
            else if (next_line_type == CONTINUATION)
                state = CONTINUATION_READ;
            else if (next_line_type == SEPARATOR) {
                if (header_end == 0)
                    header_end = start_of_line;
                state = SEPARATOR_READ;
            }
            else
                state = ERROR;
            break;
        case SEPARATOR_READ:
            if (next_line_type == HTTP_STATUS)
                state = STATUS_READ;
            else
                state = END;
            break;
        case END:
            throw std::runtime_error("in PageFetcher::SplitHttpHeadersFromBody: illegal state: END!");
        case ERROR:
            throw std::runtime_error("in PageFetcher::SplitHttpHeadersFromBody: illegal state: ERROR!");
        }

        // Go on to the next line
        if (state != END and state != ERROR) {
            last_line_type = next_line_type;
            start_of_line = end_of_line + header_line_terminator.size();
            if (start_of_line >= length)
                start_of_line = length - 1;
        }
    }

    // Split the document_source
    if (state == END) {
        // If we got to the end of the headers, then split the document
        *all_headers = header_and_body.substr(0, header_end) + header_line_terminator;
        // Make sure the headers are terminated with header_line_terminator
        if (message_body != NULL)
            *message_body = header_and_body.substr(start_of_line);
        return true;
    } else if (state == ERROR) {
        // If there was an error parsing the headers, then... what?
        if (message_body != NULL)
            *message_body = header_and_body;
        return false;
    } else
        throw std::runtime_error("in PageFetcher::SplitHttpHeadersFromBody: illegal state parsing headers: "
                                 + StringUtil::ToString(state) + "!");
}


bool PageFetcher::getHttpEquivRedirect(const Url &current_url, const HttpHeader &current_header,
                                       const std::string &current_body, std::string * const redirect_url) const
{
    redirect_url->clear();

    if (not current_url.isValidWebUrl())
        return false;

    // Only look for redirects in Web pages:
    const std::string media_type(MediaTypeUtil::GetMediaType(current_header, current_body));
    if (media_type != "text/html" and media_type != "text/xhtml")
        return false;

    // Look for HTTP-EQUIV "Refresh" meta tags:
    std::list< std::pair<std::string, std::string> > refresh_meta_tags;
    HttpEquivExtractor http_equiv_extractor(current_body, "refresh", &refresh_meta_tags);
    http_equiv_extractor.parse();
    if (refresh_meta_tags.empty())
        return false;

    std::string delay, url_and_possible_junk;
    if (not StringUtil::SplitOnStringThenTrimWhite(refresh_meta_tags.front().second, ";", &delay,
                                                   &url_and_possible_junk))
        return false;

    const char * const url_and_equal_sign(::strcasestr(url_and_possible_junk.c_str(), "url="));
    if (url_and_equal_sign != NULL)
        *redirect_url = url_and_equal_sign + 4;
    else
        *redirect_url = url_and_possible_junk;
    StringUtil::Trim(redirect_url);

    return not redirect_url->empty();
}
