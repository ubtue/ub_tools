/** \file   RegexMatcher.h
 *  \brief  Interface for the RegexMatcher class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014,2015,2018 Universitätsbibliothek Tübingen.  All rights reserved.
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
#pragma once


#include <string>
#include <stdexcept>
#include <vector>
#include <pcre.h>


/** \class RegexMatcher
 *  \brief Wrapper class for simple use cases of the PCRE library and UTF-8 strings.
 */
class RegexMatcher {
    static bool utf8_configured_;
    std::string pattern_;
    unsigned options_;
    pcre *pcre_;
    pcre_extra* pcre_extra_;
    static constexpr size_t MAX_SUBSTRING_MATCHES = 20;
    mutable std::string last_subject_;
    mutable std::vector<int> substr_vector_;
    mutable unsigned last_match_count_;
public:
    enum Option { ENABLE_UTF8 = 1, CASE_INSENSITIVE = 2, MULTILINE = 4 }; // These need to be powers of 2.
public:
    /** Copy constructor. */
    RegexMatcher(const RegexMatcher &that);

    /** Move constructor. */
    RegexMatcher(RegexMatcher &&that);

    /** Destructor. */
    virtual ~RegexMatcher() {
        ::pcre_free_study(pcre_extra_);
        ::pcre_free(pcre_);
    }

    /** Returns true if "s" was matched, false, if an error occurred or no match was found. In the case of an
     *  error "err_msg", if provided, will be set to a non-empty string, otherwise "err_msg" will be cleared.
     *  In the case of a successful match, "start_pos" and "end_pos" will point to the first and last+1
     *  character of the matched part of "s" respectively.
     */
    bool matched(const std::string &subject, std::string * const err_msg = nullptr,
                 size_t * const start_pos = nullptr, size_t * const end_pos = nullptr);
    bool matched(const std::string &subject, const size_t subject_start_offset, std::string * const err_msg = nullptr,
                 size_t * const start_pos = nullptr, size_t * const end_pos = nullptr);

    /** \brief Replaces all matches of the pattern with the replacement string.
     */
    bool replaceAll(const std::string &subject, const std::string &replacement, std::string * const replaced_string,
                    std::string * const err_msg = nullptr);
    const std::string &getPattern() const { return pattern_; }
    bool utf8Enabled() const { return options_ & ENABLE_UTF8; }
    bool caseInsensitive() const { return options_ & CASE_INSENSITIVE; }

    /** \return The number of matched parenthesised groups in the pattern.
     *  \note   Obviously you may only call this after a call to matched().
     */
    unsigned getNoOfGroups() const { return last_match_count_ - 1; }

    /** \brief Returns either the full last match or matched substrings.
     *  \param group  When "group" is 0, the full last match will be returned, o/w the n-th substring match
     *                will be returned.
     *  \throws std::out_of_range_error when "group" is greater than the 1-based index of the last substring match.
     */
    std::string operator[](const unsigned group) const;

    /** \return The number of substring matches + 1 for the full match of the last match. */
    unsigned getLastMatchCount() const { return last_match_count_; }

    /** \brief Creates a RegexMatcher.
     *  \param pattern      The pattern to be compiled.
     *  \param err_msg      If non-nullptr, an explanation of a possible error will be written here.
     *  \param options      Or'ed together values of type enum Option.
     *  \return nullptr if "pattern" failed to compile and then also sets "err_msg".
     */
    static RegexMatcher *RegexMatcherFactory(const std::string &pattern, std::string * const err_msg = nullptr,
                                             const unsigned options = 0);

    /** \brief Creates a RegexMatcher.
     *  \param pattern      The pattern to be compiled.
     *  \param options      Or'ed together values of type enum Option.
     *  \note  Aborts on error and prints an error message to stderr.
     */
    static RegexMatcher *RegexMatcherFactoryOrDie(const std::string &regex, const unsigned options = 0);

    /** \brief Create a RegexMatcher and try to match the given regex.
     *  \param pattern     The pattern to use.
     *  \param subject     The subject to match.
     *  \param options     Or'ed together values of type enum Option.
     *  \param err_msg     If non-nullptr, an explanation of a possible error will be written here.
     *  \param start_pos   If match successfull, first character of the matched part of the subject.
     *  \param end_pos     If match successfull, last+1 character of the matched part of the subject.
     */
    static bool Matched(const std::string &regex, const std::string &subject, const unsigned options = 0,
                        std::string * const err_msg = nullptr, size_t * const start_pos = nullptr, size_t * const end_pos = nullptr);
private:
    RegexMatcher(const std::string &pattern, const unsigned options, pcre * const pcre_arg,
                 pcre_extra * const pcre_extra_arg)
        : pattern_(pattern), options_(options), pcre_(pcre_arg), pcre_extra_(pcre_extra_arg),
          substr_vector_((1 + MAX_SUBSTRING_MATCHES) * 3), last_match_count_(0) {}
};
