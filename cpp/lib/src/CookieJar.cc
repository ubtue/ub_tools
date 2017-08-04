/** \file    CookieJar.cc
 *  \brief   Implementation of class CookieJar.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  \copyright 2007 Project iVia.
 *  \copyright 2007 The Regents of The University of California.
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

#include "CookieJar.h"
#include <algorithm>
#include <stdexcept>
#include "Compiler.h"
#include "DnsUtil.h"
#include "HttpHeader.h"
#include "PerlCompatRegExp.h"
#include "StringUtil.h"
#include "WebUtil.h"


std::string CookieJar::Cookie::getCookieHeader() const {
    std::string header("Cookie: ");
    header += name_;
    if (not value_.empty())
        header += "=" + value_;
    if (not version_.empty())
        header += "; $Version=" + version_;
    if (not path_.empty())
        header += "; $Path=" + path_;
    if (not domain_.empty())
        header += "; $Domain=" + domain_;
    if (not cookies_supported_.empty())
        header += "CookiesSupported=" + cookies_supported_ + "\r\n";
    header += "\r\n";

    return header;
}


std::string CookieJar::Cookie::toString() const {
    std::string cookie_as_string;
    if (name_.empty())
        cookie_as_string = "*Empty cookie!*\n";
    else {
        cookie_as_string = name_ + "=" + value_ + "\n";
        if (not comment_.empty())
            cookie_as_string += "\tComment: " + comment_ + "\n";
        if (not comment_url_.empty())
            cookie_as_string += "\tCommentURL: " + comment_url_ + "\n";
        if (not version_.empty())
            cookie_as_string += "\tVersion: " + version_ + "\n";
        if (not domain_.empty())
            cookie_as_string += "\tDomain: " + domain_ + "\n";
        if (not port_.empty())
            cookie_as_string += "\tPort: " + port_ + "\n";
        if (not path_.empty())
            cookie_as_string += "\tPath: " + path_ + "\n";
        if (not cookies_supported_.empty())
            cookie_as_string += "\tCookiesSupported: " + cookies_supported_ + "\n";
        if (secure_)
            cookie_as_string += "\tSecure\n";
        if (discard_)
            cookie_as_string += "\tDiscard\n";
        if (http_only_)
            cookie_as_string += "\tHttpOnly\n";
        cookie_as_string += "\tExpires: " + TimeUtil::TimeTToString(expiration_time_, TimeUtil::ISO_8601_FORMAT,
                                                                    TimeUtil::UTC) + "\n";
    }

    return cookie_as_string;
}


namespace {


inline bool DomainContainsNoEmbeddedDots(const std::string &domain) {
    return std::strchr(domain.c_str() + 1, '.') == nullptr;
}


inline bool RequestHostHasMoreDotsThanDomain(const std::string &domain, const std::string &request_host) {
    std::string request_host_part(StringUtil::ReplaceString(domain, "", request_host, /*global=*/false));
    return std::strchr(request_host_part.c_str(), '.') != nullptr;
}


} // unnamed namespace


bool CookieJar::Cookie::setDomain(const std::string &domain, const std::string &request_host) {
    if (domain.empty())
        return true;

    if ((domain[0] != '.') or DomainContainsNoEmbeddedDots(domain)
        or RequestHostHasMoreDotsThanDomain(domain, request_host))
        return false;

    domain_ = domain;
    return true;
}


namespace {


class MatchCookieAttributeName: public std::unary_function<std::string, bool> {
    std::string attribute_name_;
public:
    explicit MatchCookieAttributeName(const std::string &attribute_name): attribute_name_(attribute_name) { }
    bool operator()(const std::string &attribute_name_and_value) const;
};


bool MatchCookieAttributeName::operator()(const std::string &attribute_name_and_value) const {
    if (::strncasecmp(attribute_name_.c_str(), attribute_name_and_value.c_str(), attribute_name_.length()) != 0)
        return false;

    // Skip over any optional linear whitespace that may come before an equal sign:
    std::string::const_iterator ch(attribute_name_and_value.begin() + attribute_name_.length());
    while (ch != attribute_name_and_value.end() and (*ch == ' ' or *ch == '\t'))
        ++ch;

    return ch != attribute_name_and_value.end() and *ch == '=';
}


inline bool DomainMatch(const std::string &domain_pattern, const std::string &domain_name) {
    return StringUtil::IsSuffixOf(domain_pattern, "." + domain_name);
}


} // unnamed namespace


void CookieJar::addCookies(const HttpHeader &http_header, const std::string &default_domain) {
    if (unlikely(not DnsUtil::IsValidHostName(default_domain)))
        throw std::runtime_error("in CookieJar::addCookies: default domain \"" + default_domain
                                 + "\" must be a valid hostname!");

    const std::string lowercase_default_domain(StringUtil::ToLower(default_domain));

    const std::vector<std::string> raw_cookies(http_header.getCookies());
    for (std::vector<std::string>::const_iterator raw_cookie(raw_cookies.begin()); raw_cookie != raw_cookies.end();
         ++raw_cookie)
        parseCookie(*raw_cookie, lowercase_default_domain);
}


