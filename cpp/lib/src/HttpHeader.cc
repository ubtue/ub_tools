/** \file    HttpHeader.cc
 *  \brief   Implementation of class HttpHeader.
 *  \author  Artur Kedzierski
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Dr. Gordon W. Paynter
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
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

#include "HttpHeader.h"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include "MediaTypeUtil.h"
#include "StringUtil.h"
#include "WebUtil.h"


namespace {


// a Boolean predicate class
class StartsWith {
    std::string prefix_;
public:
    StartsWith(const std::string &prefix): prefix_(prefix) { }
    bool operator()(const std::string &s) const { return ::strncasecmp(prefix_.c_str(), s.c_str(), prefix_.length()) == 0; }
};


} // unnamed namespace


HttpHeader::HttpHeader(const std::string &header) {
    // Assume the header is not valid until we discover otherwise:
    is_valid_ = false;

    // Empty headers are not valid:
    if (header.empty())
        return;

    // Split the header into several lines:
    std::list<std::string> lines;
    StringUtil::Split(header, "\r\n", &lines);

    // Some Web servers incorrectly use '\n' instead of '\r\n'.
    // Detect (and compensate for) this behaviour:
    if (lines.size() == 1) {
        lines.clear();
        StringUtil::Split(header, "\n", &lines);
    }

    // If we couldn't split the headers, then it was probably trivially small:
    if (lines.empty())
        return;

    // Parse each of the lines:
    if ((lines.front().length() > 5 and lines.front().substr(0, 5) == "HTTP/")
        or StringUtil::Match("[A-Za-z][A-Za-z]*: *", lines.front()))
    {
        // Okay, we're happy the header is valid:
        is_valid_ = true;

        server_response_ = lines.front();

        // Read the status code from the first line (e.g. HTTP/1.0 200):
        if (lines.front().substr(0, 5) == "HTTP/")
            std::sscanf(lines.front().c_str(), "HTTP/%*u.%*u %u", &status_code_);
        else
            status_code_ = 200; // Too optimistic?

        // Read additional information on the status line
        if (lines.front().size() > 12)
            status_line_ = lines.front().substr(13);

        // Read the date:
        std::list<std::string>::const_iterator server_header;
        server_header = find_if(lines.begin(), lines.end(), StartsWith("Date:"));
        if (server_header == lines.end() or server_header->length() <= 6)
            date_ = TimeUtil::BAD_TIME_T;
        else
            date_ = WebUtil::ParseWebDateAndTime(server_header->substr(6));

        // Find  the last modification header:
        server_header = find_if(lines.begin(), lines.end(), StartsWith("Last-Modified:"));
        if (server_header == lines.end() or server_header->length() <= 15)
            last_modified_ = TimeUtil::BAD_TIME_T;
        else
            last_modified_ = WebUtil::ParseWebDateAndTime(server_header->substr(15));

        server_header = find_if(lines.begin(), lines.end(), StartsWith("Content-Length:"));
        if (server_header == lines.end() or server_header->length() <= 16)
            content_length_ = 0;
        else if (std::sscanf(server_header->substr(16).c_str(), "%zu", &content_length_) != 1)
            is_valid_ = false;

        server_header = find_if(lines.begin(), lines.end(), StartsWith("Content-Type:"));
        if (server_header == lines.end() or server_header->length() <= 14)
            content_type_ = "";
        else
            content_type_ = server_header->substr(14);

        server_header = find_if(lines.begin(), lines.end(), StartsWith("Content-Encoding:"));
        if (server_header == lines.end() or server_header->length() <= 18)
            content_encoding_ = "";
        else
            content_encoding_ = StringUtil::ToLower(StringUtil::Trim(server_header->substr(18)));

        server_header = find_if(lines.begin(), lines.end(), StartsWith("Location:"));
        if (server_header == lines.end() or server_header->length() <= 10)
            location_ = "";
        else {
            location_ = server_header->substr(10);
            if (status_code_ == 200) // Deal with overly optimistic assumption above!
                status_code_ = 300;
        }

        server_header = find_if(lines.begin(), lines.end(), StartsWith("Content-Language:"));
        if (server_header == lines.end() or server_header->length() <= 18)
            content_languages_ = "";
        else
            content_languages_ = server_header->substr(18);

        server_header = find_if(lines.begin(), lines.end(), StartsWith("URI:"));
        if (server_header == lines.end() or server_header->length() <= 4)
            uri_ = "";
        else
            uri_ = server_header->substr(4);

        server_header = find_if(lines.begin(), lines.end(), StartsWith("ETag:"));
        if (server_header == lines.end() or server_header->length() <= 6)
            etag_ = "";
        else
            etag_ = server_header->substr(6);

        server_header = find_if(lines.begin(), lines.end(), StartsWith("Cache-Control:"));
        if (server_header == lines.end() or server_header->length() <= 15)
            cache_control_ = "";
        else
            cache_control_ = server_header->substr(15);

        server_header = find_if(lines.begin(), lines.end(), StartsWith("Pragma:"));
        if (server_header == lines.end() or server_header->length() <= 8)
            pragma_ = "";
        else
            pragma_ = server_header->substr(8);

        server_header = find_if(lines.begin(), lines.end(), StartsWith("Expires:"));
        if (server_header == lines.end() or server_header->length() <= 9)
            expires_ = TimeUtil::BAD_TIME_T;
        else {
            std::string http_time(server_header->substr(9));
            if (StringUtil::Trim(&http_time) == "0")
                expires_ = 0;
            else
                expires_ = WebUtil::ParseWebDateAndTime(server_header->substr(9));
        }

        server_header = find_if(lines.begin(), lines.end(), StartsWith("Server:"));
        if (server_header == lines.end() or server_header->length() <= 8)
            server_ = "";
        else
            server_ = server_header->substr(8);

        server_header = find_if(lines.begin(), lines.end(), StartsWith("Accept-Ranges:"));
        if (server_header == lines.end() or server_header->length() <= 15)
            accept_ranges_ = "";
        else
            accept_ranges_ = server_header->substr(15);

        server_header = find_if(lines.begin(), lines.end(), StartsWith("Vary:"));
        if (server_header == lines.end() or server_header->length() <= 6)
            vary_ = "";
        else
            vary_ = server_header->substr(6);

        server_header = find_if(lines.begin(), lines.end(), StartsWith("Connection:"));
        if (server_header == lines.end() or server_header->length() <= 12)
            connection_ = "";
        else
            connection_ = server_header->substr(12);

        // Attempt to extract all "Set-Cookie:" headers:
        std::list<std::string>::const_iterator search_start(lines.begin());
        while (search_start != lines.end()) {
            server_header = std::find_if(search_start, std::list<std::string>::const_iterator(lines.end()),
                                         StartsWith("Set-Cookie:"));
            if (server_header != lines.end() and server_header->length() > 12)
                cookies_.push_back(server_header->substr(12));
            search_start = server_header;
            if (search_start != lines.end())
                ++search_start;
        }
    } else {
        // Set some default values.  Note that "is_valid_" is false.  We do something really crazy:
        status_code_    = 200;
        content_type_   = "text/html"; // Yeah, right!?
        date_           = TimeUtil::BAD_TIME_T;
        last_modified_  = TimeUtil::BAD_TIME_T;
        content_length_ = header.length();
        location_       = "";
        etag_           = "";
        cache_control_  = "";
        pragma_         = "";
        server_         = "";
    }
}


std::string HttpHeader::toString() const {
    if (unlikely(not is_valid_))
        throw std::runtime_error("in HttpHeader::toString: can't create a string representation of an invalid header!");

    std::string string_rep(server_response_ + "\r\n");
    if (not content_type_.empty())
        string_rep += "Content-Type: " + content_type_ + "\r\n";
    if (date_ != TimeUtil::BAD_TIME_T)
        string_rep += "Date: " + TimeUtil::TimeTToString(date_, TimeUtil::ZULU_FORMAT, TimeUtil::UTC) + "\r\n";
    if (not server_.empty())
        string_rep += "Server: " + server_ + "\r\n";
    if (not accept_ranges_.empty())
        string_rep += "Accept-Ranges: " + accept_ranges_ + "\r\n";
    if (not vary_.empty())
        string_rep += "Vary: " + vary_ + "\r\n";
    if (not connection_.empty())
        string_rep += "Connection: " + connection_ + "\r\n";
    if (last_modified_ != TimeUtil::BAD_TIME_T)
        string_rep += "Last-Modified: " + TimeUtil::TimeTToString(last_modified_, TimeUtil::ZULU_FORMAT, TimeUtil::UTC) + "\r\n";
    if (content_length_ != 0)
        string_rep += "Content-Length: " + StringUtil::ToString(content_length_) + "\r\n";
    if (not location_.empty())
        string_rep += "Location: " + location_ + "\r\n";
    if (not content_languages_.empty())
        string_rep += "Content-Language: " + content_languages_ + "\r\n";
    if (not uri_.empty())
        string_rep += "URI: " + uri_ + "\r\n";
    if (not etag_.empty())
        string_rep += "ETag: " + etag_ + "\r\n";
    if (not cache_control_.empty())
        string_rep += "Cache-Control: " + cache_control_ + "\r\n";
    if (not pragma_.empty())
        string_rep += "Pragma: " + pragma_ + "\r\n";
    if (expires_ != TimeUtil::BAD_TIME_T) {
        if (expires_ == 0)
            string_rep += "Expires: 0\r\n";
        else
            string_rep += "Expires: " + TimeUtil::TimeTToString(expires_, TimeUtil::ZULU_FORMAT, TimeUtil::UTC) + "\r\n";
    }
    if (not cookies_.empty())
        for (std::vector<std::string>::const_iterator cookie(cookies_.begin()); cookie != cookies_.end(); ++cookie)
            string_rep += "Cookie: " + *cookie + "\r\n";
    if (not content_type_.empty())
        string_rep += "Content-Type: " + content_type_ + "\r\n";

    return string_rep;
}


void HttpHeader::setContentEncoding(const std::string &new_content_encoding) {
    content_encoding_ = StringUtil::ToLower(StringUtil::Trim(new_content_encoding));
}


std::string HttpHeader::getMediaType() const {
    // If there's no Content-Type header, do nothing:
    if (content_type_.empty())
        return "";

    std::string simplified_media_type(content_type_);
    MediaTypeUtil::SimplifyMediaType(&simplified_media_type);
    return simplified_media_type;
}


namespace {


class LanguageMatch: public std::unary_function<const std::string &, bool> {
    std::string language_to_match_;
public:
    explicit LanguageMatch(const std::string &language_to_match);
    bool operator()(const std::string &acceptable_language) const;
};


LanguageMatch::LanguageMatch(const std::string &language_to_match): language_to_match_(language_to_match) {
    // Trim a possible trailing hyphen:
    if (not language_to_match_.empty() and language_to_match_[language_to_match_.length() - 1] == '-')
        language_to_match_.resize(language_to_match_.size() - 1);
    StringUtil::ToLower(&language_to_match_);
}


bool LanguageMatch::operator()(const std::string &acceptable_language) const {
    if (acceptable_language.empty() or acceptable_language == "*")
        return true; // Match everything.

    // Exact match?
    if (::strcasecmp(acceptable_language.c_str(), language_to_match_.c_str()) == 0)
        return true;

    // Trying to match XX-YY and ZZ-WW?
    if (acceptable_language.find('-') != std::string::npos)
        return false;

    // Final case, try to match XX against YY or ZZ-WW
    return StringUtil::IsPrefixOf(acceptable_language + "-", language_to_match_);
}


} // unnamed namespace


bool HttpHeader::hasAcceptableLanguage(const std::string &acceptable_languages) const {
    if (content_languages_.empty())
        return true;

    std::vector<std::string> acceptable_languages_set;
    StringUtil::SplitThenTrimWhite(StringUtil::ToLower(acceptable_languages), ',', &acceptable_languages_set);
    if (unlikely(acceptable_languages_set.empty()))
        return true;

    std::vector<std::string> content_languages;
    StringUtil::SplitThenTrimWhite(content_languages_, ',', &content_languages);
    if (unlikely(content_languages.empty()))
        return true;

    // Check to see whether at least one content language is one of the acceptable languages:
    for (const auto &content_language : content_languages) {
        if (std::find_if(acceptable_languages_set.begin(), acceptable_languages_set.end(), LanguageMatch(content_language))
            != acceptable_languages_set.end())
            return true;
    }

    return false;
}


bool HttpHeader::IsProbablyNotEnglish(const std::string &charset, const std::string &content_languages) {
    if (not content_languages.empty()) {
        std::list<std::string> language_tags;
        StringUtil::SplitThenTrim(content_languages, ",", " \t", &language_tags);
        for (const auto &language_tag : language_tags) {
            if (HttpHeader::GetLanguagePrimarySubtag(language_tag) == "en")
                return false;
        }

        return true;
    }

    if (charset.empty())
        return false;

    const std::string lc_charset(StringUtil::ToLower(charset));
    if (std::strcmp(lc_charset.c_str(), "us-ascii") == 0
        or std::strcmp(lc_charset.c_str(), "usascii") == 0
        or std::strcmp(lc_charset.c_str(), "iso-8859-1") == 0
        or std::strcmp(lc_charset.c_str(), "iso8859-1") == 0
        or std::strcmp(lc_charset.c_str(), "iso8859_1") == 0
        or std::strcmp(lc_charset.c_str(), "iso88591") == 0
        or std::strcmp(lc_charset.c_str(), "iso-8859-15") == 0
        or std::strcmp(lc_charset.c_str(), "iso8859-15") == 0
        or ::strncmp(lc_charset.c_str(), "windows-125", 11) == 0
        or ::strncmp(lc_charset.c_str(), "windows125", 10) == 0
        or std::strcmp(lc_charset.c_str(), "utf-8") == 0
        or std::strcmp(lc_charset.c_str(), "utf8") == 0
        or std::strcmp(lc_charset.c_str(), "latin1") == 0
        or std::strcmp(lc_charset.c_str(), "latin-1") == 0
        or std::strcmp(lc_charset.c_str(), "latin9") == 0
        or std::strcmp(lc_charset.c_str(), "latin-9") == 0
        or std::strcmp(lc_charset.c_str(), "x-mac-roman") == 0
        or std::strcmp(lc_charset.c_str(), "macintosh") == 0
        or std::strcmp(lc_charset.c_str(), "iso/iec10646-1") == 0)
        return false;

    return true;
}


std::string HttpHeader::GetLanguagePrimarySubtag(const std::string &language_tag) {
    std::string::const_iterator ch(language_tag.begin());

    // Skip over a possible leading "x-" or "i-":
    if (language_tag.length() > 2 and language_tag[1] == '-' and (language_tag[0] == 'x' or language_tag[0] == 'i'))
        ch += 2;

    std::string primary_subtag;
    for (/* Empty! */; ch != language_tag.end(); ++ch) {
        if (*ch == '-' or *ch == '_')
            break;
        primary_subtag += *ch;
    }

    StringUtil::ToLower(&primary_subtag);

    // Canonise certain strings:
    if (primary_subtag == "english" or primary_subtag == "eng")
        return "en";
    if (primary_subtag == "french")
        return "fr";
    if (primary_subtag == "german")
        return "de";
    if (primary_subtag == "dutch")
        return "nl";

    return primary_subtag;
}


