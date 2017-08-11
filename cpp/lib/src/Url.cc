/** \file    Url.cc
 *  \brief   Implementation of class Url.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Jiangtao Hu
 */

/*
 *  Copyright 2003-2008 Project iVia.
 *  Copyright 2003-2008 The Regents of The University of California.
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

#include "Url.h"
#include <set>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <climits>
#include <cstddef>
#include "Compiler.h"
#include "DnsUtil.h"
#include "Downloader.h"
#include "HttpHeader.h"
#include "IniFile.h"
#include "StringUtil.h"
#include "UrlUtil.h"


//#define VERSIGN_SITE_FINDER_HACK
#define CENTRAL_NIC_HACK


std::string Url::default_user_agent_;


Url::Url(const std::string &url, const std::string &default_base_url, const unsigned creation_flags,
         const RobotsDotTxtOption robots_dot_txt_option, const unsigned timeout, const std::string &user_agent)
    : url_(url), default_base_url_(default_base_url), robots_dot_txt_option_(robots_dot_txt_option),
      timeout_(timeout), user_agent_(user_agent), state_(UNINITIALISED),
      throw_exceptions_(creation_flags & THROW_EXCEPTIONS)
{
    if (creation_flags & REMOVE_FRAGMENT and (url.length() < 5 or url.substr(0, 5) != "file:")) {
        const std::string::size_type first_hash_pos(url_.find('#'));
        if (first_hash_pos != std::string::npos)
            url_ = url_.substr(0, first_hash_pos);
    }

    if ((creation_flags & FORCE_ABSOLUTE_HTTP_URL) and not isAbsolute() and not forceAbsoluteHttp())
        return;
    if ((creation_flags & AUTO_MAKE_ABSOLUTE) and not makeAbsolute())
        return;

    if ((creation_flags & AUTO_CANONIZE) and not makeCanonical())
        return;
    else if ((creation_flags & AUTO_CLEAN_UP) and not cleanUp())
        return;

    if ((creation_flags & AUTO_MAKE_VALID) and not isValid() and not makeValid())
        error("can't make invalid URL \"" + url_ + "\" valid!");
}


Url::Url()
    : robots_dot_txt_option_(IGNORE_ROBOTS_DOT_TXT), timeout_(0), state_(UNINITIALISED), throw_exceptions_(false)
{
}


Url::Url(const std::string &url, const unsigned creation_flags, const RobotsDotTxtOption robots_dot_txt_option,
         const unsigned timeout, const std::string &user_agent)
        : url_(url), robots_dot_txt_option_(robots_dot_txt_option), timeout_(timeout), user_agent_(user_agent),
          state_(UNINITIALISED), throw_exceptions_(creation_flags & THROW_EXCEPTIONS)
{
    if ((creation_flags & FORCE_ABSOLUTE_HTTP_URL) and not isAbsolute() and not forceAbsoluteHttp())
        return;

    if (creation_flags & AUTO_MAKE_ABSOLUTE)
        throw std::runtime_error("in Url::Url: constructor flag AUTO_MAKE_ABSOLUTE used with no base URL!");

    if ((creation_flags & AUTO_CANONIZE) and not makeCanonical())
        return;
    else if ((creation_flags & AUTO_CLEAN_UP) and not cleanUp())
        return;

    if ((creation_flags & AUTO_MAKE_VALID) and not isValid() and not makeValid())
        error("can't make invalid URL \"" + url_ + "\" valid!");
}


Url::Url(const std::string &scheme, const std::string &username_password, const std::string &authority,
         const std::string &port, const std::string path, const std::string &params, const std::string query,
         const std::string &fragment, const unsigned creation_flags, const RobotsDotTxtOption robots_dot_txt_option,
         const unsigned timeout, const std::string &user_agent)
    : robots_dot_txt_option_(robots_dot_txt_option), timeout_(timeout), user_agent_(user_agent), scheme_(scheme),
      username_password_(username_password), authority_(StringUtil::ToLower(authority)), port_(port), path_(path),
    params_(params), query_(query), fragment_((creation_flags & REMOVE_FRAGMENT) ? "" : fragment),
    state_(HAS_BEEN_PARSED), throw_exceptions_(creation_flags & THROW_EXCEPTIONS)
{
    if ((creation_flags & FORCE_ABSOLUTE_HTTP_URL) and not isAbsolute() and not forceAbsoluteHttp())
        return;

    if (creation_flags & AUTO_MAKE_ABSOLUTE)
        throw std::runtime_error("in Url::Url: constructor flag AUTO_MAKE_ABSOLUTE used with component "
                                 "constructor!");

    url_ = Url::MakeUrl(scheme_, username_password_, authority_, port_, path_, params_, query_, fragment_);

    if ((creation_flags & AUTO_CANONIZE) and not makeCanonical())
        return;
    else if ((creation_flags & AUTO_CLEAN_UP) and not cleanUp())
        return;

    if ((creation_flags & AUTO_MAKE_VALID) and not isValid() and not makeValid())
        error("can't make invalid URL \"" + url_ + "\" valid!");
}


namespace {


// IdentifyPotentiallyUnnecessarySuffixes -- Attempt to trim common suffixes like "index.html".  Returns the length of
//                                           a matched unnecessary suffix or 0 if no potentially unnecessary suffix was
//                                           found.
//
size_t IdentifyPotentiallyUnnecessarySuffixes(const std::string &rest) {
    const std::string::size_type MIN_SUFFIX_LENGTH(8);

    std::string::size_type rest_length = rest.length();
    if (rest_length < MIN_SUFFIX_LENGTH)
        return 0;

    std::string::size_type start_of_suffix;
    std::string::size_type last_slash_pos = rest.rfind('/');
    if (last_slash_pos == std::string::npos)
        start_of_suffix = 0;
    else if (rest_length - last_slash_pos < MIN_SUFFIX_LENGTH + 1)
        return 0;
    else
        start_of_suffix = last_slash_pos + 1;

    const std::string SUFFIX(rest.substr(start_of_suffix));
    const std::string::size_type SUFFIX_LENGTH(SUFFIX.length());

    // Quickly perform suffix tests on common optional suffixes.
#ifdef SuffixTest
#       undef SuffixTest
#endif
#define SuffixTest(test_suffix)                                         \
    if (SUFFIX_LENGTH == __builtin_strlen(test_suffix) and ::strcasecmp(SUFFIX.c_str(), test_suffix) == 0) \
        return __builtin_strlen(test_suffix)

    SuffixTest("index.html");
    else SuffixTest("index.htm");
    else SuffixTest("index.asp");
    else SuffixTest("index.php");
    else SuffixTest("index.php5");
    else SuffixTest("index.php4");
    else SuffixTest("index.php3");
    else SuffixTest("welcome.html");
    else SuffixTest("welcome.htm");
    else SuffixTest("welcome.asp");
    else SuffixTest("welcome.php");
    else SuffixTest("welcome.php5");
    else SuffixTest("welcome.php4");
    else SuffixTest("welcome.php3");
    else SuffixTest("main.asp");
#undef SuffixTest

    return 0;
}


} // unnamed namespace


uint64_t Url::getHash(const HashBehaviour hash_behaviour) const {
    // Strip off trailing slash:
    std::string canonised_url(url_);
    if (url_[url_.length() - 1] == '/') {
        canonised_url.clear();
        canonised_url.append(url_, 0, url_.size() - 1);
    }
    #pragma GCC diagnostic ignored "-Wvla"
    char lowercase_url[canonised_url.length() + 1];
    #pragma GCC diagnostic warning "-Wvla"
    std::strcpy(lowercase_url, canonised_url.c_str());
    const char *c_string = StringUtil::strlower(lowercase_url);

    // Hash the protocol ignoring slashes
    const char *start_pos = std::strchr(c_string, ':');
    uint64_t hash(0);
    if (likely(start_pos != nullptr)) {
        for (const char *cp(c_string); cp != start_pos; ++cp)
            hash = 131 * hash + *cp;
        ++start_pos;
        while (likely(*start_pos == '/'))
            ++start_pos;
    }
    else
        start_pos = c_string;

    if (hash_behaviour == IGNORE_LEADING_WWW and std::strncmp(start_pos, "www.", 4) == 0)
        start_pos += 4;

    // Remove trailing anchor
    const char *last_hash = std::strchr(c_string, '#');
    const char *end_pos;
    if (unlikely(last_hash != nullptr)) {
        end_pos = last_hash;
        if (unlikely(end_pos < start_pos))
            end_pos = start_pos;
    }
    else
        end_pos = c_string + canonised_url.length() + 1;

    const ptrdiff_t trimmed_length = end_pos - start_pos;
    #pragma GCC diagnostic ignored "-Wvla"
    char trimmed_url[trimmed_length + 1];
    #pragma GCC diagnostic warning "-Wvla"
    std::strncpy(trimmed_url, start_pos, trimmed_length);
    trimmed_url[trimmed_length] = '\0';

    // Attempt to trim a common suffix such as "/index.html":
    const size_t suffix_length = IdentifyPotentiallyUnnecessarySuffixes(trimmed_url);
    trimmed_url[trimmed_length - suffix_length - 1] = '\0';

    // Now generate the actual hash value based on _Handbook_of_Algorithms_and_Data_Structures_ (Gonnet/Baeza-Yates,
    // ISBN 0-201-41607-7, Addison-Wesley).

    for (const char *cp = trimmed_url; *cp != '\0'; ++cp)
        hash = 131 * hash + *cp;

    return hash;
}


uint64_t Url::getAuthorityHash() const {
    // Now generate the actual hash value based on _Handbook_of_Algorithms_and_Data_Structures_ (Gonnet/Baeza-Yates,
    // ISBN 0-201-41607-7, Addison-Wesley).

    uint64_t i(0);
    for (const char *cp = authority_.c_str(); *cp != '\0'; ++cp)
        i = 131 * i + *cp;

    return i;
}


unsigned Url::getPathLength() const {
    if (state_ & AN_ERROR_OCCURRED)
        return 0;
    if (not (state_ & HAS_BEEN_PARSED))
        parseUrl(url_);

    unsigned path_length(0);
    bool last_char_was_slash(true);
    for (std::string::const_iterator ch(path_.begin()); ch != path_.end(); ++ch) {
        if (*ch == '/')
            last_char_was_slash = true;
        else {
            if (last_char_was_slash)
                ++path_length;
            last_char_was_slash = false;
        }
    }

    return path_length;
}


bool Url::forceAbsoluteHttp() {
    if (state_ & AN_ERROR_OCCURRED)
        return false;
    if (state_ & HAS_BEEN_MADE_ABSOLUTE)
        return true;

    std::string potential_hostname;
    const std::string::size_type first_slash_pos = url_.find('/');
    if (first_slash_pos == std::string::npos)
        potential_hostname = url_;
    else
        potential_hostname = url_.substr(0, first_slash_pos);

    if (not DnsUtil::IsValidHostName(potential_hostname)) {
        error("can't make relative URL \"" + url_ + "\" into an absolute HTTP URL!");
        return false;
    }

    url_ = "http://" + url_;
    state_ &= ~HAS_BEEN_PARSED;

    return true;
}


namespace {


// IsValidUsernamePassword -- a helper function for Url::internalIsValid
//
bool IsValidUsernamePassword(const std::string &username_password) {
    bool colon_seen = false;

    for (std::string::const_iterator ch(username_password.begin()); ch != username_password.end(); ++ch) {
        if (*ch == ':') {
            if (colon_seen)
                return false;
            colon_seen = true;
        } else if (*ch == '%') {
            if (++ch == username_password.end() or not isxdigit(*ch))
                return false;
            if (++ch == username_password.end() or not isxdigit(*ch))
                return false;
        } else if (not StringUtil::IsAlphanumeric(*ch) and *ch != ';' and *ch != '?' and *ch != '&' and *ch != '='
                 and *ch != '$' and *ch != '-' and *ch != '_' and *ch != '.' and *ch != '+' and *ch != '!'
                 and *ch != '*' and *ch != '\'' and *ch != '(' and *ch != ')' and *ch != ',')
            return false;
    }

    return true;
}


// IsValidScheme -- a helper function for Url::internalIsValid
//
bool IsValidScheme(const std::string &scheme) {
    return scheme.empty() or scheme == "news" or scheme == "http" or scheme == "https" or scheme == "ftp"
           or scheme == "gopher" or scheme == "telnet" or scheme == "local_ucr" or scheme == "mailto"
           or scheme == "file";
}


// IsValidPortNumber -- a helper function for Url::internalIsValid
//
bool IsValidPortNumber(const std::string &port) {
    return port.empty() or (StringUtil::IsUnsignedNumber(port) and StringUtil::ToUnsigned(port) <= 65535);
}


// IsValidNewsgroupName -- a helper function for Url::internalIsValid.
//
//                         Note: valid newsgroup names consist of strings of lowercase letters, digits
//                               plus and minus signs separated by dots.  Each dot separated component
//                               must be no longer than 20 characters.
//
bool IsValidNewsgroupName(const std::string &newsgroup_name) {
    if (newsgroup_name.empty())
        return false;

    bool lowercase_letter_seen(false);
    unsigned current_component_length(0);
    const unsigned MAX_COMPONENT_LENGTH(20);
    for (std::string::const_iterator ch(newsgroup_name.begin()); ch != newsgroup_name.end(); ++ch) {
        if (*ch != '.') {
            if (*ch >= 'a' and *ch <= 'z')
                lowercase_letter_seen = true;
            else if (StringUtil::IsDigit(*ch) and *ch != '-' and *ch != '+')
                return false;

            ++current_component_length;
            if (current_component_length > MAX_COMPONENT_LENGTH)
                return false;
        } else { // We are looking at a period.
            if (not lowercase_letter_seen)
                return false;

            lowercase_letter_seen    = false;
            current_component_length = 0;
        }
    }

    return current_component_length > 0 and lowercase_letter_seen;
}


// IsValidUchar -- see RFC2396 in order to understand this implementation.
//
bool IsValidUchar(std::string::const_iterator &ch, const std::string::const_iterator &end) {
    if (likely(StringUtil::IsAlphanumeric(*ch)))
        return true;

    switch (*ch) {
        /* "safe" */
    case '$':
    case '-':
    case '_':
    case '.':
    case '+':
        /* "extra" */
    case '!':
    case '*':
    case '\'':
    case '(':
    case ')':
    case ',':
        return true;
    }

    if (likely(*ch == '%')) {
        ++ch;
        if (ch == end or not isxdigit(*ch))
            return false;

        ++ch;
        if (ch == end or not isxdigit(*ch))
            return false;

        return true;
    } else
        return false;
}


