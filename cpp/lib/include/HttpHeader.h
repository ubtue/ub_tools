/** \file    HttpHeader.h
 *  \brief   Definition of class HttpHeader.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Artur Kedzierski
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
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

#ifndef HTTP_HEADER_H
#define HTTP_HEADER_H


#include <fstream>
#include <string>
#include <stdexcept>
#include <ctime>
#include "TimeUtil.h"


/** \class  HttpHeader
 *  \brief  Holds and allows access to the information in a HTTP header.
 */
class HttpHeader {
    std::string server_response_;
    unsigned status_code_;
    time_t date_, last_modified_, expires_;
    size_t content_length_;
    std::string content_type_, content_encoding_, location_, etag_, cache_control_, pragma_, server_, accept_ranges_, vary_, connection_,
        content_languages_, uri_, status_line_;
    bool is_valid_;
    std::vector<std::string> cookies_;
public:
    HttpHeader()
        : status_code_(0), date_(TimeUtil::BAD_TIME_T), last_modified_(TimeUtil::BAD_TIME_T), expires_(TimeUtil::BAD_TIME_T),
          content_length_(0), is_valid_(false) { }
    explicit HttpHeader(const std::string &header) throw(std::exception);

    bool isValid() const { return is_valid_; }

    /** This will *not* return the original header but instead an anemic approximation.  It will throw an exception
        if isValid() returns false! */
    std::string toString() const;

    bool isRedirect() const { return status_code_ == 302 and not location_.empty(); }
    unsigned getStatusCode() const { return status_code_; }
    std::string getStatusLine() const { return status_line_; }

    time_t getDate() const { return date_; }
    time_t getLastModified() const { return last_modified_; }

    std::string getContentLanguages() const { return content_languages_; }
    void setContentLanguages(const std::string &new_content_languages) { content_languages_ = new_content_languages; }

    size_t getContentLength() const { return content_length_; }
    void setContentLength(const size_t new_content_length) { content_length_ = new_content_length; }

    std::string getContentType() const { return content_type_; }
    void setContentType(const std::string &new_content_type) { content_type_ = new_content_type; }

    /** Returns the trimmed and lowercase-converted Content-encoding. */
    std::string getContentEncoding() const { return content_encoding_; }

    /** Sets the Content-encoding to the trimmed and lowercase-converted value of "new_content_encoding." */
    void setContentEncoding(const std::string &new_content_encoding);

    std::string getLocation() const { return location_; }
    std::string getETag() const { return etag_; }
    std::string getCacheControl() const { return cache_control_; }
    std::string getPragma() const { return pragma_; }
    time_t getExpires() const { return expires_; }
    std::string getServer() const { return server_; }
    std::string getAcceptRanges() const { return accept_ranges_; }
    std::string getVary() const { return vary_; }
    std::string getConnection() const { return connection_; }
    std::string getUri() const { return uri_; }

    bool dateIsValid() const { return date_ != TimeUtil::BAD_TIME_T; }
    bool lastModifiedIsValid() const { return last_modified_ != TimeUtil::BAD_TIME_T; }
    bool expiresIsValid() const { return expires_ != TimeUtil::BAD_TIME_T; }

    /** \brief   Get the media type (a.k.a. mime type) of the body from the Content-Type header.
     *  \return  The media type, or an empty string if none can be determined. */
    std::string getMediaType() const;

    /** \brief   Get the charset of the associated body from the Content-Type header.
     *  \return  The charset, or an empty string if none can be determined. */
    std::string getCharset() const { return GetCharsetFromContentType(content_type_); }

    const std::vector<std::string> &getCookies() const { return cookies_; }

    /** \brief  Tests whether the header contains an acceptable language.
     *  \param  acceptable_languages  Comma-separated list of language codes.  May be empty.  If empty this function will always return true!
     *  \note   If an acceptable language code contains a hyphen an exact match is required, e.g. "en-US" matches "en-US" but not "en-GB."
     *          If no hyphen is included, the matching requirement is relaxed.  E.g. "en" matches "en" as well as "en-GB" etc.  "*" can be used as
     *          a universal language code.  Also all matching is case insensitive and "en-us" is therefore considered to be the same as "en-US."
     *  \return Returns true if the Content-Language header was empty or contained at least one of the languages in "acceptable_languages",
     *          matched one of the Content-Language languages otherwise returns false.
     */
    bool hasAcceptableLanguage(const std::string &acceptable_languages) const;

    /** Guess whether the associated content is in English or not. */
    bool isProbablyEnglish() const { return not IsProbablyNotEnglish(getCharset(), getContentLanguages()); }

    /** Guess whether or not the associated content is in the English language. */
    static bool IsProbablyNotEnglish(const std::string &charset, const std::string &content_languages);

    /** Strips off the primary subtag from "language_tag".  E.g. given "en-GB" we will return "en".  This
        function also canonises certain strings. */
    static std::string GetLanguagePrimarySubtag(const std::string &language_tag);

    static std::string GetCharsetFromContentType(const std::string &content_type);
};


std::ostream &operator<<(std::ostream &output, const HttpHeader &http_header);


#endif // ifndef HTTP_HEADER_H
