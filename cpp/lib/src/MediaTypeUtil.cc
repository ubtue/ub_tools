/** \file    MediaTypeUtil.cc
 *  \brief   Implementation of Media Type utility functions.
 *  \author  Dr. Gordon W. Paynter
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2004-2008 Project iVia.
 *  Copyright 2004-2008 The Regents of The University of California.
 *  Copyright 2016 Universitätsbibliothek Tübingen.
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

#include "MediaTypeUtil.h"
#include <stdexcept>
#include <cctype>
#include <alloca.h>
#include <magic.h>
#include "File.h"
#include "HttpHeader.h"
#include "PerlCompatRegExp.h"
#include "StringUtil.h"
#include "Url.h"
#include "WebUtil.h"


namespace MediaTypeUtil {


std::string GetHtmlMediaType(const std::string &document) {
    static const PerlCompatRegExp doctype_regexp("^\\s*<(?:!DOCTYPE\\s+HTML\\s+PUBLIC\\s+\"-//W3C//DTD\\s+){0,1}(X?HTML)",
                                                 PerlCompatRegExp::OPTIMIZE_FOR_MULTIPLE_USE, PCRE_CASELESS);

    // If we have a match we have either HTML or XHTML...
    std::string matched_substring;
    if (doctype_regexp.match(document) and doctype_regexp.getMatchedSubstring(1, &matched_substring))
        return matched_substring.length() == 4 ? "text/html" : "text/xhtml";

    // ...otherwise we have no idea what we have:
    return "";
}


static std::string LZ4_MAGIC("\000\042\115\030");


// GetMediaType -- Get the media type of a document.
//
std::string GetMediaType(const std::string &document, const bool auto_simplify) {
    if (document.empty())
        return "";

    // 1. See if we have (X)HTML:
    std::string media_type(GetHtmlMediaType(document));
    if (not media_type.empty())
        return media_type;

    // 2. Check for LZ4 compression:
    if (document.substr(0, 4) == LZ4_MAGIC)
        return "application/lz4";

    // 3. Next try libmagic:
    const magic_t cookie = ::magic_open(MAGIC_MIME);
    if (unlikely(cookie == nullptr))
        throw std::runtime_error("in MediaTypeUtil::GetMediaType: could not open libmagic!");

    // Load the default "magic" definitions file:
    if (unlikely(::magic_load(cookie, nullptr /* use default magic file */) != 0)) {
        ::magic_close(cookie);
        throw std::runtime_error("in MediaTypeUtil::GetMediaType: could not load libmagic ("
                                 + std::string(::magic_error(cookie)) + ").");
    }

    // Use magic to get the mime type of the buffer:
    const char *magic_mime_type = ::magic_buffer(cookie, document.c_str(), document.length());
    if (unlikely(magic_mime_type == nullptr)) {
        ::magic_close(cookie);
        throw std::runtime_error("in MediaTypeUtil::GetMediaType: error in libmagic ("
                                 + std::string(::magic_error(cookie)) + ").");
    }

    // Attempt to remove possible leading junk (no idea why libmagic behaves in this manner every now and then):
    if (std::strncmp(magic_mime_type, "\\012- ", 6) == 0)
        magic_mime_type += 6;

    // Close the library:
    media_type = magic_mime_type;
    ::magic_close(cookie);

    // 4. If the libmagic could not determine the document's MIME type, test for XML:
    if (media_type.empty() and document.size() > 5)
        return std::strncmp(document.c_str(), "<?xml", 5) == 0 ? "text/xml" : "";

    if (auto_simplify)
        SimplifyMediaType(&media_type);

    return media_type;
}