// IsValidUric -- see RFC2396 in order to understand this implementation.
//
bool IsValidUric(std::string::const_iterator &ch, const std::string::const_iterator &end) {
    if (likely(StringUtil::IsAlphanumeric(*ch)))
        return true;

    switch (*ch) {
        /* "reserved" */
    case ';':
    case '/':
    case '?':
    case ':':
    case '@':
    case '&':
    case '=':
    case '+':
    case '$':
    case ',':
        /* "mark" */
    case '-':
    case '_':
    case '.':
    case '!':
    case '~':
    case '*':
    case '\'':
    case '(':
    case ')':
        return true;
    }

    /* "escaped" */
    if (likely(*ch == '%')) {
        ++ch;
        if (ch == end or not isxdigit(*ch))
            return false;

        ++ch;
        if (ch == end or not isxdigit(*ch))
            return false;

        return true;
    } else
        return false;
}


// IsValidFsegmentChar -- helper function for IsValidFsegment and FixFsegment.
//                        Implementation follows RFC 1738.
//
inline bool IsValidFsegmentChar(std::string::const_iterator &ch, const std::string::const_iterator &end) {
    if (IsValidUchar(ch, end))
        return true;

    switch (*ch) {
    case '?':
    case ':':
    case '@':
    case '&':
    case '=':
        return true;
    }

    return false;
}


// IsValidFsegment -- a helper function for IsValidPath (see RFC2396)
//
bool IsValidFsegment(const std::string &fsegment) {
    for (std::string::const_iterator ch(fsegment.begin()); ch != fsegment.end(); ++ch) {
        if (IsValidFsegmentChar(ch, fsegment.end()))
            continue;

        if (likely(*ch == '%')) {
            ++ch;
            if (ch == fsegment.end() or not isxdigit(*ch))
                return false;

            ++ch;
            if (ch == fsegment.end() or not isxdigit(*ch))
                return false;
        }
        else
            return false;
    }

    return true;
}


// FixFsegment -- a helper function for FixFpath
//
bool FixFsegment(std::string * const fsegment) {
    std::string new_segment;
    new_segment.reserve(fsegment->size());

    for (std::string::const_iterator ch(fsegment->begin()); ch != fsegment->end(); ++ch) {
        if (unlikely(*ch == '%')) {
            ++ch;
            new_segment +=  *ch;
            if (ch == fsegment->end() or not isxdigit(*ch))
                return false;
            new_segment +=  *ch;

            ++ch;
            if (ch == fsegment->end() or not isxdigit(*ch))
                return false;
            new_segment +=  *ch;
        }
        else if (IsValidFsegmentChar(ch, fsegment->end()))
            new_segment += *ch;
        else
            new_segment += UrlUtil::UrlEncodeChar(*ch);
    }

    *fsegment = new_segment;
    return true;
}


