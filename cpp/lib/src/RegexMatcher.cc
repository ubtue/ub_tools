/** \file   RegexMatcher.cc
 *  \brief  Implementation of the RegexMatcher class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2021 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include <unordered_map>
#include "Compiler.h"
#include "StringUtil.h"
#include "util.h"


static bool CheckPCRE_UTF8Compatibility() {
    int utf8_available;
    if (::pcre_config(PCRE_CONFIG_UTF8, reinterpret_cast<void *>(&utf8_available)) == PCRE_ERROR_BADOPTION or utf8_available != 1) {
        LOG_ERROR("This version of the PCRE library does not support UTF8!");
    }

    return true;
}


static const bool dummy_variable(CheckPCRE_UTF8Compatibility());


ThreadSafeRegexMatcher::MatchResult::MatchResult(const std::string &subject): subject_(subject), matched_(false), match_count_(0) {
    substr_indices_.resize(ThreadSafeRegexMatcher::MAX_SUBSTRING_MATCHES * 3);
}


std::string ThreadSafeRegexMatcher::MatchResult::operator[](const unsigned group) const {
    if (unlikely(group >= match_count_)) {
        throw std::out_of_range("in ThreadSafeRegexMatcher::MatchResult::operator[]: group(" + std::to_string(group)
                                + ") >= " + std::to_string(match_count_) + "!");
    }

    const unsigned first_index(group * 2);
    const unsigned substring_length(substr_indices_[first_index + 1] - substr_indices_[first_index]);
    return (substring_length == 0) ? "" : subject_.substr(substr_indices_[first_index], substring_length);
}


bool CompileRegex(const std::string &pattern, const unsigned options, ::pcre **pcre_arg, ::pcre_extra **pcre_extra_arg,
                  std::string * const err_msg) {
    if (err_msg != nullptr)
        err_msg->clear();

    const char *errptr;
    int erroffset;

    int pcre_options(0);
    if (options & RegexMatcher::ENABLE_UTF8)
        pcre_options |= PCRE_UTF8;
    if (options & RegexMatcher::ENABLE_UCP)
        pcre_options |= PCRE_UCP;
    if (options & RegexMatcher::CASE_INSENSITIVE)
        pcre_options |= PCRE_CASELESS;
    if (options & RegexMatcher::MULTILINE)
        pcre_options |= PCRE_MULTILINE;

    *pcre_arg = ::pcre_compile(pattern.c_str(), pcre_options, &errptr, &erroffset, nullptr);
    if (*pcre_arg == nullptr) {
        *pcre_extra_arg = nullptr;
        if (err_msg != nullptr)
            *err_msg = "failed to compile invalid regular expression: \"" + pattern + "\"! (" + std::string(errptr) + ")";
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


ThreadSafeRegexMatcher::ThreadSafeRegexMatcher(const std::string &pattern, const unsigned options)
    : pattern_(pattern), options_(options), pcre_data_(new PcreData) {
    std::string err_msg;
    if (not CompileRegex(pattern_, options_, &pcre_data_->pcre_, &pcre_data_->pcre_extra_, &err_msg))
        LOG_ERROR("failed to compile pattern: \"" + pattern + "\": " + err_msg);
}


ThreadSafeRegexMatcher::MatchResult ThreadSafeRegexMatcher::match(const std::string &subject, const size_t subject_start_offset,
                                                                  size_t * const start_pos, size_t * const end_pos) const {
    MatchResult match_result(subject);
    const int retcode(::pcre_exec(pcre_data_->pcre_, pcre_data_->pcre_extra_, subject.data(), subject.length(), subject_start_offset, 0,
                                  &match_result.substr_indices_[0], match_result.substr_indices_.size()));

    if (retcode == 0) {
        LOG_ERROR("Too many captured substrings! (We only support " + std::to_string(match_result.substr_indices_.size() / 3 - 1)
                  + " substrings.)");
    }

    if (retcode > 0) {
        match_result.match_count_ = retcode;
        match_result.matched_ = true;
        if (start_pos != nullptr)
            *start_pos = match_result.substr_indices_[0];
        if (end_pos != nullptr)
            *end_pos = match_result.substr_indices_[1];

        return match_result;
    }

    if (retcode != PCRE_ERROR_NOMATCH) {
        if (retcode == PCRE_ERROR_BADUTF8)
            match_result.error_message_ = "invalid UTF-8 in subject";
        else
            match_result.error_message_ = "unknown PCRE error for pattern '" + pattern_ + "': " + std::to_string(retcode);
    }

    return match_result;
}


std::string ThreadSafeRegexMatcher::replaceAll(const std::string &subject, const std::string &replacement) const {
    if (not match(subject))
        return subject;

    std::string replaced_string;
    // the matches need to be sequentially sorted from left to right
    size_t subject_start_offset(0), match_start_offset(0), match_end_offset(0);
    while (subject_start_offset < subject.length()) {
        if (not match(subject, subject_start_offset, &match_start_offset, &match_end_offset))
            break;

        if (subject_start_offset == match_start_offset and subject_start_offset == match_end_offset) {
            replaced_string += subject[subject_start_offset++];
            continue;
        }

        replaced_string += subject.substr(subject_start_offset, match_start_offset - subject_start_offset);
        replaced_string += replacement;
        subject_start_offset = match_end_offset;
    }

    while (subject_start_offset < subject.length())
        replaced_string += subject[subject_start_offset++];

    return replaced_string;
}


template <class MatchedGroups>
std::string InsertReplacement(const MatchedGroups &result, const std::string &replacement_pattern) {
    std::string replacement_text;

    bool backslash_seen(false);
    for (const char ch : replacement_pattern) {
        if (backslash_seen) {
            if (unlikely(ch == '\\'))
                replacement_text += '\\';
            else {
                if (unlikely(not StringUtil::IsDigit(ch)))
                    LOG_ERROR("not a digit nor a backslash found in the replacement pattern \"" + replacement_pattern + "\"!");
                const unsigned group_no(ch - '0'); // Only works with ASCII!
                replacement_text += result[group_no];
            }

            backslash_seen = false;
        } else if (ch == '\\')
            backslash_seen = true;
        else
            replacement_text += ch;
    }

    return replacement_text;
}


std::string ThreadSafeRegexMatcher::replaceWithBackreferences(const std::string &subject, const std::string &replacement,
                                                              const bool global) {
    if (not match(subject))
        return subject;

    std::string replaced_string;
    // the matches need to be sequentially sorted from left to right
    size_t subject_start_offset(0), match_start_offset(0), match_end_offset(0);
    for (MatchResult result = match(subject, subject_start_offset, &match_start_offset, &match_end_offset);
         subject_start_offset < subject.length() and result;
         result = match(subject, subject_start_offset, &match_start_offset, &match_end_offset))
    {
        if (subject_start_offset == match_start_offset and subject_start_offset == match_end_offset) {
            replaced_string += subject[subject_start_offset++];
            continue;
        }

        replaced_string += subject.substr(subject_start_offset, match_start_offset - subject_start_offset);
        replaced_string += InsertReplacement<ThreadSafeRegexMatcher::MatchResult>(result, replacement);
        subject_start_offset = match_end_offset;
        if (not global)
            break;
    }

    while (subject_start_offset < subject.length())
        replaced_string += subject[subject_start_offset++];

    return replaced_string;
}


bool RegexMatcher::utf8_configured_;


RegexMatcher *RegexMatcher::RegexMatcherFactory(const std::string &pattern, std::string * const err_msg, const unsigned options) {
    // Make sure the PCRE library supports UTF8:
    if ((options & RegexMatcher::ENABLE_UTF8) and not RegexMatcher::utf8_configured_) {
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
    if (not CompileRegex(pattern, options, &pcre_ptr, &pcre_extra_ptr, err_msg)) {
        if (err_msg != nullptr and err_msg->empty())
            *err_msg = "failed to compile pattern: \"" + pattern + "\"";
        return nullptr;
    }

    return new RegexMatcher(pattern, options, pcre_ptr, pcre_extra_ptr);
}


RegexMatcher *RegexMatcher::RegexMatcherFactoryOrDie(const std::string &regex, const unsigned options) {
    std::string error_message;
    RegexMatcher *regex_matcher(RegexMatcher::RegexMatcherFactory(regex, &error_message, options));
    if (regex_matcher == nullptr or not error_message.empty())
        LOG_ERROR("failed to compile regex \"" + regex + "\": " + error_message);

    return regex_matcher;
}


RegexMatcher::RegexMatcher(const RegexMatcher &that): pattern_(that.pattern_) {
    if (this == &that)
        return;

    if (that.pcre_ == nullptr) {
        pcre_ = nullptr;
        pcre_extra_ = nullptr;
    } else {
        std::string err_msg;
        if (not CompileRegex(pattern_, that.options_, &pcre_, &pcre_extra_, &err_msg))
            logger->error("In RegexMatcher copy constructor: unexpected error: " + err_msg);
        substr_vector_ = that.substr_vector_;
        last_match_count_ = that.last_match_count_;
    }
}


RegexMatcher::RegexMatcher(RegexMatcher &&that)
    : pattern_(std::move(that.pattern_)), options_(that.options_), pcre_(that.pcre_), pcre_extra_(that.pcre_extra_),
      last_subject_(std::move(that.last_subject_)), substr_vector_(std::move(that.substr_vector_)),
      last_match_count_(that.last_match_count_) {
    that.pcre_ = nullptr;
    that.pcre_extra_ = nullptr;
}


bool RegexMatcher::matched(const std::string &subject, const size_t subject_start_offset, std::string * const err_msg,
                           size_t * const start_pos, size_t * const end_pos) {
    if (err_msg != nullptr)
        err_msg->clear();

    const int retcode(::pcre_exec(pcre_, pcre_extra_, subject.data(), subject.length(), subject_start_offset, 0, &substr_vector_[0],
                                  substr_vector_.size()));

    if (retcode == 0) {
        if (err_msg != nullptr)
            *err_msg = "Too many captured substrings! (We only support " + std::to_string(substr_vector_.size() / 3 - 1) + " substrings.)";
        return false;
    }

    if (retcode > 0) {
        last_match_count_ = retcode;
        last_subject_ = subject;
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


std::string RegexMatcher::replaceAll(const std::string &subject, const std::string &replacement) {
    if (not matched(subject))
        return subject;

    std::string replaced_string;
    // the matches need to be sequentially sorted from left to right
    size_t subject_start_offset(0), match_start_offset(0), match_end_offset(0);
    while (subject_start_offset < subject.length()
           and matched(subject, subject_start_offset, /* err_msg */ nullptr, &match_start_offset, &match_end_offset))
    {
        if (subject_start_offset == match_start_offset and subject_start_offset == match_end_offset) {
            replaced_string += subject[subject_start_offset++];
            continue;
        }

        replaced_string += subject.substr(subject_start_offset, match_start_offset - subject_start_offset);
        replaced_string += replacement;
        subject_start_offset = match_end_offset;
    }

    while (subject_start_offset < subject.length())
        replaced_string += subject[subject_start_offset++];

    return replaced_string;
}


