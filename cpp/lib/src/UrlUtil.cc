/** \file    UrlUtil.cc
 *  \brief   URL-related utility functions and classes.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2004-2008 Project iVia.
 *  Copyright 2004-2008 The Regents of The University of California.
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

#include "UrlUtil.h"
#include <fstream>
#include <stdexcept>
#include <cassert>
#include "Compiler.h"
#include "IniFile.h"
#include "MiscUtil.h"
#include "StringUtil.h"
#include "Url.h"


namespace UrlUtil {


namespace { // Helper functions for the Blacklister class.


std::string PrivoxyDomainToPerlRegExp(const std::string &privoxy_domain_pattern) {
    if (privoxy_domain_pattern.empty())
        return ".*";

    std::string perl_domain_pattern;
    perl_domain_pattern.reserve(privoxy_domain_pattern.size());
    bool in_character_class = false;
    const unsigned SOURCE_LENGTH(privoxy_domain_pattern.length());
    for (unsigned pos = 0; pos < SOURCE_LENGTH; ++pos) {
        if (pos == 0 and privoxy_domain_pattern[pos] == '.')
            perl_domain_pattern += "(?:.*\\.)?";
        else if (pos == SOURCE_LENGTH - 1 and privoxy_domain_pattern[pos] == '.')
            perl_domain_pattern += "\\..*";
        else if (in_character_class) {
            perl_domain_pattern += privoxy_domain_pattern[pos];
            if (privoxy_domain_pattern[pos] == ']')
                in_character_class = false;
        }
        else if (privoxy_domain_pattern[pos] == '[') {
            in_character_class = true;
            perl_domain_pattern += '[';
        }
        else if (privoxy_domain_pattern[pos] == '.')
            perl_domain_pattern += "\\.";
        else if (privoxy_domain_pattern[pos] == '?')
            perl_domain_pattern += ".?";
        else if (privoxy_domain_pattern[pos] == '*')
            perl_domain_pattern += ".*";
        else
            perl_domain_pattern += privoxy_domain_pattern[pos];
    }

    return perl_domain_pattern;
}


} // unnamed namespace


// PrivoxyToPerlRegExpPattern -- turns a Privoxy pattern into a Perl regexp pattern.
//
std::string PrivoxyToPerlRegExpPattern(const std::string &privoxy_pattern) {
    if (unlikely(privoxy_pattern.empty()))
        throw std::runtime_error("in UrlUtil::PrivoxyToPerlRegExpPattern: \"privoxy_pattern\" must not be empty!");

    std::string domain, path;
    std::string::size_type slash_pos = privoxy_pattern.find('/');
    if (slash_pos == std::string::npos)
        domain = privoxy_pattern;
    else {
        domain = privoxy_pattern.substr(0, slash_pos);
        path   = privoxy_pattern.substr(slash_pos);
    }

    if (unlikely(not(path.empty() or path[0] == '/')))
        throw std::runtime_error("in UrlUtil::PrivoxyToPerlRegExpPattern: \"path\" must be empty or absolute!");

    return PrivoxyDomainToPerlRegExp(domain) + path;
}


bool IsValidPrivoxyPattern(const std::string &test_pattern) {
    try {
        const std::string perl_regexp(PrivoxyToPerlRegExpPattern(test_pattern));
        return PerlCompatRegExp::IsValid(perl_regexp);
    } catch (...) {
        return false;
    }
}


bool Blacklister::UrlRegExpList::block(const std::string &url, std::string * const reason) const {
    for (std::list<ReferenceCountedPerlCompatRegExp>::const_reverse_iterator reg_exp(ref_counted_reg_exps_.rbegin());
         reg_exp != ref_counted_reg_exps_.rend(); ++reg_exp)
    {
        if (reg_exp->match(url)) {
            if (reason != nullptr)
                *reason = reg_exp->getPattern();
            return reg_exp->block();
        }
    }

    if (reason != nullptr)
        reason->clear();

    return false;
}


Blacklister::ReferenceCountedPerlCompatRegExp::ReferenceCountedPerlCompatRegExp(const std::string &pattern,
                                                                                const bool initial_block)
    : pattern_(new std::string(PrivoxyToPerlRegExpPattern(pattern))),
      reg_exp_(new PerlCompatRegExp(*pattern_, PerlCompatRegExp::OPTIMIZE_FOR_MULTIPLE_USE, PCRE_ANCHORED)),
      count_(new unsigned(1)), block_(initial_block)
{
}


const Blacklister::ReferenceCountedPerlCompatRegExp &Blacklister::ReferenceCountedPerlCompatRegExp::operator=(
    const ReferenceCountedPerlCompatRegExp &rhs)
{
    // Avoid self-assignment:
    if (this != &rhs) {
        this->~ReferenceCountedPerlCompatRegExp();

        count_ = rhs.count_;
        ++*count_;
        pattern_ = rhs.pattern_;
        reg_exp_ = rhs.reg_exp_;
        block_   = rhs.block_;
    }

    return *this;
}


Blacklister::ReferenceCountedPerlCompatRegExp::~ReferenceCountedPerlCompatRegExp() {
    --*count_;
    if (*count_ == 0) {
        delete count_;
        delete pattern_;
        delete reg_exp_;
    }
}


Blacklister::Blacklister(const std::string &config_filename, const bool indirect) {
    if (indirect) { // We're dealing with an IniFile referening one or more Privoxy pattern files.
        IniFile ini_file(config_filename);
        std::list<std::string> section_entry_names = ini_file.getSectionEntryNames("");
        for (std::list<std::string>::const_iterator section_entry_name(section_entry_names.begin());
             section_entry_name != section_entry_names.end(); ++section_entry_name)
        {
            std::string filename = ini_file.getString("", *section_entry_name);
            if (filename[0] != '/')
                filename = std::string(ETC_DIR) + "/" + filename;
            processPrivoxyActionFile(filename);
        }
    } else // We're dealing with an actual Privoxy pattern file.
        processPrivoxyActionFile(config_filename);
}


Blacklister::Blacklister(const std::list<std::string> &privoxy_patterns) {
    for (const auto &privoxy_pattern : privoxy_patterns)
        reg_exps_.push_back(ReferenceCountedPerlCompatRegExp(privoxy_pattern, /* initial_block = */ true));
}