// IsValidHsegmentChar -- helper function for IsValidHsegment and FixHsegment.
//
inline bool IsValidHsegmentChar(std::string::const_iterator &ch, const std::string::const_iterator &end) {
    if (IsValidUchar(ch, end))
        return true;

    switch (*ch) {
    case ';':
    case ':':
    case '@':
    case '&':
    case '=':
        return true;
    }

    return false;
}


// IsValidHsegment -- a helper function for IsValidPath (see RFC2396)
//
bool IsValidHsegment(const std::string &hsegment) {
    for (std::string::const_iterator ch(hsegment.begin()); ch != hsegment.end(); ++ch) {
        if (IsValidHsegmentChar(ch, hsegment.end()))
            continue;

        if (likely(*ch == '%')) {
            ++ch;
            if (ch == hsegment.end() or not isxdigit(*ch))
                return false;

            ++ch;
            if (ch == hsegment.end() or not isxdigit(*ch))
                return false;
        }
        else
            return false;
    }

    return true;
}


// FixHsegment -- a helper function for FixHpath
//
bool FixHsegment(std::string * const hsegment) {
    std::string new_segment;
    new_segment.reserve(hsegment->size());

    for (std::string::const_iterator ch(hsegment->begin()); ch != hsegment->end(); ++ch) {
        if (unlikely(*ch == '%')) {
            new_segment += *ch;
            ++ch;
            if (ch == hsegment->end() or not isxdigit(*ch))
                return false;
            new_segment += *ch;

            ++ch;
            if (ch == hsegment->end() or not isxdigit(*ch))
                return false;
            new_segment += *ch;
        }
        else if (IsValidHsegmentChar(ch, hsegment->end()))
            new_segment += *ch;
        else
            new_segment += UrlUtil::UrlEncodeChar(*ch);
    }

    *hsegment = new_segment;
    return true;
}


// IsValidFpath -- a helper function for Url::internalIsValid
//
bool IsValidFpath(const std::string &fpath) {
    if (fpath.empty())
        return true;

    std::list<std::string> segments;
    StringUtil::Split(fpath, "/", &segments);
    if (segments.empty())
        return true;

    for (std::list<std::string>::const_iterator segment(segments.begin()); segment != segments.end(); ++segment)
        if (not IsValidFsegment(*segment))
            return false;

    return true;
}


// FixFpath -- a helper function for Url::makeValid
//
bool FixFpath(std::string * const fpath) {
    if (fpath->empty())
        return true;

    const bool ends_in_slash = (*fpath)[fpath->length() - 1] == '/';

    std::list<std::string> segments;
    StringUtil::Split(*fpath, "/", &segments);
    if (segments.empty())
        return true;

    for (std::list<std::string>::iterator segment(segments.begin()); segment != segments.end(); ++segment)
        if (not FixFsegment(&*segment))
            return false;

    StringUtil::Join(segments, "/", fpath);
    *fpath = "/" + *fpath;
    if (ends_in_slash)
        *fpath += '/';

    return true;
}


// IsValidHpath -- a helper function for Url::internalIsValid
//
bool IsValidHpath(const std::string &hpath) {
    if (hpath.empty())
        return true;

    std::list<std::string> segments;
    StringUtil::Split(hpath, "/", &segments);
    if (segments.empty())
        return true;

    std::list<std::string>::const_iterator segment(segments.begin());

    // Note: We allow for a leading tilde in the first path segment
    //       even though it is against the spec as per RFC2396!
    if (not segment->empty() and (*segment)[0] == '~') {
        if (not IsValidHsegment(segment->substr(1)))
            return false;
        ++segment;
    }

    for (/*empty*/; segment != segments.end(); ++segment)
        if (not IsValidHsegment(*segment))
            return false;

    return true;
}


// FixHpath -- a helper function for Url::makeValid
//
bool FixHpath(std::string * const hpath) {
    if (hpath->empty())
        return true;

    const bool ends_in_slash = (*hpath)[hpath->length() - 1] == '/';

    std::list<std::string> segments;
    StringUtil::Split(*hpath, "/", &segments);
    if (segments.empty())
        return true;

    std::list<std::string>::iterator segment(segments.begin());

    // Note: We allow for a leading tilde in the first path segment
    //       even though it is against the spec as per RFC2396!
    if (not segment->empty()
        and ((*segment)[0] == '~' or ::strncasecmp("%7E", segment->c_str(), 3) == 0))
        {
            std::string temp;
            if (likely((*segment)[0] == '~'))
                temp = segment->substr(1);
            else
                temp = segment->substr(3);
            if (not FixHsegment(&temp))
                return false;
            *segment = "~" + temp;
            ++segment;
        }

    for (/*empty*/; segment != segments.end(); ++segment)
        if (not FixHsegment(&*segment))
            return false;

    StringUtil::Join(segments, "/", hpath);
    *hpath = "/" + *hpath;
    if (ends_in_slash)
        *hpath += '/';

    return true;
}


// FixQuery -- a helper function for Url::makeValid().
//
bool FixQuery(std::string * const query) {
    std::string new_query;
    new_query.reserve(query->size());

    for (std::string::const_iterator ch(query->begin()); ch != query->end(); ++ch) {
        if (unlikely(*ch == '%')) {
            new_query += *ch;
            ++ch;
            if (ch == query->end() or not isxdigit(*ch))
                return false;
            new_query += *ch;

            ++ch;
            if (ch == query->end() or not isxdigit(*ch))
                return false;
            new_query += *ch;
        }
        else if (IsValidUric(ch, query->end()))
            new_query += *ch;
        else
            new_query += UrlUtil::UrlEncodeChar(*ch);
    }

    *query = new_query;
    return true;
}


// AreValidFtpParams -- a helper function for Url::internalIsValid
//
bool AreValidFtpParams(const std::string &params) {
    if (likely(params.empty()))
        return true;

    return params.length() == 6 and params.substr(0, 5) != "type="
        and (params[5] == 'a' or params[5] == 'i' or params[5] == 'd');
}


// IsValidQuery -- see RFC2396 in order to understand this implementation.
//
inline bool IsValidQuery(std::string &query) {
    for (std::string::const_iterator ch(query.begin()); ch != query.end(); ++ch) {
        if (unlikely(not IsValidUric(ch, query.end())))
            return false;
    }

    return true;
}


// IsValidFragment -- a helper function for IsValidPath (see RFC2396)
//
inline bool IsValidFragment(const std::string &fragment) {
    for (std::string::const_iterator ch(fragment.begin()); ch != fragment.end(); ++ch) {
        if (unlikely(not IsValidUric(ch, fragment.end())))
            return false;
    }

    return true;
}


/** \brief  Turns "non_canonical_path" potentially relative path into an absolute path with no embedded references to
 *          current and parent directories using "reference_path" if necessary.
 *  \param  non_canonical_path  A relative or absolute path that may or may not contain embedded "." and ".." components.
 *  \param  reference_path      An absolute reference path.  Must be empty if "non_canonical_path" is absolute.
 *  \param  canonical_path      On return, a canonical, meaning no embedded parent and current directory references,
 *                              absolute path if this function returns true.
 *  \return True if "non_canonical_path" was successfully converted to a canonical path in "canonical_path", else false.
 */
