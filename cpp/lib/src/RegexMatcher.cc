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

bool HasUnicodeSupport() {
    static const bool supported = [] {
        uint32_t available = 0;
        return ::pcre2_config(PCRE2_CONFIG_UNICODE, &available) == 0 and available == 1;
    }();
    return supported;
}

bool IsInvalidUTF8(const int error_code) {
    return error_code <= PCRE2_ERROR_UTF8_ERR1 and error_code >= PCRE2_ERROR_UTF8_ERR21;
}

bool CompileRegex(const std::string &pattern, const unsigned options, pcre2_code ** const compiled,
                  std::string * const err_msg) {
    if (err_msg != nullptr)
        err_msg->clear();
    *compiled = nullptr;
    constexpr unsigned known_options = RegexMatcher::ENABLE_UTF8 | RegexMatcher::ENABLE_UCP
                                       | RegexMatcher::CASE_INSENSITIVE | RegexMatcher::MULTILINE;
    if (options & ~known_options) {
        if (err_msg != nullptr)
            *err_msg = "unknown RegexMatcher option bits: " + std::to_string(options & ~known_options);
        return false;
    }
    if ((options & (RegexMatcher::ENABLE_UTF8 | RegexMatcher::ENABLE_UCP)) and not HasUnicodeSupport()) {
        if (err_msg != nullptr)
            *err_msg = "This version of PCRE2 does not support Unicode!";
        return false;
    }
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
               std::vector<PCRE2_SIZE> * const offsets, const uint32_t match_options = 0) {
    offsets->clear();
    if (compiled == nullptr)
        return PCRE2_ERROR_NULL;
    const std::unique_ptr<pcre2_match_data, decltype(&::pcre2_match_data_free)> data(
        ::pcre2_match_data_create_from_pattern(compiled, nullptr), &::pcre2_match_data_free);
    if (not data)
        throw std::bad_alloc();
    const int result = ::pcre2_match(compiled, reinterpret_cast<PCRE2_SPTR>(subject.data()), subject.size(), start_offset,
                                     match_options, data.get(), nullptr);
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
            match_result.error_message_ = "PCRE2 error for pattern '" + pattern_ + "': " + RegexError(retcode);
    }

    return match_result;
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


namespace {

struct CapturedGroups {
    const std::string &subject_;
    const std::vector<PCRE2_SIZE> &offsets_;

    unsigned size() const { return offsets_.size() / 2; }
    std::string operator[](const unsigned group) const {
        if (group >= size())
            throw std::out_of_range("replacement capture group out of range: " + std::to_string(group));
        const PCRE2_SIZE start = offsets_[group * 2];
        return start == PCRE2_UNSET ? "" : subject_.substr(start, offsets_[group * 2 + 1] - start);
    }
};

template <class Replacement>
std::string ReplaceRegex(const pcre2_code * const compiled, const std::string &subject, const Replacement &replacement,
                         const bool global) {
    uint32_t compile_options = 0, newline = 0;
    ::pcre2_pattern_info(compiled, PCRE2_INFO_ALLOPTIONS, &compile_options);
    ::pcre2_pattern_info(compiled, PCRE2_INFO_NEWLINE, &newline);
    const bool utf = compile_options & PCRE2_UTF;
    const bool crlf = newline == PCRE2_NEWLINE_CRLF or newline == PCRE2_NEWLINE_ANY or newline == PCRE2_NEWLINE_ANYCRLF;
    size_t search_offset = 0, copied_until = 0;
    uint32_t match_options = 0;
    std::vector<PCRE2_SIZE> offsets;
    std::string result;
    while (search_offset <= subject.size()) {
        const int count = MatchRegex(compiled, subject, search_offset, &offsets, match_options);
        if (count == PCRE2_ERROR_NOMATCH and match_options != 0) {
            if (search_offset == subject.size())
                break;
            // After an empty match, retry a nonempty alternative at the same
            // position before advancing by a complete UTF-8 character or CRLF.
            if (crlf and subject[search_offset] == '\r' and search_offset + 1 < subject.size()
                and subject[search_offset + 1] == '\n')
                search_offset += 2;
            else {
                ++search_offset;
                if (utf)
                    while (search_offset < subject.size() and (static_cast<unsigned char>(subject[search_offset]) & 0xc0) == 0x80)
                        ++search_offset;
            }
            match_options = 0;
            continue;
        }
        if (count <= 0)
            break;
        const size_t start = offsets[0], end = offsets[1];
        result.append(subject, copied_until, start - copied_until);
        result += replacement(CapturedGroups{subject, offsets});
        copied_until = end;
        if (not global)
            break;
        search_offset = end;
        match_options = start == end ? PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED : 0;
    }
    result.append(subject, copied_until, std::string::npos);
    return result;
}

} // namespace