std::string HttpHeader::GetCharsetFromContentType(const std::string &content_type) {
    const char *start(::strcasestr(content_type.c_str(), "charset="));
    if (start == nullptr)
        return "";

    std::string charset(start + 8);
    return StringUtil::TrimWhite(charset);
}


std::ostream &operator<<(std::ostream &output, const HttpHeader &http_header) {
    if (not http_header.isValid())
        output << "Invalid HTTP header!\n";
    else {
        output << "Status:\t\t" << http_header.getStatusCode() << '\n';

        if (http_header.dateIsValid())
            output << "Date:\t\t" << TimeUtil::TimeTToUtcString(http_header.getDate()) << '\n';

        if (http_header.lastModifiedIsValid())
            output << "LastModified:\t" << TimeUtil::TimeTToUtcString(http_header.getLastModified()) << '\n';

        const std::string content_languages(http_header.getContentLanguages());
        if (not content_languages.empty())
            output << "ContentLanguages:\t" << content_languages << '\n';

        const size_t content_length(http_header.getContentLength());
        if (content_length != 0)
            output << "ContentLength:\t" << content_length << '\n';

        const std::string content_type(http_header.getContentType());
        if (not content_type.empty())
            output << "ContentType:\t" << content_type << '\n';

        const std::string content_encoding(http_header.getContentEncoding());
        if (not content_encoding.empty())
            output << "ContentEncoding:\t" << content_encoding << '\n';

        const std::string location(http_header.getLocation());
        if (not location.empty())
            output << "Location:\t" << location << '\n';

        const std::string e_tag(http_header.getETag());
        if (not e_tag.empty())
            output << "Etag:\t\t" << e_tag << '\n';

        const std::string cache_control(http_header.getCacheControl());
        if (not cache_control.empty())
            output << "CacheControl:\t" << cache_control << '\n';

        const std::string pragma(http_header.getPragma());
        if (not pragma.empty())
            output << "Pragma:\t\t" << pragma << '\n';

        if (http_header.expiresIsValid())
            output << "Expires:\t" << TimeUtil::TimeTToUtcString(http_header.getExpires()) << '\n';

        const std::string server(http_header.getServer());
        if (not server.empty())
            output << "Server:\t\t" << server << '\n';

        const std::string uri(http_header.getUri());
        if (not uri.empty())
            output << "URI:\t\t" << uri << '\n';

        const std::string media_type(http_header.getMediaType());
        if (not media_type.empty())
            output << "MediaType:\t" << media_type << '\n';

        const std::string charset(http_header.getCharset());
        if (not charset.empty())
            output << "Charset:\t" << charset << '\n';

        const std::vector<std::string> &cookies(http_header.getCookies());
        if (not cookies.empty()) {
            output << "Cookies:\n";
            for (std::vector<std::string>::const_iterator cookie(cookies.begin()); cookie != cookies.end(); ++cookie)
                output << '\t' << *cookie << '\n';
        }

        output << '\n';
    }

    return output;
}