bool Blacklister::blocked(const std::string &url, std::string * const reason) const {
    const char * const url_as_c_string(url.c_str());
    bool scheme_is_http;
    if (::strncmp(url_as_c_string, "http://", 7) == 0)
        scheme_is_http = true;
    else if (::strncmp(url_as_c_string, "https://", 8) == 0)
        scheme_is_http = false;
    else {
        if (reason != nullptr)
            *reason = "Not a valid HTTP or HTTPS URL!";
        return false; // Not a valid HTTP or HTTPS URL!
    }

    // Check the URL against the blacklists:
    if (scheme_is_http)
        return reg_exps_.block(url_as_c_string + 7, reason);
    // The URL starts with "https":
    return reg_exps_.block(url_as_c_string + 8, reason);
}


void Blacklister::processPrivoxyActionFile(const std::string &action_filename) {
    std::ifstream input(action_filename.c_str());
    if (input.fail())
        throw std::runtime_error("in Blacklister::processPrivoxyActionFile: can't open \"" + action_filename
                                 + "\" for reading!");

    enum { BLOCK, UNBLOCK, SOMETHING_ELSE } state = SOMETHING_ELSE;
    while (input) {
        std::string line;
        std::getline(input, line);

        // Strip out comments:
        std::string::size_type hash_pos = line.find('#');
        if (hash_pos != std::string::npos)
            line = line.substr(0, hash_pos);

        StringUtil::Trim(&line);

        if (line.empty())
            continue;

        if (line[0] == '{') {
            if (line.substr(1, 6) == "+block")
                state = BLOCK;
            else if (line.substr(1, 6) == "-block")
                state = UNBLOCK;
            else
                state = SOMETHING_ELSE;
            continue;
        }

        if (state == SOMETHING_ELSE)
            continue;

        reg_exps_.push_back(ReferenceCountedPerlCompatRegExp(line, state == BLOCK));
    }
}


// UrlEncodeChar -- encode "ch" into "%XX" where XX are two hex digits
//
std::string UrlEncodeChar(const char ch) {
    std::string encoded_char("%");
    encoded_char += MiscUtil::HexDigit(static_cast<const unsigned char>(ch) >> 4u);
    encoded_char += MiscUtil::HexDigit(static_cast<const unsigned char>(ch) & 0xFu);

    return encoded_char;
}


std::string UrlEncode(std::string * const s) {
    std::string result;
    result.reserve(2 * s->length());
    for (std::string::const_iterator ch(s->begin()); ch != s->end(); ++ch) {
        if (StringUtil::IsAlphanumeric(*ch) or *ch == '-' or *ch == '_' or *ch == '.'
            or *ch == '!' or *ch == '~' or *ch == '*' or *ch == '\'' or *ch == '(' or *ch == ')')
            result += *ch;
        else
            result += UrlEncodeChar(*ch);
    }

    return *s = result;
}


