/** \file    UrlUtil.h
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
#ifndef URL_UTIL_H
#define URL_UTIL_H


#include <string>
#include "PerlCompatRegExp.h"
#include "SList.h"
#include "Url.h"


namespace UrlUtil {


/** \brief   Turns a Privoxy pattern into a Perl regexp pattern.
 *  \param   privoxy_pattern  The Privoxy pattern that we want to convert.
 *  \return  An equivalent Perl-compatible regexp.
 *
 *  \note    The following has been taken from the Privoxy manual:
 *
 *
 *                               --- Patterns ---
 *
 *                               Generally, a pattern has the form &lt;domain>/&lt;path>, where both the &lt;domain>
 *                               and &lt;path>  are optional. (This is why the pattern / matches all URLs).
 *
 *                               www.example.com/
 *
 *                               is a domain-only pattern and will match any request to www.example.com,
 *                               regardless of which document on that server is requested.
 *
 *                               www.example.com
 *
 *                               means exactly the same. For domain-only patterns, the trailing / may be
 *                               omitted.
 *
 *                               www.example.com/index.html
 *
 *                               matches only the single document /index.html on www.example.com.
 *
 *                               /index.html
 *
 *                               matches the document /index.html, regardless of the domain, i.e. on any
 *                               Web server.
 *
 *                               index.html
 *
 *                               matches nothing, since it would be interpreted as a domain name and there
 *                               is no top-level domain called .html.
 *
 *
 *                               --- Domain Patterns ---
 *
 *                               The matching of the domain part offers some flexible options: if the domain
 *                               starts or ends with a dot, it becomes unanchored at that end. For example:
 *
 *                               .example.com
 *
 *                               matches any domain that ENDS in .example.com
 *
 *                               www.
 *
 *                               matches any domain that STARTS with www.
 *
 *                               .example.
 *
 *                               matches any domain that CONTAINS .example. (Correctly speaking: It matches
 *                               any FQDN that contains example as a domain.)
 *
 *                               Additionally, there are wild-cards that you can use in the domain names
 *                               themselves. They work pretty similar to shell wild-cards: "*" stands for zero
 *                               or more arbitrary characters, "?" stands for any single character, you can
 *                               define character classes in square brackets and all of that can be freely
 *                               mixed:
 *
 *                               ad*.example.com
 *
 *                               matches "adserver.example.com", "ads.example.com", etc but not
 *                               "sfads.example.com"
 *
 *                               *ad*.example.com
 *
 *                               matches all of the above, and then some.
 *
 *                               .?pix.com
 *
 *                               matches www.ipix.com, pictures.epix.com, a.b.c.d.e.upix.com etc.
 *
 *                               www[1-9a-ez].example.c*
 *
 *                               matches www1.example.com, www4.example.cc, wwwd.example.cy, wwwz.example.com
 *                               etc., but not wwww.example.com
 */
std::string PrivoxyToPerlRegExpPattern(const std::string &privoxy_pattern);


/**  \brief   Tests whether "test_pattern" is a valid Privoxy regexp or not.
 *   \param   test_pattern  The string we want to test for validity as a Privoxy regexp.
 *   \return  'True' if "test_pattern" is a valid Privoxy regexp, otherwise 'false'.
 */
bool IsValidPrivoxyPattern(const std::string &test_pattern);


/** \brief  Implement Privoxy-style URL blacklists.
 */
class Blacklister {
    class ReferenceCountedPerlCompatRegExp {
        std::string *pattern_;
        PerlCompatRegExp *reg_exp_;
        unsigned *count_;
        bool block_;
    public:
        ReferenceCountedPerlCompatRegExp(const std::string &pattern, const bool initial_block);
        ReferenceCountedPerlCompatRegExp(const ReferenceCountedPerlCompatRegExp &rhs)
            : pattern_(rhs.pattern_), reg_exp_(rhs.reg_exp_), count_(rhs.count_), block_(rhs.block_)
        { ++*count_; }
        const ReferenceCountedPerlCompatRegExp &operator=(const ReferenceCountedPerlCompatRegExp &rhs);
        ~ReferenceCountedPerlCompatRegExp();
        const std::string &getPattern() const { return *pattern_; }
        bool match(const std::string &s) const { return reg_exp_->match(s); }
        bool block() const { return block_; }
    private:
        ReferenceCountedPerlCompatRegExp(); // Intentionally unimplemented!
    };


    class UrlRegExpList {
        std::list<ReferenceCountedPerlCompatRegExp> ref_counted_reg_exps_;
    public:
        bool block(const std::string &url, std::string * const reason) const;
        void push_back(const ReferenceCountedPerlCompatRegExp &new_ref_counted_reg_exp)
        { ref_counted_reg_exps_.push_back(new_ref_counted_reg_exp); }
    } reg_exps_;
public:
    /** \brief  Creates a Blacklister object.
     *  \param  config_filename  The name of a config file (IniFile) containing a lists of entries pointing to
     *                           Privoxy pattern files or an actual Privoxy pattern file.
     *  \param  indirect         If "true" "config_filename" points to an IniFile that has entries referencing
     *                           Privoxy pattern files.  If "false" "config_filename" is the name of an actual
     *                           Privoxy pattern file.
     */
    explicit Blacklister(const std::string &config_filename, const bool indirect = true);

