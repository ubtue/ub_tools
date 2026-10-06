/** \file   RegexMatcher.cc
 *  \brief  Implementation of the RegexMatcher class.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *  \author Steven Lolong (steven.lolong@uni-tuebingen.de)
 *
 *  \copyright 2015-2026 Universitätsbibliothek Tübingen.  All rights reserved.
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
    uint32_t utf8_available = 0;
    if (::pcre2_config(PCRE2_CONFIG_UNICODE, &utf8_available) != 0 or utf8_available != 1) {
        LOG_ERROR("This version of the PCRE library does not support UTF8!");
    }

    return true;
}


static const bool dummy_variable(CheckPCRE_UTF8Compatibility());


ThreadSafeRegexMatcher::MatchResult::MatchResult(const std::string &subject): subject_(subject), matched_(false), match_count_(0) {
}


std::string ThreadSafeRegexMatcher::MatchResult::operator[](const unsigned group) const {
    if (unlikely(group >= match_count_)) {
        throw std::out_of_range("in ThreadSafeRegexMatcher::MatchResult::operator[]: group(" + std::to_string(group)
                                + ") >= " + std::to_string(match_count_) + "!");
    }

    const unsigned first_index(group * 2);
    if (substr_indices_[first_index] == PCRE2_UNSET)
        return "";
    const PCRE2_SIZE substring_length(substr_indices_[first_index + 1] - substr_indices_[first_index]);
    return (substring_length == 0) ? "" : subject_.substr(substr_indices_[first_index], substring_length);
}


namespace {

std::string RegexError(const int error_code) {
    PCRE2_UCHAR text[256];
    const int length = ::pcre2_get_error_message(error_code, text, sizeof(text));
    return length >= 0 ? std::string(reinterpret_cast<const char *>(text), length)
                       : "PCRE2 error " + std::to_string(error_code);
}

bool IsInvalidUTF8(const int error_code) {
    return error_code <= PCRE2_ERROR_UTF8_ERR1 and error_code >= PCRE2_ERROR_UTF8_ERR21;
}

bool CompileRegex(const std::string &pattern, const unsigned options, pcre2_code ** const compiled,
                  std::string * const err_msg) {
    if (err_msg != nullptr)
        err_msg->clear();
    uint32_t pcre_options = 0;
    // Keep the public wrapper option values; they are not PCRE2 option bits.
    if (options & RegexMatcher::ENABLE_UTF8)
        pcre_options |= PCRE2_UTF;
    if (options & RegexMatcher::ENABLE_UCP)
        pcre_options |= PCRE2_UCP;
    if (options & RegexMatcher::CASE_INSENSITIVE)
        pcre_options |= PCRE2_CASELESS;
    if (options & RegexMatcher::MULTILINE)
        pcre_options |= PCRE2_MULTILINE;
    int error_code;
    PCRE2_SIZE error_offset;
    *compiled = ::pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.data()), pattern.size(), pcre_options,
                                &error_code, &error_offset, nullptr);
    if (*compiled == nullptr) {
        if (err_msg != nullptr)
            *err_msg = "failed to compile invalid regular expression: \"" + pattern + "\"! (" + RegexError(error_code)
                       + "; offset " + std::to_string(error_offset) + ")";
        return false;
    }
    return true;
}

int MatchRegex(const pcre2_code * const compiled, const std::string &subject, const size_t start_offset,
               std::vector<PCRE2_SIZE> * const offsets) {
    offsets->clear();
    if (compiled == nullptr)
        return PCRE2_ERROR_NULL;
    const std::unique_ptr<pcre2_match_data, decltype(&::pcre2_match_data_free)> data(
        ::pcre2_match_data_create_from_pattern(compiled, nullptr), &::pcre2_match_data_free);
    if (not data)
        throw std::bad_alloc();
    const int result = ::pcre2_match(compiled, reinterpret_cast<PCRE2_SPTR>(subject.data()), subject.size(), start_offset,
                                     0, data.get(), nullptr);
    if (result > 0) {
        const PCRE2_SIZE *ovector = ::pcre2_get_ovector_pointer(data.get());
        offsets->assign(ovector, ovector + 2 * result);
    }
    return result;
}

} // namespace


ThreadSafeRegexMatcher::ThreadSafeRegexMatcher(const std::string &pattern, const unsigned options)
    : pattern_(pattern), options_(options), pcre_data_(new PcreData) {
    std::string err_msg;
    if (not CompileRegex(pattern_, options_, &pcre_data_->pcre_, &err_msg))
        LOG_ERROR("failed to compile pattern: \"" + pattern + "\": " + err_msg);
}


ThreadSafeRegexMatcher::MatchResult ThreadSafeRegexMatcher::match(const std::string &subject, const size_t subject_start_offset,
                                                                  size_t * const start_pos, size_t * const end_pos) const {
    MatchResult match_result(subject);
    const int retcode = MatchRegex(pcre_data_->pcre_, subject, subject_start_offset, &match_result.substr_indices_);

    if (retcode > 0) {
        match_result.match_count_ = retcode;
        match_result.matched_ = true;
        if (start_pos != nullptr)
            *start_pos = match_result.substr_indices_[0];
        if (end_pos != nullptr)
            *end_pos = match_result.substr_indices_[1];

        return match_result;
    }

    if (retcode != PCRE2_ERROR_NOMATCH) {
        if (IsInvalidUTF8(retcode))
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
        uint32_t utf8_available = 0;
        if (::pcre2_config(PCRE2_CONFIG_UNICODE, &utf8_available) != 0) {
            if (err_msg != nullptr)
                *err_msg = "PCRE2 library does not support the Unicode configuration query!";
            return nullptr;
        }

        if (utf8_available != 1) {
            if (err_msg != nullptr)
                *err_msg = "This version of the PCRE library does not support UTF8!";
            return nullptr;
        }

        RegexMatcher::utf8_configured_ = true;
    }

    ::pcre2_code *pcre_ptr;
    if (not CompileRegex(pattern, options, &pcre_ptr, err_msg)) {
        if (err_msg != nullptr and err_msg->empty())
            *err_msg = "failed to compile pattern: \"" + pattern + "\"";
        return nullptr;
    }

    std::unique_ptr<pcre2_code, decltype(&::pcre2_code_free)> compiled(pcre_ptr, &::pcre2_code_free);
    auto matcher = new RegexMatcher(pattern, options, compiled.get());
    // The matcher takes ownership only after its construction succeeds.
    compiled.release();
    return matcher;
}


RegexMatcher *RegexMatcher::RegexMatcherFactoryOrDie(const std::string &regex, const unsigned options) {
    std::string error_message;
    RegexMatcher *regex_matcher(RegexMatcher::RegexMatcherFactory(regex, &error_message, options));
    if (regex_matcher == nullptr or not error_message.empty())
        LOG_ERROR("failed to compile regex \"" + regex + "\": " + error_message);

    return regex_matcher;
}


RegexMatcher::RegexMatcher(const RegexMatcher &that)
    : pattern_(that.pattern_), options_(that.options_), pcre_(nullptr), last_subject_(that.last_subject_),
      substr_vector_(that.substr_vector_), last_match_count_(that.last_match_count_) {
    if (that.pcre_ != nullptr) {
        pcre_ = ::pcre2_code_copy(that.pcre_);
        if (pcre_ == nullptr)
            throw std::bad_alloc();
    }
}


RegexMatcher::RegexMatcher(RegexMatcher &&that)
    : pattern_(std::move(that.pattern_)), options_(that.options_), pcre_(that.pcre_),
      last_subject_(std::move(that.last_subject_)), substr_vector_(std::move(that.substr_vector_)),
      last_match_count_(that.last_match_count_) {
    that.pcre_ = nullptr;
    that.last_match_count_ = 0;
}


bool RegexMatcher::matched(const std::string &subject, const size_t subject_start_offset, std::string * const err_msg,
                           size_t * const start_pos, size_t * const end_pos) {
    if (err_msg != nullptr)
        err_msg->clear();

    last_match_count_ = 0;
    last_subject_.clear();
    const int retcode = MatchRegex(pcre_, subject, subject_start_offset, &substr_vector_);

    if (retcode > 0) {
        last_match_count_ = retcode;
        last_subject_ = subject;
        if (start_pos != nullptr)
            *start_pos = substr_vector_[0];
        if (end_pos != nullptr)
            *end_pos = substr_vector_[1];
        return true;
    }

    if (retcode != PCRE2_ERROR_NOMATCH) {
        if (IsInvalidUTF8(retcode)) {
            if (err_msg != nullptr)
                *err_msg = "A \"subject\" with invalid UTF-8 was passed into RegexMatcher::matched()!";
        } else if (err_msg != nullptr)
            *err_msg = RegexError(retcode);
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
    static std::unordered_map<std::string, std::unique_ptr<RegexMatcher>> regex_to_matcher_map;
    const std::string KEY(regex + ":" + std::to_string(options));
    std::string local_error;
    std::string * const error_output = err_msg != nullptr ? err_msg : &local_error;
    const auto regex_and_matcher(regex_to_matcher_map.find(KEY));
    if (regex_and_matcher != regex_to_matcher_map.cend())
        return regex_and_matcher->second->matched(subject, error_output, start_pos, end_pos);

    RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory(regex, error_output, options));
    if (matcher == nullptr)
        LOG_ERROR("Failed to compile pattern \"" + regex + "\": " + *error_output);
    regex_to_matcher_map[KEY].reset(matcher);

    return matcher->matched(subject, error_output, start_pos, end_pos);
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
    if (substr_vector_[first_index] == PCRE2_UNSET)
        return "";
    const PCRE2_SIZE substring_length(substr_vector_[first_index + 1] - substr_vector_[first_index]);
    return (substring_length == 0) ? "" : last_subject_.substr(substr_vector_[first_index], substring_length);
}