namespace {


inline unsigned HexValue(char ch) {
    switch (ch) {
    case '0':
        return 0;
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    case '9':
        return 9;
    case 'A':
    case 'a':
        return 10;
    case 'B':
    case 'b':
        return 11;
    case 'C':
    case 'c':
        return 12;
    case 'D':
    case 'd':
        return 13;
    case 'E':
    case 'e':
        return 14;
    case 'F':
    case 'f':
        return 15;
    default:
        return 0;
    }
}


} // unnamed namespace


std::string &UrlDecode(std::string * const s) {
    std::string result;
    for (std::string::const_iterator ch(s->begin()); ch != s->end(); ++ch) {
        if (*ch == '+')
            result += ' ';
        else if (*ch != '%')
            result += *ch;
        else { // assume hex encoded character
            unsigned value = 0;
            ++ch;
            if (ch == s->end())
                break;
            value += HexValue(*ch);
            ++ch;
            if (ch == s->end())
                break;
            value <<= 4;
            value += HexValue(*ch);
            result += static_cast<char>(value);
        }
    }

    return (*s = result);
}


std::string UrlDecode(const std::string &s) {
    std::string temp_s(s);
    return UrlDecode(&temp_s);
}


int UrlCompare(const Url &lhs, const Url &rhs) {
    const std::string lhs_scheme(lhs.getScheme());
    std::string lhs_as_string(lhs.toString());
    if (lhs_scheme == "http" or lhs_scheme == "https" or lhs_scheme == "ftp")
        lhs_as_string = lhs_as_string.substr(lhs_scheme.length() + 3); // Skip over the scheme and "://".
    else if (not lhs_as_string.empty())
        lhs_as_string = lhs_as_string.substr(lhs_scheme.length() + 1); // Skip over the scheme and ":".
    if ((lhs_scheme == "http" or lhs_scheme == "https") and lhs_as_string.substr(0, 4) == "www.")
        lhs_as_string = lhs_as_string.substr(4);
    else if (lhs_scheme == "ftp" and lhs_as_string.substr(0, 4) == "ftp.")
        lhs_as_string = lhs_as_string.substr(4);

    const std::string rhs_scheme(rhs.getScheme());
    std::string rhs_as_string(rhs.toString());
    if (rhs_scheme == "http" or rhs_scheme == "https" or rhs_scheme == "ftp")
        rhs_as_string = rhs_as_string.substr(rhs_scheme.length() + 3); // Skip over the scheme and "://".
    else if (not rhs_as_string.empty())
        rhs_as_string = rhs_as_string.substr(rhs_scheme.length() + 1); // Skip over the scheme and ":".
    if ((rhs_scheme == "http" or rhs_scheme == "https") and rhs_as_string.substr(0, 4) == "www.")
        rhs_as_string = rhs_as_string.substr(4);
    else if (rhs_scheme == "ftp" and rhs_as_string.substr(0, 4) == "ftp.")
        rhs_as_string = rhs_as_string.substr(4);

    // Remove a possible trailing slash:
    if (not lhs_as_string.empty() and lhs_as_string[lhs_as_string.length() - 1] == '/')
        lhs_as_string.resize(lhs_as_string.length() - 1);

    // Remove a possible trailing slash:
    if (not rhs_as_string.empty() and rhs_as_string[rhs_as_string.length() - 1] == '/')
        rhs_as_string.resize(rhs_as_string.length() - 1);

    return ::strcasecmp(lhs_as_string.c_str(), rhs_as_string.c_str());
}