namespace {


bool PathMatch(const std::string &path_pattern, const std::string &path) {
    return StringUtil::IsPrefixOf(path_pattern, path);
}


} // unnamed namespace


void CookieJar::getCookieHeaders(const std::string &domain_name, const std::string &path,
                                 std::string * const cookie_headers) const
{
    cookie_headers->clear();
    const std::string lowercase_domain_name(StringUtil::ToLower(domain_name));
    const std::string normalised_path(path.empty() ? "/" : path);

    const time_t now(std::time(nullptr));
    std::vector<Cookie> matching_cookies;
    for (iterator key_and_cookie(cookies_.begin()); key_and_cookie != cookies_.end(); /* Empty */) {
        // Delete the cookie if it has expired:
        if (now > key_and_cookie->second.expiration_time_)
            cookies_.erase(key_and_cookie++);
        else {
            const Cookie cookie(key_and_cookie->second);
            const std::string domain((cookie.domain_.empty()) ? cookie.request_host_ : cookie.domain_);
            if (DomainMatch(domain, lowercase_domain_name) and (PathMatch(cookie.path_, normalised_path)))
                matching_cookies.push_back(cookie);


            ++key_and_cookie;
        }
    }

    std::sort(matching_cookies.begin(), matching_cookies.end(), PathCompare);

    // Now generate the "Cookie:" headers:
    for (std::vector<Cookie>::const_iterator matching_cookie(matching_cookies.begin());
         matching_cookie != matching_cookies.end(); ++matching_cookie)
        *cookie_headers += matching_cookie->getCookieHeader();
}


namespace {


bool ExtractAttribAndValue(std::string::const_iterator &ch, const std::string::const_iterator &end,
                           std::string * attrib_name, std::string * attrib_value)
{
    attrib_name->clear();
    attrib_value->clear();

    // Skip over leading linear space:
    while (ch != end and (*ch == ' ' or *ch == '\t'))
        ++ch;
    if (ch == end)
        return false;

    // Extract the attribute name:
    while (ch != end and *ch != '=')
        *attrib_name += *ch++;
    StringUtil::RightTrim(" \t", attrib_name);
    if (attrib_name->empty())
        return false;

    // Skip the equal sign:
    if (ch != end)
        ++ch;

    // Skip over linear space following the '=' sign:
    while (ch != end and (*ch == ' ' or *ch == '\t'))
        ++ch;

    // Do we have an attribute name without an attribute value?
    if (ch == end or *ch == ';')
        return true;

    const bool quoted_value(*ch == '"');
    if (quoted_value) {
        ++ch;
        while (ch != end and *ch != '"')
            *attrib_value += *ch++;
        if (unlikely(ch == end))
            return false;
        ++ch; // Skip over the terminating quote.

        // Skip over linear space following the '=' sign:
        while (ch != end and (*ch == ' ' or *ch == '\t'))
            ++ch;

        // Garbled input?
        if (unlikely(ch != end and *ch != ';'))
            return false;
    } else {
        while (ch != end and *ch != ';')
            *attrib_value += *ch++;
        StringUtil::RightTrim(" \t", attrib_value);
    }

    return true;
}


enum AttribType { COOKIE_NAME, KNOWN_ATTRIB, DOMAIN_ATTRIB, RESERVED_ATTRIB };
AttribType GetAttribType(const std::string &attrib_name) {
    if (unlikely(attrib_name.empty()))
        throw std::runtime_error("in GetAttribType (CookieJar.cc): can't determine the type of name of an empty "
                                 "string!");

    if (attrib_name[0] == '$')
        return RESERVED_ATTRIB;

    const std::string lowercase_attrib_name(StringUtil::ToLower(attrib_name));
    if (lowercase_attrib_name == "comment")
        return KNOWN_ATTRIB;
    if (lowercase_attrib_name == "domain")
        return DOMAIN_ATTRIB;
    if (lowercase_attrib_name == "max-age")
        return KNOWN_ATTRIB;
    if (lowercase_attrib_name == "path")
        return KNOWN_ATTRIB;
    if (lowercase_attrib_name == "secure")
        return KNOWN_ATTRIB;
    if (lowercase_attrib_name == "version")
        return KNOWN_ATTRIB;
    if (lowercase_attrib_name == "expires")
        return KNOWN_ATTRIB;
    if (lowercase_attrib_name == "port")
        return KNOWN_ATTRIB;
    if (lowercase_attrib_name == "discard")
        return KNOWN_ATTRIB;
    if (lowercase_attrib_name == "commenturl")
        return KNOWN_ATTRIB;
    if (lowercase_attrib_name == "httponly")
        return KNOWN_ATTRIB;
    if (lowercase_attrib_name == "cookiessupported")
        return KNOWN_ATTRIB;

    return COOKIE_NAME;
}


// PortListIsValid -- checks to see that the ports are linear whitespace separated.
//
bool PortListIsValid(const std::string &port_list_candidate) {
    std::vector<std::string> port_candidates;
    StringUtil::SplitThenTrimWhite(port_list_candidate, " \t", &port_candidates);

    for (std::vector<std::string>::const_iterator port_candidate(port_candidates.begin());
         port_candidate != port_candidates.end(); ++port_candidate)
    {
        if (unlikely(not StringUtil::IsUnsignedNumber(*port_candidate)))
            return false;
    }

    return true;
}


bool UpdateKnownAttrib(const std::string &attrib_name, const std::string &attrib_value,
                       CookieJar::Cookie * const cookie)
{
    const std::string lowercase_attrib_name(StringUtil::ToLower(attrib_name));
    if (lowercase_attrib_name == "comment") {
        if (unlikely(attrib_value.empty()))
            return false;

        cookie->comment_ = attrib_value;
        return true;
    }

    if (lowercase_attrib_name == "max-age") {
        unsigned seconds_from_now;
        if (unlikely(not StringUtil::ToUnsigned(attrib_value, &seconds_from_now) or seconds_from_now == 0))
            return false;

        cookie->expiration_time_ = std::time(nullptr) + seconds_from_now;
        return true;
    }

    if (lowercase_attrib_name == "path") {
        cookie->path_ = attrib_value;
        return true;
    }

    if (lowercase_attrib_name == "secure") {
        if (unlikely(not attrib_value.empty()))
            return false;

        cookie->secure_ = true;
        return true;
    }

    if (lowercase_attrib_name == "version") {
        cookie->version_ = attrib_value; // We intentionally don't check for compliance with the spec.
        return true;
    }

    if (lowercase_attrib_name == "expires") {
        if (unlikely(attrib_value.empty()))
            return false;

        cookie->expiration_time_ = WebUtil::ParseWebDateAndTime(attrib_value);
        return cookie->expiration_time_ != TimeUtil::BAD_TIME_T;
    }

    if (lowercase_attrib_name == "port") {
        if (unlikely(not PortListIsValid(attrib_value)))
            return false;

        cookie->port_ = attrib_value;
        return true;
    }

    if (lowercase_attrib_name == "discard") {
        if (unlikely(not attrib_value.empty()))
            return false;

        cookie->discard_ = true;
        return true;
    }

    if (lowercase_attrib_name == "httponly") {
        if (unlikely(not attrib_value.empty()))
            return false;

        cookie->http_only_ = true;
        return true;
    }

    if (lowercase_attrib_name == "commenturl") {
        if (unlikely(attrib_value.empty()))
            return false;

        cookie->comment_url_ = attrib_value;
        return true;
    }

    if (lowercase_attrib_name == "cookiessupported") {
        if (unlikely(attrib_value.empty()))
            return false;

        cookie->cookies_supported_ = attrib_value;
        return true;
    }

    throw std::runtime_error("in UpdateKnownAttrib (CookieJar.cc): unknown \"known\" attribute name \"" + attrib_name
                             + "\"!");
}


} // unnamed namespace