bool CanonizePath(std::string non_canonical_path, std::string reference_path, std::string * const canonical_path) {
    const bool starts_with_slash = not non_canonical_path.empty() and non_canonical_path[0] == '/';
    const bool ends_in_slash = not non_canonical_path.empty()
        and non_canonical_path[non_canonical_path.length() -1] == '/';

    if (starts_with_slash and not reference_path.empty())
        throw std::runtime_error("in CanonizePath (Url.cc): \"non_canonical_path\" is an absolute path and \"reference_path\" is non-empty!");

    // Strip off final component of relative path and store it for later
    std::string last_path_component;
    std::string::size_type last_slash_pos = non_canonical_path.rfind('/');
    if (last_slash_pos != std::string::npos) {
        last_path_component = non_canonical_path.substr(last_slash_pos + 1);
        if (last_path_component == ".." or last_path_component == ".")
            last_path_component.clear();
        else
            non_canonical_path = non_canonical_path.substr(0, last_slash_pos); // Strip off any characters following the last slash
    }

    /* Step 6 a), strip off any characters following the last path component in the reference path. */
    last_slash_pos = reference_path.rfind('/');
    if (last_slash_pos != std::string::npos)
        reference_path = reference_path.substr(0, last_slash_pos); // Strip off any characters following the last slash

    std::list<std::string> segments;
    StringUtil::Split(reference_path, "/", &segments);

    /* Step 6 b) */
    std::list<std::string> non_canonical_path_segments;
    StringUtil::Split(non_canonical_path, "/", &non_canonical_path_segments);
    segments.splice(segments.end(), non_canonical_path_segments);

    /* Steps 6 c) and d), remove single-dot components. */
    for (std::list<std::string>::iterator segment(segments.begin()); segment != segments.end(); /*empty*/) {
        if (*segment == ".")
            segment = segments.erase(segment);
        else
            ++segment;
    }

    /* Step 6 e) and f), remove all occurrences of "<segment>/../", where <segment> is not equal to "..". */
    std::list<std::string>::reverse_iterator segment(segments.rbegin());
    unsigned drop_count(0);
    while (segment != segments.rend()) {
        if (*segment == "..") {
            std::list<std::string>::iterator iter(segment.base());
            --iter;
            segment = std::list<std::string>::reverse_iterator(segments.erase(iter));
            ++drop_count;
        }
        else if (drop_count > 0) {
            --drop_count;
            std::list<std::string>::iterator iter(segment.base());
            --iter;
            segment = std::list<std::string>::reverse_iterator(segments.erase(iter));
        }
        else
            ++segment;
    }

    StringUtil::Join(segments, "/", canonical_path);
    if (not last_path_component.empty()) {
        if (canonical_path->empty() or (*canonical_path)[canonical_path->length() - 1] != '/')
            *canonical_path += "/";
        *canonical_path += last_path_component;
    } else if (ends_in_slash and (*canonical_path)[canonical_path->length() -1 ] != '/')
        *canonical_path += '/';

    if (starts_with_slash and (canonical_path->empty() or (*canonical_path)[0] != '/'))
        *canonical_path = '/' + *canonical_path;

    return true;
}


} // unnamed namespace


bool Url::makeAbsolute(const std::string &override_base_url) {
    if (state_ & AN_ERROR_OCCURRED)
        return false;
    if (state_ & HAS_BEEN_MADE_ABSOLUTE)
        return true;
    if ((state_ & HAS_BEEN_PARSED) == 0 and not parseUrl(url_))
        return false;

    // Is this URL already absolute?
    if (isAbsolute()) {
        state_ |= HAS_BEEN_MADE_ABSOLUTE; // Let's not do this again!
        return true;
    }

    std::string base_scheme, base_username_password, base_authority, base_port, base_path, base_params, base_query, base_fragment,
        base_relative_url;
    if (not UrlUtil::ParseUrl(override_base_url.empty() ? default_base_url_ : override_base_url, &base_scheme, &base_username_password,
                              &base_authority, &base_port, &base_path, &base_params, &base_query, &base_fragment, &base_relative_url)
        or not base_relative_url.empty())
    {
        error("in Url::makeAbsolute: base URL cannot be parsed or is relative!");
        return false;
    }

    // If the URL is empty, replace it with the base_url:
    if (url_.empty()) {
        url_ = override_base_url.empty() ? default_base_url_ : override_base_url;
        if (not url_.empty()) {
            scheme_            = base_scheme;
            username_password_ = base_username_password;
            authority_         = base_authority;
            port_              = base_port;
            path_              = base_path;
            params_            = base_params;
            query_             = base_query;
            fragment_          = base_fragment;

            // Note: in the following assignment we intentionally do not use "or"
            // to assign flags, as we want  start with a clean slate!
            state_ = HAS_BEEN_PARSED | HAS_BEEN_MADE_ABSOLUTE;
        }

        return true;
    }

    //
    // At this point, we have an absolute base_url, and a relative, non-empty current URL.
    //

    if (relative_url_[0] == ':')
        url_ = base_scheme + relative_url_;
    else if (relative_url_.length() >= 2 and relative_url_[0] == '/' and relative_url_[1] == '/')
        url_ = base_scheme + ":" + relative_url_;
    else { // Assume "relative_url_" is a relative or absolute path.
        url_ = base_scheme + "://";
        if (not base_username_password.empty())
            url_ += base_username_password + "@";
        url_ += base_authority;

        // Handle the port:
        if (not base_port.empty()
            and not ((base_scheme == "http" and base_port == "80") or (base_scheme == "https" and base_port == "443")))
            url_ += ":" + base_port;

        // Handle the path:
        if (unlikely(base_scheme == "telnet"))
            url_ += '/';
        else if (relative_url_[0] == '/') // We assume that "relative_url_" is an absolute path.
            url_ += relative_url_;
        else {  // We assume that "relative_url_" is a relative path.
            if (not CanonizePath(relative_url_, base_path, &relative_url_)) {
                error("in Url::makeAbsolute: cannot make relative path absolute!");
                return false;
            }
            if (not relative_url_.empty() and relative_url_[0] != '/')
                url_ += '/';
            url_ += relative_url_;
        }
    }

    state_ = UNINITIALISED;
    if (not parseUrl(url_))
        return false;

    if (relative_url_.empty())
        state_ |= HAS_BEEN_MADE_ABSOLUTE;
    return relative_url_.empty();
}


// Url::getRobotsDotTxtUrl -- generate a RobotsDotTxt URL based on this Url object.
//
std::string Url::getRobotsDotTxtUrl() const {
    if ((state_ & HAS_BEEN_PARSED) == 0) {
        if (not UrlUtil::ParseUrl(url_, &scheme_, &username_password_, &authority_, &port_, &path_, &params_,
                                  &query_, &fragment_, &relative_url_)
            or not relative_url_.empty())
            return std::string();
        state_ |= HAS_BEEN_PARSED;
    }

    if (scheme_ == "http" or scheme_ == "https")
        return Url::MakeUrl(scheme_, username_password_, authority_, port_, "/robots.txt", "", "", "");

    return std::string();
}


//  Url::isBlacklisted -- Is the URL blacklisted by the local configuration.
//
bool Url::isBlacklisted() const {
    if (not isValidWebUrl())
        return true;

    static UrlUtil::Blacklister blacklister(Url::UrlBlacklistConfFile());

    return blacklister.blocked(url_);
}


// Url::UrlBlacklistConfFile -- Get the path of the URL blacklist file.
//
std::string Url::UrlBlacklistConfFile() {
    return ETC_DIR "/UrlBlacklist.conf";
}


// Url::isChildOrSiblingUrl -- is the given URL a child or sibling of the current one?
//
bool Url::isChildOrSiblingUrl(const Url &test_url) const {
    if (not isValid() or not test_url.isValid())
        return false;

    if (getScheme() != test_url.getScheme() or getPort() != test_url.getPort())
        return false;

    if (getAuthority() != test_url.getAuthority()
        and not StringUtil::Match("*." + getAuthority(), test_url.getAuthority()))
        return false;

    if (getPath().empty())
        return true;

    if (test_url.getPath().empty())
        return false;

    std::string::size_type test_length = getPath().rfind('/');
    if (test_length == std::string::npos)
        return true;

    return ::strncasecmp(getPath().c_str(), test_url.getPath().c_str(), test_length) == 0;
}


bool Url::internalIsValid() const {
    if (state_ & AN_ERROR_OCCURRED)
        return false;

    // Note: We can't bail out early if the VALIDITY_HAS_BEEN_CHECKED is already set in "state_"
    //       because the client may have attempted a makeValid() after the first validity check!

    state_ |= VALIDITY_HAS_BEEN_CHECKED; // at least once, that is.

    // We cheat if we have a relative URL we always claim it's valid:
    if (not relative_url_.empty()) {
        state_ |= IS_VALID;
        return true;
    }

    try {
        if ((state_ & HAS_BEEN_PARSED) == 0 and not parseUrl(url_))
            return false;

        if (not IsValidScheme(scheme_))
            return false;

        if (not IsValidUsernamePassword(username_password_))
            return false;

        // For "local_ucr" or "file" we allow any non-empty authority:
        if (scheme_ == "local_ucr" or scheme_ == "file") { // Accept almost anything!
            if (not authority_.empty()) {
                state_ |= IS_VALID;
                return true;
            } else
                return false;
        }

        if (unlikely(not IsValidPortNumber(port_)))
            return false;

        if (scheme_ == "news") {
            if (IsValidNewsgroupName(authority_)) {
                state_ |= IS_VALID;
                return true;
            } else
                return false;
        }

        if (not scheme_.empty() and authority_.empty())
            return false;

        if (not (authority_.empty() or DnsUtil::IsValidHostName(authority_) or DnsUtil::IsDottedQuad(authority_)))
            return false;

        if (scheme_ == "telnet") {
            if (not authority_.empty() and path_.empty()
                and query_.empty() and fragment_.empty())
            {
                state_ |= IS_VALID;
                return true;
            } else
                return false;
        }

        if (likely(scheme_ == "http" or scheme_ == "https")) {
            if (not IsValidHpath(path_) or not IsValidQuery(query_))
                return false;
        } else if (scheme_ == "ftp" and not IsValidFpath(path_) and not AreValidFtpParams(params_))
            return false;

        if (not fragment_.empty() and not IsValidFragment(fragment_))
            return false;
    } catch (const std::exception &x) {
        error("in Url::internalIsValid: caught exception: " + std::string(x.what()));
        return false;
    }

    state_ |= IS_VALID;
    return true;
}