std::string GetFileMediaType(const std::string &filename, const bool auto_simplify) {
    const magic_t cookie = ::magic_open(MAGIC_MIME | MAGIC_SYMLINK);
    if (unlikely(cookie == nullptr))
        throw std::runtime_error("in MediaTypeUtil::GetMediaType: could not open libmagic!");

    // Load the default "magic" definitions file:
    if (unlikely(::magic_load(cookie, nullptr /* use default magic file */) != 0)) {
        ::magic_close(cookie);
        throw std::runtime_error("in MediaTypeUtil::GetMediaType: could not load libmagic ("
                                 + std::string(::magic_error(cookie)) + ").");
    }

    // Use magic to get the mime type of the buffer:
    const char *magic_mime_type(::magic_file(cookie, filename.c_str()));
    if (unlikely(magic_mime_type == nullptr)) {
        ::magic_close(cookie);
        throw std::runtime_error("in MediaTypeUtil::GetFileMediaType: error in libmagic ("
                                 + std::string(::magic_error(cookie)) + ").");
    }

    // Attempt to remove possible leading junk (no idea why libmagic behaves in this manner every now and then):
    if (std::strncmp(magic_mime_type, "\\012- ", 6) == 0)
        magic_mime_type += 6;

    // Close the library:
    std::string media_type(magic_mime_type);
    ::magic_close(cookie);

    if (auto_simplify)
        SimplifyMediaType(&media_type);

    if (StringUtil::StartsWith(media_type, "application/octet-stream")) {
        File input(filename, "rb");
        char *buf = reinterpret_cast<char *>(::alloca(LZ4_MAGIC.size()));
        if ((input.read(buf, sizeof(buf)) == sizeof(buf)) and std::strncmp(LZ4_MAGIC.c_str(), buf, LZ4_MAGIC.size()) == 0)
            return "application/lz4";
    }

    return media_type;
}


// GetMediaType -- get the media type of a Web page.
//
std::string GetMediaType(const std::string &page_header, const std::string &page_body, const bool auto_simplify) {
    // First, attempt to find the media type in the header:
    HttpHeader http_header(page_header);
    if (http_header.isValid()) {
        std::string media_type(http_header.getMediaType());
        if (not media_type.empty()) {
            StringUtil::ToLower(&media_type);
            if (auto_simplify)
                SimplifyMediaType(&media_type);
            return media_type;
        }
    }

    // Otherwise, check the content:
    return GetMediaType(page_body, auto_simplify);
}


// GetMediaType -- Get the MediaType of the page.  Returns true if "media_type" is known and set.
//
bool GetMediaType(const Url &url, const HttpHeader &http_header, const std::string &page_content,
                  std::string * const media_type, const bool auto_simplify)
{
    // First, attempt to find the media type in the header:
    if (http_header.isValid()) {
        *media_type = http_header.getMediaType();
        if (not media_type->empty()) {
            if (auto_simplify)
                SimplifyMediaType(media_type);
            return true;
        }
    }

    // Second, attempt to use the "magic" library (libmagic) to analyse the page:
    *media_type = MediaTypeUtil::GetMediaType(page_content);
    if (not media_type->empty()) {
        if (auto_simplify)
            SimplifyMediaType(media_type);
        return true;
    }

    // Third, guess based on URL:
    *media_type = WebUtil::GuessMediaType(url);
    if (not media_type->empty() and auto_simplify)
        SimplifyMediaType(media_type);
    return not media_type->empty();
}


std::string GetMediaType(const HttpHeader &http_header, const std::string &page_body, const bool auto_simplify) {
    std::string media_type;

    // First, attempt to find the media type in the header:
    if (http_header.isValid()) {
        media_type = http_header.getMediaType();
        if (not media_type.empty()) {
            StringUtil::ToLower(&media_type);
            if (auto_simplify)
                SimplifyMediaType(&media_type);
            return media_type;
        }
    }

    // Second, attempt to use the "magic" library (libmagic) to analyse the page:
    media_type = MediaTypeUtil::GetMediaType(page_body, auto_simplify);

    return media_type;
}


bool GetMediaType(const std::string &url, const HttpHeader &http_header, const std::string &page_content,
                  std::string * const media_type, const bool auto_simplify)
{
    return GetMediaType(Url(url), http_header, page_content, media_type, auto_simplify);
}


bool SimplifyMediaType(std::string * const media_type) {
    if (media_type->empty())
        return false;

    // This is the format of the Content-Type field for Web pages:
    //  media-type     = type "/" subtype *( ";" parameter )
    //  type           = token
    //      subtype        = token
    // See: http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.7

    const std::string initial_media_type(*media_type);

    // Search for a semicolon and delete any 'parameter' parts:
    const std::string::size_type semicolon_pos(media_type->find(';'));
    if (semicolon_pos != std::string::npos)
        media_type->resize(semicolon_pos);
    else { // Try a space instead of a semicolon.
        const std::string::size_type space_pos(media_type->find(' '));
        if (space_pos != std::string::npos)
            media_type->resize(space_pos);
    }

    StringUtil::TrimWhite(media_type);

    // Return if "media_type" has changed:
    return initial_media_type != *media_type;
}


} // namespace MediaTypeUtil