void CookieJar::parseCookie(const std::string &raw_cookie, const std::string &default_domain) {
    Cookie cookie;
    std::string name, value;
    std::string::const_iterator ch(raw_cookie.begin());
    while (ch != raw_cookie.end()) {
        if (not ExtractAttribAndValue(ch, raw_cookie.end(), &name, &value))
            break;

        switch (GetAttribType(name)) {
        case COOKIE_NAME:
            // Take care of the previous cookie if there was one:
            if (not cookie.empty()) {
                cookie.request_host_ = default_domain;
                addCookie(cookie);
            }
            cookie = Cookie(name, value);
            break;
        case KNOWN_ATTRIB: {
            // Must follow a cookie name:
            if (unlikely(cookie.empty()))
                return; // Garbage!

            if (not UpdateKnownAttrib(name, value, &cookie))
                return; // Garbage!
            break;
        }
        case DOMAIN_ATTRIB:
            if (not cookie.setDomain(value, default_domain))
                return; // Violated RFC2109 4.3.2
            break;
        case RESERVED_ATTRIB:
            /* Ignore! */
            break;
        default:
            throw std::runtime_error("in CookieJar::parseCookie: unknown attribute type!");
        }

        // If we have more data we need to skip over a semicolon:
        if (ch != raw_cookie.end()) {
            if (unlikely(*ch != ';'))
                throw std::runtime_error("in CookieJar::parseCookie: this condition should *never* happen! "
                                         "(Remainder of text is \""
                                         + StringUtil::CStyleEscape(std::string(ch, raw_cookie.end())) + "\".)\n"
                                         "(Full cookie text is \"" + raw_cookie + "\".)");
            ++ch;
        }
    }

    if (not cookie.empty()) {
        cookie.request_host_ = default_domain;
        addCookie(cookie);
    }
}