bool Url::parseUrl(const std::string &url) const {
    if (state_ & AN_ERROR_OCCURRED)
        return false;
    state_ |= HAS_BEEN_PARSED;

    return UrlUtil::ParseUrl(url, &scheme_, &username_password_, &authority_, &port_, &path_, &params_, &query_, &fragment_,
                             &relative_url_);
}


bool Url::isValidWebUrl() const {
    if (state_ & AN_ERROR_OCCURRED)
        return false;
    if (not (state_ & VALIDITY_HAS_BEEN_CHECKED))
        internalIsValid();
    if (not (state_ & IS_VALID))
        return false;
    if (not (state_ & HAS_BEEN_PARSED))
        parseUrl(url_);
    return scheme_ == "http" or scheme_ == "https";
}


namespace {


// UppercaseHexEscapes -- makes sure that any hexadecimal digits that are letters in %XX escapes
//                        are in uppercase.
//
std::string UppercaseHexEscapes(std::string * const url) {
    std::string new_url;
    for (std::string::const_iterator ch(url->begin()); ch != url->end(); ++ch) {
        if (*ch != '%')
            new_url += *ch;
        else {
            new_url += '%';
            ++ch;
            if (ch == url->end())
                break;
            if (not isxdigit(*ch)) {
                new_url += *ch;
                continue;
            }
            new_url += toupper(*ch);
            ++ch;
            if (ch == url->end())
                break;
            if (not isxdigit(*ch))
                new_url += *ch;
            else
                new_url += toupper(*ch);
        }
    }

    return *url = new_url;
}


} // unnamed namespace


bool Url::cleanUp() {
    if (state_ & AN_ERROR_OCCURRED)
        return false;
    if (state_ & HAS_BEEN_CLEANED_UP) // No need to bother if we've already done this.
        return true;
    state_ |= HAS_BEEN_CLEANED_UP;

    if ((state_ & HAS_BEEN_PARSED) == 0 and not parseUrl(url_))
        return false;

    // If this is a relative URL, we may be able to make it into an HTTP URL:
    if (scheme_.empty() and default_base_url_.empty() and not relative_url_.empty()) {
        std::list<std::string> segments;
        StringUtil::Split(relative_url_, "/", &segments);
        if (not segments.empty() and DnsUtil::IsValidHostName(segments.front())) {
            scheme_    = "http";
            authority_ = segments.front();
            segments.pop_front();
            path_ = "/" + StringUtil::Join(segments, "/");
        }

        setUrlFromComponents(state_);
    }

    // Do nothing to relative URLs
    if (not isAbsolute())
        return true;

    // For "local_ucr" we allow any garbage:
    if (scheme_ == "local_ucr")
        return true;

    // Clean up case
    StringUtil::ToLower(&authority_);
    StringUtil::Trim(&authority_);
    UppercaseHexEscapes(&username_password_);
    UppercaseHexEscapes(&path_);
    StringUtil::Trim(&path_);
    UppercaseHexEscapes(&params_);
    UppercaseHexEscapes(&query_);
    UppercaseHexEscapes(&fragment_);

    if (likely(scheme_ == "http" or scheme_ == "https")) {
        if (not FixHpath(&path_))
            return false;
    } else if (scheme_ == "ftp") {
        if (not FixFpath(&path_))
            return false;
    }

    if (not CanonizePath(path_, "", &path_))
        return false;

    setUrlFromComponents(state_);

    return internalIsValid();
}


void Url::toLower() {
    StringUtil::ToLower(&url_);
    StringUtil::ToLower(&default_base_url_);
    StringUtil::ToLower(&scheme_);
    StringUtil::ToLower(&username_password_);
    StringUtil::ToLower(&authority_);
    StringUtil::ToLower(&port_);
    StringUtil::ToLower(&path_);
    StringUtil::ToLower(&params_);
    StringUtil::ToLower(&query_);
    StringUtil::ToLower(&fragment_);
}


bool Url::makeValid() {
    if (state_ & AN_ERROR_OCCURRED)
        return false;
    if (state_ & IS_VALID) // No need to bother if we're already valid.
        return true;
    if (state_ & MADE_VALID_ATTEMPTED) // Have we tried this before?
        return state_ & IS_VALID;
    state_ |= MADE_VALID_ATTEMPTED;

    try {
        // Trim whitespace:
        std::string trimmed_url(url_);
        StringUtil::Trim(" \t\r\n", &trimmed_url);
        std::string::size_type trimmed_url_length = trimmed_url.length();

        // Fix some known problem cases.

        // news: URLs have to have trailing slashes trimmed.
        if (trimmed_url_length >= 5 and  trimmed_url.substr(0, 5) == "news:") {
            StringUtil::RightTrim(&trimmed_url, '/');
            url_ = trimmed_url;
        }
        // mailto: URLs have to have trailing slashes trimmed.
        else if (trimmed_url_length >= 7 and  trimmed_url.substr(0, 7) == "mailto:") {
            StringUtil::RightTrim(&trimmed_url, '/');
            url_ = trimmed_url;
        }
        // file: URLs.
        else if (trimmed_url_length >= 5 and  trimmed_url.substr(0, 5) == "file:")
            url_ = trimmed_url;
        // URLS with no "://" part are assumed to be HTTP URLs.
        else if (trimmed_url.find("://") == std::string::npos)
            trimmed_url = "http://" + trimmed_url;

        // Attempt to parse the URL:
        if (not parseUrl(trimmed_url))
            return false;

        StringUtil::Trim(&authority_);
        StringUtil::Trim(&path_);

        if (likely(scheme_ == "http" or scheme_ == "https")) {
            if (not FixHpath(&path_))
                return false;
            if (not query_.empty() and not FixQuery(&query_))
                return false;
        } else if (scheme_ == "ftp") {
            if (not FixFpath(&path_))
                return false;
        } else if (scheme_ == "file")
            return true;
        else if (scheme_.empty() and default_base_url_.empty() and not path_.empty()) {
            std::list<std::string> segments;
            StringUtil::Split(path_, "/", &segments);
            if (not segments.empty() and DnsUtil::IsValidHostName(segments.front())) {
                scheme_    = "http";
                authority_ = segments.front();
                segments.pop_front();
                path_ = "/" + StringUtil::Join(segments, "/");
            }
        }

        StringUtil::ToLower(&authority_);
        UppercaseHexEscapes(&username_password_);
        UppercaseHexEscapes(&path_);
        UppercaseHexEscapes(&params_);
        UppercaseHexEscapes(&query_);
        UppercaseHexEscapes(&fragment_);

        setUrlFromComponents(state_);
        return internalIsValid();
    } catch (const std::exception &x) {
        error("in Url::makeValid: caught exception: " + std::string(x.what()));
        return false;
    }
}


#ifdef VERSIGN_SITE_FINDER_HACK
bool IsVeriSignSiteFinderUrl(const std::string &url, Downloader * const downloader, const TimeLimit &time_limit) {
    if (unlikely(::strncasecmp(url.c_str(), "http://sitefinder.verisign.com/", 31) == 0))
        return true;

    // Download the page:
    downloader->newUrl(url, time_limit);
    if (downloader->anErrorOccurred())
        return false;

    HttpHeader http_header(downloader->getMessageHeader());
    if (http_header.getStatusCode() < 200 or http_header.getStatusCode() > 299)
        return false;

    if (unlikely(not http_header.getLocation().empty()
                 and ::strncasecmp(http_header.getLocation().c_str(), "http://sitefinder.verisign.com/", 31) == 0))
        return true;

    return false;
}
#endif


#ifdef CENTRAL_NIC_HACK
bool IsCentralNicUrl(const std::string &url, Downloader * const downloader, const TimeLimit &time_limit) {
    if (unlikely(::strncasecmp(url.c_str(), "http://www.uk.com/", 18) == 0))
        return true;

    // Download the page:
    downloader->newUrl(url, time_limit);
    if (downloader->anErrorOccurred())
        return false;

    HttpHeader http_header(downloader->getMessageHeader());
    if (http_header.getStatusCode() < 200 or http_header.getStatusCode() > 299)
        return false;

    if (unlikely(not http_header.getLocation().empty()
                 and ::strncasecmp(http_header.getLocation().c_str(), "http://www.uk.com/", 18) == 0))
        return true;

    return false;
}
#endif