std::string ThreadSafeRegexMatcher::replaceAll(const std::string &subject, const std::string &replacement) const {
    return ReplaceRegex(pcre_data_->pcre_, subject, [&replacement](const CapturedGroups &) { return replacement; }, true);
}


std::string ThreadSafeRegexMatcher::replaceWithBackreferences(const std::string &subject, const std::string &replacement,
                                                              const bool global) const {
    return ReplaceRegex(pcre_data_->pcre_, subject,
                        [&replacement](const CapturedGroups &groups) { return InsertReplacement(groups, replacement); }, global);
}


RegexMatcher *RegexMatcher::RegexMatcherFactory(const std::string &pattern, std::string * const err_msg, const unsigned options) {
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
    return ReplaceRegex(pcre_, subject, [&](const CapturedGroups &groups) {
        last_subject_ = subject;
        substr_vector_ = groups.offsets_;
        last_match_count_ = groups.size();
        return replacement;
    }, true);
}


std::string RegexMatcher::replaceWithBackreferences(const std::string &subject, const std::string &replacement, const bool global) {
    if (not matched(subject))
        return subject;
    return ReplaceRegex(pcre_, subject, [&](const CapturedGroups &groups) {
        last_subject_ = subject;
        substr_vector_ = groups.offsets_;
        last_match_count_ = groups.size();
        return InsertReplacement(groups, replacement);
    }, global);
}


bool RegexMatcher::Matched(const std::string &regex, const std::string &subject, const unsigned options, std::string * const err_msg,
                           size_t * const start_pos, size_t * const end_pos) {
    // Deprecated matchers store mutable results; each thread needs its own cache.
    static thread_local std::unordered_map<std::string, std::unique_ptr<RegexMatcher>> regex_to_matcher_map;
    const std::string KEY(regex + ":" + std::to_string(options));
    std::string local_error;
    std::string * const error_output = err_msg != nullptr ? err_msg : &local_error;
    const auto regex_and_matcher(regex_to_matcher_map.find(KEY));
    if (regex_and_matcher != regex_to_matcher_map.cend())
        return regex_and_matcher->second->matched(subject, error_output, start_pos, end_pos);

    std::unique_ptr<RegexMatcher> matcher(RegexMatcher::RegexMatcherFactory(regex, error_output, options));
    if (matcher == nullptr)
        LOG_ERROR("Failed to compile pattern \"" + regex + "\": " + *error_output);
    const auto inserted = regex_to_matcher_map.emplace(KEY, std::move(matcher));
    return inserted.first->second->matched(subject, error_output, start_pos, end_pos);
}


std::string RegexMatcher::ReplaceAll(const std::string &regex, const std::string &subject, const std::string &replacement,
                                     const unsigned options) {
    std::string err_msg;
    const std::unique_ptr<RegexMatcher> matcher(RegexMatcherFactory(regex, &err_msg, options));
    if (matcher == nullptr)
        LOG_ERROR("failed to compile \"" + regex + "\": " + err_msg);
    const auto result(matcher->replaceAll(subject, replacement));
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
