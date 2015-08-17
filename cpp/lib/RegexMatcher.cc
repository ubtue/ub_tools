/** \file   RegexMatcher.cc
 *  \brief  Implementation of the RegexMatcher class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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
#include "util.h"


bool RegexMatcher::utf8_configured_;


bool CompileRegex(const std::string &pattern, const bool enable_utf8, ::pcre **pcre, ::pcre_extra **pcre_extra,
                  std::string * const err_msg)
{
    if (err_msg != nullptr)
        err_msg->clear();

    const char *errptr;
    int erroffset;
    *pcre = ::pcre_compile(pattern.c_str(), enable_utf8 ? PCRE_UTF8 : 0, &errptr, &erroffset, nullptr);
    if (*pcre == nullptr) {
        *pcre = nullptr;
        *pcre_extra = nullptr;
        if (err_msg != nullptr)
            *err_msg = "failed to compile invalid regular expression: \"" + pattern + "\"! ("
                + std::string(errptr) + ")";
        return false;
    }

    *pcre_extra = ::pcre_study(*pcre, 0, &errptr); // Can't use PCRE_STUDY_JIT_COMPILE because it's not thread safe.
    if (*pcre_extra == nullptr and errptr != nullptr) {
        ::pcre_free(*pcre);
        *pcre = nullptr;
        *pcre_extra = nullptr;
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

    ::pcre *pcre;
    ::pcre_extra *pcre_extra;
    if (not CompileRegex(pattern, enable_utf8, &pcre, &pcre_extra, err_msg))
        return nullptr;

    return new RegexMatcher(pattern, enable_utf8, pcre, pcre_extra);
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
            Error("In RegexMatcher copy constructor: unexpected error: " + err_msg);
    }
}


RegexMatcher::RegexMatcher(RegexMatcher &&that)
    : pattern_(std::move(that.pattern_)), utf8_enabled_(that.utf8_enabled_), pcre_(that.pcre_),
      pcre_extra_(that.pcre_extra_)
{
    that.pcre_       = nullptr;
    that.pcre_extra_ = nullptr;
}


bool RegexMatcher::matched(const std::string &s, std::string * const err_msg,
                           size_t * const start_pos, size_t * const end_pos) const
{
    err_msg->clear();

    const int substr_vector_size(60); // must be a multiple of 3
    int substr_vector[substr_vector_size];
    const int retcode = ::pcre_exec(pcre_, pcre_extra_, s.data(), s.length(), 0, 0,
                                    substr_vector, substr_vector_size);
    if (retcode > 0) {
        if (start_pos != nullptr)
            *start_pos = substr_vector[0];
        if (end_pos != nullptr)
            *end_pos = substr_vector[1];
        return true;
    }

    if (retcode != PCRE_ERROR_NOMATCH) {
        if (retcode == PCRE_ERROR_BADUTF8)
            *err_msg = "A \"subject\" with invalid UTF-8 was passed into RegexMatcher::matched()!";
        else
            *err_msg = "Unknown error!";
    }

    return false;
}