// IsIdenticalDocument -- a helper function for Url::makeCanonical.  Called with the headers of two documents that
//                        are presumed to be the same document.  Compares the two and headers using etags if they are
//                        available, if not, we try last modified dates and if those aren't available or don't match
//                        we give up.
//
bool IsIdenticalDocument(const std::string &url1, const HttpHeader &http_header1, const std::string &url2,
                         const HttpHeader &http_header2)
{
    if (not http_header1.getETag().empty() and not http_header2.getETag().empty())
        return http_header1.getETag() == http_header2.getETag();

    if (http_header1.getLocation() == url2 or http_header2.getLocation() == url1)
        return true;

    if (not http_header1.getLocation().empty() and not http_header2.getLocation().empty())
        return http_header1.getLocation() == http_header2.getLocation();

    if (http_header1.getContentLength() != 0 and http_header2.getContentLength() != 0)
        return http_header1.getContentLength() == http_header2.getContentLength();

    if (http_header1.lastModifiedIsValid() and http_header2.lastModifiedIsValid())
        return http_header1.getLastModified() == http_header2.getLastModified();

    return false;
}


// TestNewURL -- helper for makeCanonial().  Determines whether "new_url" is equivalent to "original_url".  Returns true if we convinced ourselves that it
//               leads to the same resource as the original URL.
//               Note: "original_http_header" has to be a valid HTTP header for "original_url".
//
bool TestNewURL(const std::string &original_url, const HttpHeader &original_http_header, const std::string &original_content_hash,
                const std::string &new_url, const TimeLimit &time_limit, Downloader * const downloader)
{
    // Download the page
    downloader->newUrl(new_url, time_limit);
    if (downloader->anErrorOccurred())
        return false;

    HttpHeader new_http_header(downloader->getMessageHeader());
    if (new_http_header.getStatusCode() < 200 or new_http_header.getStatusCode() > 299)
        return false;

    // Compare the old and new content:
    if (not original_content_hash.empty() and original_content_hash == StringUtil::Sha1(downloader->getMessageBody()))
        return true;

    // Compare the old and new headers:
    return IsIdenticalDocument(original_url, original_http_header, new_url, new_http_header);
}


bool Url::makeCanonical(const unsigned override_timeout) {
    if (state_ & AN_ERROR_OCCURRED)
        return false;
    if (state_ & CANONIZATION_ATTEMPTED)
        return state_ & IS_CANONICAL;
    state_ |= CANONIZATION_ATTEMPTED;

    try {
        // First clean up the URL
        makeValid();
        cleanUp();
        if (not isValid())
            return false;

        // Get a list of potential canonized forms:
        std::list<std::string> potential_urls;
        if (not Url::SuggestPotentialCanonicalUrls(this->toString(), &potential_urls))
            return false;

        // If one URL is returned, it must be the original URL (or safe variant), and canonical.
        if (potential_urls.size() == 1) {
            state_ &= ~HAS_BEEN_PARSED;
            state_ |= IS_CANONICAL;
            url_ = potential_urls.front();
            return true;
        }

        //
        // If we made it this far, we have a Web protocol and several potential canonical forms.
        //

        // Make sure we have a valid, i.e. non-empty user agent string:
        const std::string user_agent_string(user_agent_.empty() ? Url::default_user_agent_ : user_agent_);
        if (unlikely(user_agent_string.empty()))
            throw std::runtime_error("in Url::makeCanonical: need a non-empty user agent string!");

        // Set up Web page fetcher and its parameters:
        Downloader::Params downloader_params(
            user_agent_string, Downloader::DEFAULT_ACCEPTABLE_LANGUAGES, Downloader::DEFAULT_MAX_REDIRECTS,
            /* dns_cache_timeout = */ 600,
            /* honour_robots_dot_txt = */ robots_dot_txt_option_ == Url::CONSULT_ROBOTS_DOT_TXT);
        const TimeLimit time_limit(override_timeout != 0 ? override_timeout : timeout_);

        // Retrieve the "original" URL
        Downloader downloader(*this, downloader_params, time_limit);
        const HttpHeader original_http_header(downloader.getMessageHeader());
        const std::string original_content_hash(StringUtil::Sha1(downloader.getMessageBody()));

        // Work through the possible candidates until we find
        // one that matches the original.
        for (std::list<std::string>::const_iterator potential_url(potential_urls.begin());
             potential_url != potential_urls.end(); ++potential_url)
        {
#ifdef VERSIGN_SITE_FINDER_HACK
            if (IsVeriSignSiteFinderUrl(*potential_url, &downloader, time_limit))
                continue;
#endif

#ifdef CENTRAL_NIC_HACK
            if (IsCentralNicUrl(*potential_url, &downloader, time_limit))
                continue;
#endif

            if (TestNewURL(this->toString(), original_http_header, original_content_hash, *potential_url,
                           time_limit, &downloader))
            {
                url_ = *potential_url;
                state_ &= ~HAS_BEEN_PARSED;
                state_ |= IS_CANONICAL;
                return true;
            }
        }

        // If we make it here we declare the original URL to be the canonical one:
        state_ &= ~HAS_BEEN_PARSED;
        state_ |= IS_CANONICAL;
        return true;
    } catch (const std::exception &x) {
        error("in Url::makeCanonical: caught exception: "  + std::string(x.what()));
        return false;
    }
}


std::string Url::getPortAsString(const bool get_default_when_port_not_set) const {
    if (state_ & AN_ERROR_OCCURRED)
        return 0;
    if (not (state_ & HAS_BEEN_PARSED))
        parseUrl(url_);

    if (port_.empty() and get_default_when_port_not_set) {
        if (scheme_ == "http")
            return "80";
        else if (scheme_ == "https")
            return "443";
        else if (scheme_ == "ftp")
            return "21";
        else if (scheme_ == "telnet")
            return "23";
    }

    return port_;
}


unsigned short Url::getPort() const {
    if (state_ & AN_ERROR_OCCURRED)
        return 0;
    if (not (state_ & HAS_BEEN_PARSED))
        parseUrl(url_);

    if (scheme_.empty())
        throw std::runtime_error("in Url::getPort: port undefined for relative URL (" + url_ + ")!");

    std::string port = getPortAsString(true);
    if (port.empty())
        throw std::runtime_error("in Url::getPort: unimplemented: default port for scheme \"" + scheme_ + "\"!");

    unsigned n;
    if (not StringUtil::ToNumber(port, &n) or n > 65535) {
        error("in Url::getPort: \"" + port + "\" is not a valid port number!");
        return 0;
    }

    return n;
}


namespace {


bool url_config_file_loaded = false;
std::list< std::pair<std::string, std::string> > pattern_to_site_map;


void LoadUrlConfigFile() {
    url_config_file_loaded = true;

    const std::string config_filename(ETC_DIR + std::string("/UrlGetSite.conf"));
    std::ifstream input(config_filename.c_str());
    if (input.fail())
        throw std::runtime_error("in LoadUrlConfigFile (Url.cc): can't open \"" + config_filename
                                 + "\" for reading!");

    unsigned line_no(0);
    while (input) {
        ++line_no;
        std::string line;
        std::getline(input, line);

        // Comment processing:
        std::string::size_type hash_pos = line.find('#');
        if (hash_pos != std::string::npos)
            line = line.substr(0, hash_pos);

        if (StringUtil::Trim(&line).empty())
            continue;

        std::string::size_type equal_pos = line.find('=');
        if (equal_pos == std::string::npos)
            throw std::runtime_error("in LoadUrlConfigFile (Url.cc): malformed input in \"" + config_filename
                                     + "\" on line " + StringUtil::ToString(line_no) + "!");

        pattern_to_site_map.push_back(std::make_pair<std::string, std::string>(line.substr(equal_pos + 1),
                                                                               line.substr(0, equal_pos)));
    }
}


} // unnamed namespace