std::string RegexMatcher::replaceWithBackreferences(const std::string &subject, const std::string &replacement, const bool global) {
    if (not matched(subject))
        return subject;

    std::string replaced_string;
    // the matches need to be sequentially sorted from left to right
    size_t subject_start_offset(0), match_start_offset(0), match_end_offset(0);
    while (subject_start_offset < subject.length()
           and matched(subject, subject_start_offset, /* err_msg */ nullptr, &match_start_offset, &match_end_offset))
    {
        if (subject_start_offset == match_start_offset and subject_start_offset == match_end_offset) {
            replaced_string += subject[subject_start_offset++];
            continue;
        }

        replaced_string += subject.substr(subject_start_offset, match_start_offset - subject_start_offset);
        replaced_string += InsertReplacement<RegexMatcher>(*this, replacement);
        subject_start_offset = match_end_offset;
        if (not global)
            break;
    }

    while (subject_start_offset < subject.length())
        replaced_string += subject[subject_start_offset++];

    return replaced_string;
}


bool RegexMatcher::Matched(const std::string &regex, const std::string &subject, const unsigned options, std::string * const err_msg,
                           size_t * const start_pos, size_t * const end_pos) {
    static std::unordered_map<std::string, RegexMatcher *> regex_to_matcher_map;
    const std::string KEY(regex + ":" + std::to_string(options));
    const auto regex_and_matcher(regex_to_matcher_map.find(KEY));
    if (regex_and_matcher != regex_to_matcher_map.cend())
        return regex_and_matcher->second->matched(subject, err_msg, start_pos, end_pos);

    RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(regex, err_msg, options));
    if (matcher == nullptr)
        LOG_ERROR("Failed to compile pattern \"" + regex + "\": " + *err_msg);
    regex_to_matcher_map[KEY] = matcher;

    return matcher->matched(subject, err_msg, start_pos, end_pos);
}


std::string RegexMatcher::ReplaceAll(const std::string &regex, const std::string &subject, const std::string &replacement,
                                     const unsigned options) {
    std::string err_msg;
    auto matcher(RegexMatcherFactory(regex, &err_msg, options));
    if (matcher == nullptr)
        LOG_ERROR("failed to compile \"" + regex + "\": " + err_msg);
    const auto result(matcher->replaceAll(subject, replacement));
    delete matcher;
    return result;
}


std::string RegexMatcher::Escape(const std::string &subpattern) {
    // escape backslashes first to keep from overwriting other escape sequences
    std::string escaped_subpattern(subpattern);
    return StringUtil::BackslashEscape("\\^$.[]|()?*+{}", &escaped_subpattern);
}


std::string RegexMatcher::operator[](const unsigned group) const {
    if (unlikely(group >= last_match_count_))
        throw std::out_of_range("in RegexMatcher::operator[]: group(" + std::to_string(group) + ") >= " + std::to_string(last_match_count_)
                                + "!");

    const unsigned first_index(group * 2);
    const unsigned substring_length(substr_vector_[first_index + 1] - substr_vector_[first_index]);
    return (substring_length == 0) ? "" : last_subject_.substr(substr_vector_[first_index], substring_length);
}