bool ParseUrl(const std::string &url, std::string * const scheme, std::string * const username_password,
              std::string * const authority, std::string * const port, std::string * const path,
              std::string * const params, std::string * const query, std::string * const fragment,
              std::string * const relative_url)
{
    scheme->clear();
    username_password->clear();
    authority->clear();
    port->clear();
    path->clear();
    params->clear();
    query->clear();
    fragment->clear();
    relative_url->clear();

    // Reject empty URLs:
    if (unlikely(url.empty()))
        return false;

    // If there is no scheme, store this as a relative_url and bail out:
    const std::string::size_type first_question_mark_pos(url.find('?'));
    std::string::size_type first_colon_pos = url.find(':');
    if (first_colon_pos == std::string::npos or first_colon_pos == 0
        or (first_question_mark_pos != std::string::npos and first_question_mark_pos < first_colon_pos))
    {
        *relative_url = url;
        return true;
    }

    // We have an absolute URL.
    *scheme = url.substr(0, first_colon_pos);

    std::string::size_type authority_start;
    std::string::size_type path_start;
    if (*scheme == "http" or *scheme == "https" or *scheme == "ftp" or *scheme == "gopher" or *scheme == "telnet") {
        if (url.length() < first_colon_pos + 4)
            return false;
        if (url[first_colon_pos + 1] != '/' or url[first_colon_pos + 2] != '/')
            return false;

        authority_start = first_colon_pos + 3;
        path_start = url.find('/', authority_start);
    } else if (*scheme == "news" or *scheme == "mailto" or *scheme == "file" or *scheme == "local_ucr") {
        if (url.length() < first_colon_pos + 1)
            return false;

        *authority = url.substr(first_colon_pos + 1);
        return true;
    } else // We do not recognise this scheme, so can go no further.
        return false;

    assert(authority_start != std::string::npos);

    // Don't let authority start with a '/' (e.g. "http:///yyy").
    if (url[authority_start] == '/')
        return false;

    // Read the username_password, authority and port:
    std::string::size_type first_at_sign = url.find('@', authority_start);
    std::string::size_type first_colon_after_authority;

    // Do we have a username/password combo?
    if (first_at_sign != std::string::npos and (path_start == std::string::npos or path_start > first_at_sign)) {
        *username_password = url.substr(authority_start, first_at_sign - authority_start);
        first_colon_after_authority = url.find(':', first_at_sign + 1);
        authority_start = first_at_sign + 1;
    } else
        first_colon_after_authority = url.find(':', authority_start);

    // Extract the hostname or IP address and the optional port number:
    if (first_colon_after_authority != std::string::npos
        and (path_start == std::string::npos or path_start > first_colon_after_authority))
    {
        // We have a port number!
        *authority = url.substr(authority_start, first_colon_after_authority - authority_start);
        if (path_start == std::string::npos)
            *port = url.substr(first_colon_after_authority + 1);
        else
            *port = url.substr(first_colon_after_authority + 1, path_start - first_colon_after_authority - 1);
    } else { // We don't have a port number!
        if (path_start == std::string::npos)
            *authority = url.substr(authority_start);
        else
            *authority = url.substr(authority_start, path_start - authority_start);
    }

    StringUtil::ToLower(authority);

    // If the path is empty, we're done:
    if (path_start == std::string::npos)
        return true;

    // If the path is defined, read it into path, query, params and fragment components:
    *path = url.substr(path_start);

    if (*scheme == "http" or *scheme == "https") {
        std::string::size_type questionmark_pos = path->find('?');
        if (questionmark_pos != std::string::npos) {
            *query = path->substr(questionmark_pos + 1);
            *path  = path->substr(0, questionmark_pos);

            // Check for a fragment on the tail end of our query:
            std::string::size_type hash_pos = query->find('#');
            if (hash_pos != std::string::npos) {
                *fragment = query->substr(hash_pos + 1);
                *query    = query->substr(0, hash_pos);
            }
        } else { // Check for a fragment on the tail end of our path:
            std::string::size_type hash_pos = path->find('#');
            if (hash_pos != std::string::npos) {
                *fragment = path->substr(hash_pos + 1);
                *path     = path->substr(0, hash_pos);
            }
        }
    } else if (*scheme == "ftp") {
        std::string::size_type semicolon_pos = path->find(';');
        if (unlikely(semicolon_pos != std::string::npos)) {
            *params = path->substr(semicolon_pos + 1);
            *path   = path->substr(0, semicolon_pos);
        }
    } else if (*scheme == "telnet") {
        if (*path == "/")
            path->clear();
        return path->empty();
    }

    return true;
}


std::string CanoniseUrlAsKey(const std::string &url) {
    std::string key(StringUtil::ToLower(url));

    // Deal with fragments:
    const std::string::size_type hash_pos(key.find('#'));
    if (hash_pos != std::string::npos)
        key = key.substr(0, hash_pos);

    if (unlikely(key.empty()))
        return key;

    UrlDecode(&key);

    // Remove a common index file name suffix, if present:
    if (StringUtil::IsSuffixOf("/index.html", key))
        key = key.substr(0, key.length() - 10);
    else if (StringUtil::IsSuffixOf("/index.htm", key))
        key = key.substr(0, key.length() - 9);
    else if (StringUtil::IsSuffixOf("/index.shtml", key))
        key = key.substr(0, key.length() - 11);

    const std::string::size_type key_length(key.length());
    if (key_length <= 1)
        return key;

    if (key[key_length - 1] == '/')
        return key.substr(0, key_length - 1);
    else
        return key;
}


bool IsRobotsDotTxtUrl(const Url &test_url) {
    return test_url.isValid() and ::strcasecmp(test_url.getPath().c_str(), "/robots.txt") == 0;
}


} // namespace UrlUtil