std::string Url::getSite() const {
    if (state_ & AN_ERROR_OCCURRED)
        return "";
    if (not (state_ & HAS_BEEN_PARSED))
        parseUrl(url_);

    if (scheme_ != "http" and scheme_ != "https" and scheme_ != "ftp")
        return "";

    if (not url_config_file_loaded)
        LoadUrlConfigFile();

    for (const auto &pair : pattern_to_site_map) {
        if (PerlCompatRegExp::Match(pair.first, url_, PCRE_ANCHORED | PCRE_CASELESS))
            return pair.second;

    }

    std::string lowercase_authority(authority_);
    StringUtil::ToLower(&lowercase_authority);

    std::string site;
    if (scheme_ == "http" or scheme_ == "http") {
        if (lowercase_authority.length() > 4 and lowercase_authority.substr(0, 4) == "www.")
            site = lowercase_authority.substr(4);
        else
            site = lowercase_authority;
    } else if (lowercase_authority.length() > 4 and lowercase_authority.substr(0, 4) == "ftp.")
        site = lowercase_authority.substr(4);
    else
        site = lowercase_authority;

    if (::strncasecmp(path_.c_str(), "/users/", 7) == 0 and path_.length() > 7) {
        site += "/users/";
        std::string remainder(path_.substr(7));
        std::string::size_type slash_pos = remainder.find('/');
        if (slash_pos == std::string::npos)
            site += remainder;
        else
            site += remainder.substr(0, slash_pos);
    } else if (path_.length() > 2 and path_[0] == '/' and path_[1] == '~') {
        std::string::size_type slash_pos = path_.find('/', 1);
        if (slash_pos == std::string::npos)
            site += path_;
        else
            site += path_.substr(0, slash_pos);
    }

    return site;
}


bool Url::setScheme(const std::string &scheme) {
    if (anErrorOccurred())
        return false;
    if (not (state_ & HAS_BEEN_PARSED) and not parseUrl(url_))
        return false;

    if (not relative_url_.empty()) {
        error("You cannot set the 'scheme' component of a relative URL!");
        return false;
    }

    scheme_ = scheme;
    setUrlFromComponents();
    return true;
}


bool Url::setUsernamePassword(const std::string &username_password) {
    if (anErrorOccurred())
        return false;
    if (not (state_ & HAS_BEEN_PARSED) and not parseUrl(url_))
        return false;

    if (not relative_url_.empty()) {
        error("You cannot set the 'username_password' component of a relative URL!");
        return false;
    }

    username_password_ = username_password;
    setUrlFromComponents();
    return true;
}


bool Url::setAuthority(const std::string &authority) {
    if (anErrorOccurred())
        return false;
    if (not (state_ & HAS_BEEN_PARSED) and not parseUrl(url_))
        return false;

    if (not relative_url_.empty()) {
        error("You cannot set the 'authority' component of a relative URL!");
        return false;
    }

    authority_ = authority;
    setUrlFromComponents();
    return true;
}


bool Url::setPort(const std::string &port) {
    if (anErrorOccurred())
        return false;
    if (not (state_ & HAS_BEEN_PARSED) and not parseUrl(url_))
        return false;
    if (StringUtil::IsUnsignedDecimalNumber(port))
        return false;

    if (not relative_url_.empty()) {
        error("You cannot set the 'port' component of a relative URL!");
        return false;
    }

    port_ = port;
    setUrlFromComponents();
    return true;
}


bool Url::setPort(const unsigned short port) {
    if (anErrorOccurred())
        return false;
    if (not (state_ & HAS_BEEN_PARSED) and not parseUrl(url_))
        return false;

    if (not relative_url_.empty()) {
        error("You cannot set the 'port' component of a relative URL!");
        return false;
    }

    port_ = StringUtil::ToString(port);
    setUrlFromComponents();
    return true;
}


bool Url::setPath(const std::string &path) {
    if (anErrorOccurred())
        return false;
    if (not (state_ & HAS_BEEN_PARSED) and not parseUrl(url_))
        return false;

    if (not relative_url_.empty()) {
        error("You cannot set the 'path' component of a relative URL!");
        return false;
    }

    path_ = path;
    setUrlFromComponents();
    return true;
}


bool Url::setParams(const std::string &params) {
    if (anErrorOccurred())
        return false;
    if (not (state_ & HAS_BEEN_PARSED) and not parseUrl(url_))
        return false;

    if (not relative_url_.empty()) {
        error("You cannot set the 'params' component of a relative URL!");
        return false;
    }

    params_ = params;
    setUrlFromComponents();
    return true;
}


bool Url::setQuery(const std::string &query) {
    if (anErrorOccurred())
        return false;
    if (not (state_ & HAS_BEEN_PARSED) and not parseUrl(url_))
        return false;

    if (not relative_url_.empty()) {
        error("You cannot set the 'query' component of a relative URL!");
        return false;
    }

    query_ = query;
    setUrlFromComponents();
    return true;
}


bool Url::setFragment(const std::string &fragment) {
    if (anErrorOccurred())
        return false;
    if (not (state_ & HAS_BEEN_PARSED) and not parseUrl(url_))
        return false;

    if (not relative_url_.empty()) {
        error("You cannot set the 'fragment' component of a relative URL!");
        return false;
    }

    fragment_ = fragment;
    setUrlFromComponents();
    return true;
}


// MakeUrl -- assemble a URL given the components.
//
std::string Url::MakeUrl(const std::string &scheme, const std::string &username_password, const std::string &authority,
                         const std::string &port, const std::string &path, const std::string &params,
                         const std::string &query, const std::string &fragment)
{
    std::string url;

    if (scheme == "news")
        url = "news:" + authority;
    else {
        if (not scheme.empty())
            url += scheme + "://";
        if (not username_password.empty())
            url += username_password + "@";
        if (not authority.empty())
            url += authority;
        if (not port.empty() and not ((scheme == "http" and port == "80") or (scheme == "https" and port == "443")))
            url += ":" + port;

        if (likely(scheme != "telnet")) {
            if (path.empty() and not (params.empty() and query.empty() and fragment.empty()))
                url += '/';
            else
                url += path;
        } else {
            url += '/';
            return url;
        }

        if (likely(not params.empty()))
            url += ";" + params;
        if (not query.empty())
            url += "?" + query;

        if ((scheme == "http" or scheme == "https") and not fragment.empty())
            url += "#" + fragment;
    }

    return url;
}


namespace { // Helpers for TransformUrl.


class UrlMap {
    std::string from_pattern_, to_pattern_;
public:
    UrlMap(const std::string &from_pattern, const std::string &to_pattern)
        : from_pattern_(from_pattern), to_pattern_(to_pattern) { }
    UrlMap(const UrlMap &rhs)
        : from_pattern_(rhs.from_pattern_), to_pattern_(rhs.to_pattern_) { }
    const std::string &getFromPattern() const { return from_pattern_; }
    const std::string &getToPattern() const { return to_pattern_; }
private:
    UrlMap() = delete;
};


class UrlMaps: public SList<UrlMap> {
public:
        void push_back(const std::string &from_pattern, const std::string &to_pattern)
                { SList<UrlMap>::push_back(UrlMap(from_pattern, to_pattern)); }
        std::string map(const std::string &url) const;
};


std::string UrlMaps::map(const std::string &url) const {
    for (const_iterator url_map(begin()); url_map != end(); ++url_map) {
        const std::string new_url(PerlCompatRegExp::Subst(url_map->getFromPattern(), url_map->getToPattern(), url,
                                                          false/*= global*/, PCRE_ANCHORED));
        if (new_url != url)
            return new_url;
    }

    return url;
}


void LoadUrlMaps(UrlMaps * const url_maps) {
    const std::string CONF_FILENAME(ETC_DIR + std::string("/UrlMaps.conf"));
    std::ifstream url_maps_conf(CONF_FILENAME.c_str());
    if (url_maps_conf.fail())
        return;

    unsigned line_no = 0;
    while (url_maps_conf) {
        std::string line;
        std::getline(url_maps_conf, line);
        ++line_no;

        // Strip out comments:
        std::string::size_type hash_pos = line.find('#');
        if (hash_pos != std::string::npos)
            line = line.substr(0, hash_pos);

        StringUtil::Trim(&line);

        if (line.empty())
            continue;

        std::list<std::string> patterns;
        StringUtil::SplitThenTrim(line, " \t", " \t", &patterns);
        if (patterns.size() != 2)
            throw std::runtime_error("in LoadUrlMaps(Url.cc): invalid entry in \""
                                     + CONF_FILENAME + "\" on line "
                                     + StringUtil::ToString(line_no) + "!");

        url_maps->push_back(patterns.front(), patterns.back());
    }
}


} // unnamed namespace -- Helpers for TransformUrl.


std::string Url::TransformUrl(const std::string &url) {
    static UrlMaps url_maps;
    LoadUrlMaps(&url_maps);
    return url_maps.map(url);
}


bool Url::transform() {
    if (not isValidWebUrl())
        return false;

    // Load the blacklists
    std::string new_url(TransformUrl(url_));

    if (new_url == url_)
        return false;

    url_ = new_url;
    state_ = 0;
    return true;
}