    /** \brief  Creates a Blacklister object.
     *  \param  privoxy_patterns  A list of Privoxy patterns.
     */
    explicit Blacklister(const std::list<std::string> &privoxy_patterns = std::list<std::string>());

    /** \brief   Is this URL blocked by the blacklist?
     *  \param   url     The URL to test.
     *  \param   reason  If non-nullptr, returns the pattern that was the reason that the URL was blocked.
     *  \return  True if the URL is an 'http' or 'https' URL and is blocked; otherwise false.
     */
    bool blocked(const std::string &url, std::string * const reason = nullptr) const;
private:
    /** Extracts blocking and unblocking info from "action_filename" and stores it in "reg_exps_". */
    void processPrivoxyActionFile(const std::string &action_filename);
};


/* Return the "%XX" sequence (with two hex digits) corresponding to "ch". */
std::string UrlEncodeChar(const char ch);


/** Replace all characters that have a special meaning in a URL with %XX where "XX" is a two character hex encoding. */
std::string UrlEncode(std::string * const s);


/** Replace all characters that have a special meaning in a URL with %XX where "XX" is a two character hex encoding. */
inline std::string UrlEncode(const std::string &s)
{
        std::string mutable_s(s);
        return UrlEncode(&mutable_s);
}


/** Decode all "%XX" hex encodings. */
std::string &UrlDecode(std::string * const s);


/** Decode all "%XX" hex encodings. */
std::string UrlDecode(const std::string &s);


/** \fn     UrlCompare(const Url &lhs, const Url &rhs)
 *  \brief  Compares "lhs" and "rhs" in a case-idependent manner, skipping over leading schemes and if the scheme is
 *          "http" or "https" we skip over "www." if it is the first part of the domainname.
 *  \param  lhs 1st URL to compare.
 *  \param  rhs 2nd URL to compare.
 *  \return An int analogous to strcmp(3).
 */
int UrlCompare(const Url &lhs, const Url &rhs);


/** \fn     UrlCompare(const char * const lhs, const char * const rhs)
 *  \brief  See UrlCompare(const Url &lhs, const Url &rhs)
 */
inline int UrlCompare(const char * const lhs, const char * const rhs) { return UrlCompare(Url(lhs), Url(rhs)); }


/** \fn     UrlCompare(const std:: string &lhs, const std::string &rhs)
 *  \brief  See UrlCompare(const Url &lhs, const Url &rhs)
 */
inline int UrlCompare(const std::string &lhs, const std::string &rhs) { return UrlCompare(Url(lhs), Url(rhs)); }


/** \brief   Splits a URL into its component parts:
 *           \<scheme\>://\<net_loc\>/\<path\>;\<params\>?\<query\>#\<fragment\>.
 *  \param   url                The URL to split apart.
 *  \param   scheme             Whatever comes before "://" (hopefully "http" or something similar).
 *  \param   username_password  Whatever comes between "://" and "\@hostname" if anything.
 *  \param   authority          Typically a hostname, e.g. www.cyber-dyne.com.
 *  \param   port               Whatever was found between "...hostname:" and a slash or if no slash follows, the end of
 *                              the string.
 *  \param   path               The optional path following the hostname or port.
 *  \param   params             An optional parameter part.
 *  \param   query              An optional query part (only used by HTTP or HTTPS).
 *  \param   fragment           An optional fragment part (only used by HTTP or HTTPS).
 *  \param   relative_url       The relative URL computed by the function.
 *  \return  True if it found the URL to start off with "protocol://" followed by arbitrary stuff, else false.
 *  \note    In the case of "local_url://", "local_url" is returned as the protocol and everything after the "://" is
 *           returned in "authority".'
 */
bool ParseUrl(const std::string &url, std::string * const scheme, std::string * const username_password,
              std::string * const authority, std::string * const port, std::string * const path,
              std::string * const params, std::string * const query, std::string * const fragment,
              std::string * const relative_url);


/** \brief  Converts a URL to a string that can be reasonably expected to act as a key that is likely to avoid some URL
 *          aliasing issues.
 *
 *  The strategy used is as follows:
 *  1) Lowercases the URL.
 *  2) Strips off fragments.
 *  3) Strips off a trailing index.html, index.html or index.shtml.
 *  4) Strips off a trailing slash unless the key remaining at that point is a single slash.
 *  5) Replaces all %XX encoded characters with their corresponding character codes.
 *
 *  \param  url  The URL to turn into a key.
 *  \return The key.
 *  \note   The returned value is not necessarily a valid URL and cannot be expected to always refer to the same page
 *          as the original URL even if it is a valid URL.
 */
std::string CanoniseUrlAsKey(const std::string &url);


bool IsRobotsDotTxtUrl(const Url &test_url);


inline bool IsValidWebUrl(const std::string &url) { return Url(url).isValidWebUrl(); }


} // namespace UrlUtil


#endif // ifndef URL_UTIL_H
