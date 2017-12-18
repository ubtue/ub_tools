/** \file   RegexMatcher.cc
 *  \brief  Implementation of the RegexMatcher class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015,2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "RegexMatcher.h"
#include "Compiler.h"
#include "util.h"


bool RegexMatcher::utf8_configured_;


bool CompileRegex(const std::string &pattern, const bool enable_utf8, ::pcre **pcre_arg,
                  ::pcre_extra **pcre_extra_arg, std::string * const err_msg)
{
    if (err_msg != nullptr)
        err_msg->clear();

    const char *errptr;
    int erroffset;
    *pcre_arg = ::pcre_compile(pattern.c_str(), enable_utf8 ? PCRE_UTF8 : 0, &errptr, &erroffset, nullptr);
    if (*pcre_arg == nullptr) {
        *pcre_extra_arg = nullptr;
        if (err_msg != nullptr)
            *err_msg = "failed to compile invalid regular expression: \"" + pattern + "\"! ("
                + std::string(errptr) + ")";
        return false;
    }

    // Can't use PCRE_STUDY_JIT_COMPILE because it's not thread safe.
    *pcre_extra_arg = ::pcre_study(*pcre_arg, 0, &errptr);
    if (*pcre_extra_arg == nullptr and errptr != nullptr) {
        ::pcre_free(*pcre_arg);
        *pcre_arg = nullptr;
        if (err_msg != nullptr)
            *err_msg = "failed to \"study\" the compiled pattern \"" + pattern + "\"! (" + std::string(errptr) + ")";
        return false;
    }

    return true;
}


RegexMatcher *RegexMatcher::RegexMatcherFactory(const std::string &pattern, std::string * const err_msg,
                                                const bool enable_utf8)
{
    // Make sure the PCRE library supports UTF8:
    if (enable_utf8 and not RegexMatcher::utf8_configured_) {
        int utf8_available;
        if (::pcre_config(PCRE_CONFIG_UTF8, reinterpret_cast<void *>(&utf8_available)) == PCRE_ERROR_BADOPTION) {
            if (err_msg != nullptr)
                *err_msg = "PCRE library does not know PCRE_CONFIG_UTF8!";
            return nullptr;
        }

        if (utf8_available != 1) {
            if (err_msg != nullptr)
                *err_msg = "This version of the PCRE library does not support UTF8!";
            return nullptr;
        }

        RegexMatcher::utf8_configured_ = true;
    }

    ::pcre *pcre_ptr;
    ::pcre_extra *pcre_extra_ptr;
    if (not CompileRegex(pattern, enable_utf8, &pcre_ptr, &pcre_extra_ptr, err_msg))
        return nullptr;

    return new RegexMatcher(pattern, enable_utf8, pcre_ptr, pcre_extra_ptr);
}


RegexMatcher::RegexMatcher(const RegexMatcher &that): pattern_(that.pattern_) {
    if (this == &that)
        return;

    if (that.pcre_ == nullptr) {
        pcre_ = nullptr;
        pcre_extra_ = nullptr;
    } else {
        std::string err_msg;
        if (not CompileRegex(pattern_, that.utf8_enabled_, &pcre_, &pcre_extra_, &err_msg))
            logger->error("In RegexMatcher copy constructor: unexpected error: " + err_msg);
        substr_vector_    = that.substr_vector_;
        last_match_count_ = that.last_match_count_;
    }
}


RegexMatcher::RegexMatcher(RegexMatcher &&that)
    : pattern_(std::move(that.pattern_)), utf8_enabled_(that.utf8_enabled_), pcre_(that.pcre_),
      pcre_extra_(that.pcre_extra_), last_subject_(std::move(that.last_subject_)),
      substr_vector_(std::move(that.substr_vector_)), last_match_count_(that.last_match_count_)
{
    that.pcre_       = nullptr;
    that.pcre_extra_ = nullptr;
}


bool RegexMatcher::matched(const std::string &subject, std::string * const err_msg,
                           size_t * const start_pos, size_t * const end_pos) const
{
    if (err_msg != nullptr)
        err_msg->clear();

    const int retcode = ::pcre_exec(pcre_, pcre_extra_, subject.data(), subject.length(), 0, 0,
                                    &substr_vector_[0], substr_vector_.size());

    if (retcode == 0) {
        if (err_msg != nullptr)
            *err_msg = "Too many captured substrings! (We only support "
                       + std::to_string(substr_vector_.size() / 3 - 1) + " substrings.)";
        return false;
    }

    if (retcode > 0) {
        last_match_count_ = retcode;
        last_subject_     = subject;
        if (start_pos != nullptr)
            *start_pos = substr_vector_[0];
        if (end_pos != nullptr)
            *end_pos = substr_vector_[1];
        return true;
    }

    if (retcode != PCRE_ERROR_NOMATCH) {
        if (retcode == PCRE_ERROR_BADUTF8) {
            if (err_msg != nullptr)
                *err_msg = "A \"subject\" with invalid UTF-8 was passed into RegexMatcher::matched()!";
        } else if (err_msg != nullptr)
            *err_msg = "Unknown error!";
    }

    return false;
}


std::string RegexMatcher::operator[](const unsigned group) const {
    if (unlikely(group >= last_match_count_))
        throw std::out_of_range("in RegexMatcher::operator[]: group(" + std::to_string(group) + ") >= "
                                + std::to_string(last_match_count_) + "!");

    const unsigned first_index(group * 2);
    const unsigned substring_length(substr_vector_[first_index + 1] - substr_vector_[first_index]);
    return (substring_length == 0) ? "" : last_subject_.substr(substr_vector_[first_index], substring_length);
}