namespace { // Helper functions for SuggestPotentialCanonicalUrls.


// HostLessThan -- Helper function for SuggestPotentialCanonicalUrls.
//
bool HostLessThan(const std::string &host1, const std::string &host2) {
    // First, prefer hostnames that start with "www."
    if ((host1.length() > 4 and host1.substr(0, 4) == "www.")
        and (host2.length() <= 4 or host2.substr(0, 4) != "www."))
        return true;
    else if ((host2.length() > 4 and host2.substr(0, 4) == "www.")
             and (host1.length() <= 4 or host1.substr(0, 4) != "www."))
        return false;

    // Prefer shorter hostnames
    else if (host1.length() < host2.length())
        return true;
    else if (host1.length() > host2.length())
        return false;

    // If all else fails, sort alphabetically.
    else
        return (host1 < host2);
}


// RestLessThan -- Helper function for SuggestPotentialCanonicalUrls.
//
bool RestLessThan(const std::string &rest1, const std::string &rest2) {
    // Prefer shorter paths
    if (rest1.length() < rest2.length())
        return true;
    else if (rest1.length() > rest2.length())
        return false;

    // Prefer all-lowercase: if rest1 is all-lowercase, it comes before rest2
    else if (::strcasecmp(rest1.c_str(), rest2.c_str()) == 0)
        return (rest1 != rest2) and (rest1 == StringUtil::ToLower(rest1));

    // If all else fails, sort alphabetically.
    else
        return (rest1 < rest2);
}


// GetSuggestedWwwHost -- Suggest alternate form of URL using "www" prefix
//
std::string GetSuggestedWwwHost(const std::string &hostname) {
    if (hostname.length() < 3)
        return hostname;

    if (hostname.substr(0, 3) != "www")
        return "www." + hostname;

    std::string::size_type dot_pos = hostname.find('.');
    if (dot_pos == 3 // hostname starts with "www." already
        or dot_pos == std::string::npos // hostname has no '.' in it.
        or not StringUtil::IsUnsignedNumber(hostname.substr(3, dot_pos-3))) // hostname is not "wwwN."
        return hostname;

    return "www" + hostname.substr(dot_pos); // Replace "wwwN." with "www."

}


} // unnamed namespace


// Url::SuggestPotentialCanonicalUrls -- Suggest possible canonical forms of a URL.
//
bool Url::SuggestPotentialCanonicalUrls(const std::string &original_url, std::list<std::string> * const potential_urls,
                                        const bool resolve_ip_address)
{
    potential_urls->clear();
    Url url(original_url, Url::AUTO_CLEAN_UP);

    // A canonical url is necessarily absolute.  If we're passed a
    // relative URL, try blindly making it an absolute URL.
    if (not url.isAbsolute()) {
        std::string hack_url("http://" + original_url);
        url = Url(hack_url, Url::AUTO_CLEAN_UP);
    }

    if (not url.isValid())
        return false;

    // For non-HTTP schemes, we do very little (since
    // these are created by human experts, not robots).
    const std::string scheme = url.getScheme();
    if (scheme != "http" and scheme != "https") {
        potential_urls->push_back(url.toString());
        return true;
    }

    // If we made it this far, we have an HTTP or HTTPS scheme.
    std::set<std::string> possible_hosts, possible_paths;

    // Get a set of candiate hostnames:
    const std::string hostname = url.getAuthority();
    if (resolve_ip_address and DnsUtil::IsDottedQuad(hostname)) {
        // We have an IP address instead of a proper host name and want to translate it:
        std::list<std::string> proper_hostnames;
        if (DnsUtil::IpAddrToHostnames(hostname, &proper_hostnames)) {
            for (std::list<std::string>::iterator proper_hostname(proper_hostnames.begin());
                 proper_hostname != proper_hostnames.end(); ++proper_hostname)
            {
                if (proper_hostname->find('.') == std::string::npos)
                    continue;
                possible_hosts.insert(StringUtil::ToLower(&(*proper_hostname)));
                possible_hosts.insert(GetSuggestedWwwHost(*proper_hostname));
            }
        }
    } else {
        possible_hosts.insert(hostname);
        if (not DnsUtil::IsDottedQuad(hostname))
            possible_hosts.insert(GetSuggestedWwwHost(hostname));
    }

    // Get a set of candidate paths:
    const std::string path(url.getPath());
    possible_paths.insert(path);

    // Microsoft path hack (attempt to lowercase path if server seems to be on an M$ OS):
    std::string lowercase_path = StringUtil::ToLower(path);
    const bool lowercase_is_different(path != lowercase_path);
    if (lowercase_is_different) {
        UppercaseHexEscapes(&lowercase_path);
        possible_paths.insert(lowercase_path);
    }

    // Attempt to trim off default suffixes.
    const unsigned trim_length = IdentifyPotentiallyUnnecessarySuffixes(path);
    if (trim_length != 0) {
        possible_paths.insert(path.substr(0, path.length() - trim_length));
        if (lowercase_is_different)
            possible_paths.insert(lowercase_path.substr(0, lowercase_path.length() - trim_length));
    }

    // Check for trailing slashes
    if (path[path.length() - 1 - trim_length] == '/') {
        possible_paths.insert(path.substr(0, path.length() - 1 - trim_length));
        if (lowercase_is_different)
            possible_paths.insert(lowercase_path.substr(0, lowercase_path.length() - 1 - trim_length));
    }

    // Created sorted vectors of hostnames and paths.
    std::vector<std::string> sorted_hosts(possible_hosts.size());
    std::copy(possible_hosts.begin(), possible_hosts.end(), sorted_hosts.begin());
    std::sort(sorted_hosts.begin(), sorted_hosts.end(), HostLessThan);
    std::vector<std::string> sorted_paths(possible_paths.size());
    std::copy(possible_paths.begin(), possible_paths.end(), sorted_paths.begin());
    std::sort(sorted_paths.begin(), sorted_paths.end(), RestLessThan);

    // Make a list of every combination of hostname and path:
    for (std::vector<std::string>::const_iterator host(sorted_hosts.begin());
         host != sorted_hosts.end(); ++host)
        {
            for (std::vector<std::string>::const_iterator path_iter(sorted_paths.begin());
                 path_iter != sorted_paths.end(); ++path_iter)
                potential_urls->push_back(Url::MakeUrl(scheme, url.getUsernamePassword(), *host,
                                                       url.getPortAsString(), *path_iter, url.getParams(),
                                                       url.getQuery(), url.getFragment()));
        }

    // Make sure the original URL is in the list:
    std::list<std::string>::iterator pos = std::find(potential_urls->begin(),
                                                     potential_urls->end(), url.toString());
    if (pos == potential_urls->end())
        potential_urls->push_back(url.toString());
    else if (++pos != potential_urls->end())
        potential_urls->erase(pos, potential_urls->end());
    return true;
}


// Url::SuggestLikelyAlternateFormsForWebUrl -- Suggest simple, common variants on a URL.
//
unsigned Url::SuggestLikelyAlternateFormsForWebUrl(const std::string &url, std::list<std::string> * const url_list,
                                                   const bool canonize_url)
{
    url_list->clear();

    Url the_url(url);

    // Ensure we're dealing with a valid web URL:
    if (not the_url.isValidWebUrl())
        throw std::runtime_error("in Url::SuggestLikelyAlternateFormsForWebUrl: not a valid Web URL: " + url);

    // Start by making sure the URL is Canonized
    if (canonize_url)
        the_url.makeCanonical();
    url_list->push_back(the_url);

    // Create a variant based on absense/presence of "www" subdomain
    std::string new_url(the_url);
    const std::string::size_type pos(new_url.find("//www."));

    // If there is no www part in the URL, create a variant that has one.
    if (pos == std::string::npos) {
        if (new_url.length() > 7 and new_url.substr(0,7) == "http://")
            new_url = "http://www." + new_url.substr(7);
        else if (new_url.length() > 8 and new_url.substr(0,8) == "https://")
            new_url = "https://www." + new_url.substr(8);
    }

    // If there is a www part, create a variane without one
    else if (new_url.length() > pos + 6)
        new_url = new_url.substr(0, pos + 2) + new_url.substr(pos + 6);

    url_list->push_back(new_url);
    return url_list->size();
}


bool operator==(const Url &lhs, const Url &rhs) {
    if (not lhs.isValid() or not rhs.isValid())
        return false;
    if (not lhs.relative_url_.empty() or not rhs.relative_url_.empty())
        return lhs.relative_url_ == rhs.relative_url_;

    return lhs.getScheme() == rhs.getScheme()
           and lhs.getUsernamePassword() == rhs.getUsernamePassword()
           and lhs.getAuthority() == rhs.getAuthority()
           and lhs.getPortAsString(true) == rhs.getPortAsString(true)
           and lhs.getPath() == rhs.getPath()
           and lhs.getParams() == rhs.getParams()
           and lhs.getQuery() == rhs.getQuery()
           and lhs.getFragment() == rhs.getFragment();
}
