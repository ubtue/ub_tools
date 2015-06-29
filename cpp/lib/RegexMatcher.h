/** \file   RegexMatcher.h
 *  \brief  Interface for the RegexMatcher class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014 Universitätsbiblothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef REGEX_MATCHER_H
#define REGEX_MATCHER_H


#include <string>
#include <pcre.h>


/** \class RegexMatcher
 *  \brief Wrapper class for simple use cases of the PCRE library and UTF-8 strings.
 */
class RegexMatcher {
    static bool utf8_configured_;
    std::string pattern_;
    bool utf8_enabled_;
    pcre *pcre_;
    pcre_extra* pcre_extra_;
public:
    /** Copy constructor. */
    RegexMatcher(const RegexMatcher &that);

    /** Move constructor. */
    RegexMatcher(RegexMatcher &&that);

    /** Destrcutor. */
    virtual ~RegexMatcher() {
        ::pcre_free_study(pcre_extra_);
        ::pcre_free(pcre_);
    }

    /** \brief Creates a RegexMatcher.
     *  \param pattern      The pattern to be compiled.
     *  \param err_msg      If non-NULL, an explanation of a possible error will be written here.
     *  \param enable_utf8  If true, support matching of UTF8 text.
     *  \return NULL if "pattern" failed to compile and then also sets "err_msg".
     */
    static RegexMatcher *RegexMatcherFactory(const std::string &pattern, std::string * const err_msg = NULL,
                                             const bool enable_utf8 = false);

    /** Returns true if "s" was matched, false, if an error occurred or no match was found. In the case of an
     *  error "err_msg" will be set to a non-empty string, otherwise "err_msg" will be cleared.
     *  In the case of a successful match, "start_pos" and "end_pos" will point to the first and last+1
     *  character of the matched part of "s" respectively.
     */
    bool matched(const std::string &s, std::string * const err_msg,
                 size_t * const start_pos = NULL, size_t * const end_pos = NULL) const;

    const std::string &getPattern() const { return pattern_; }
    bool utf8Enabled() const { return utf8_enabled_; }
private:
    RegexMatcher(const std::string &pattern, const bool utf8_enabled, pcre * const pcre,
                 pcre_extra * const pcre_extra)
        : pattern_(pattern), utf8_enabled_(utf8_enabled), pcre_(pcre), pcre_extra_(pcre_extra) {}
};


#endif // ifndef REGEX_MATCHER_H
