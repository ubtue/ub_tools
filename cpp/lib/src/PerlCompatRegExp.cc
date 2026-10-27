/** \file    PerlCompatRegExp.cc
 *  \brief   Implementation of class PerlCompatRegExp, a wrapper around libpcre2.
 *  \author  Dr. Johannes Ruscheinski
 *  \author  Steven Lolong (steven.lolong@uni-tuebingen.de)
 */

/*
 *  Copyright 2002-2008 Project iVia.
 *  Copyright 2002-2008 The Regents of The University of California.
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

#include "PerlCompatRegExp.h"
#include <stdexcept>
#include <cassert>
#include <cstring>
#include <memory>
#include <utility>
#include "Compiler.h"


namespace {

using CompiledPattern = std::unique_ptr<pcre2_code, decltype(&::pcre2_code_free)>;

CompiledPattern CompilePattern(const std::string &pattern, const unsigned options, int * const error_code,
                               PCRE2_SIZE * const error_offset) {
    const auto free_tables = [](const uint8_t *tables) { ::pcre2_maketables_free(nullptr, tables); };
    const std::unique_ptr<const uint8_t, decltype(free_tables)> tables(::pcre2_maketables(nullptr), free_tables);
    const std::unique_ptr<pcre2_compile_context, decltype(&::pcre2_compile_context_free)> context(
        ::pcre2_compile_context_create(nullptr), &::pcre2_compile_context_free);
    if (not tables or not context)
        throw std::bad_alloc();
    ::pcre2_set_character_tables(context.get(), tables.get());

    CompiledPattern compiled(::pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.data()), pattern.size(), options, error_code,
                                            error_offset, context.get()), &::pcre2_code_free);
    if (not compiled)
        return compiled;

    // Each compiled pattern owns its tables, so changing the locale or resetting
    // another expression cannot invalidate tables used by an existing pattern.
    CompiledPattern owned(::pcre2_code_copy_with_tables(compiled.get()), &::pcre2_code_free);
    if (not owned)
        throw std::bad_alloc();
    return owned;
}

} // namespace


PerlCompatRegExp::PerlCompatRegExp(const std::string &pattern, const ProcessingMode processing_mode, const int options)
    : PerlCompatRegExp() {
    resetPattern(pattern, processing_mode, options);
}


PerlCompatRegExp::PerlCompatRegExp(const PerlCompatRegExp &rhs)
    : subject_text_(rhs.subject_text_), compiled_pattern_(nullptr), substring_match_count_(rhs.substring_match_count_),
      pattern_(rhs.pattern_), processing_mode_(rhs.processing_mode_), options_(rhs.options_), offset_vector_(rhs.offset_vector_) {
    if (rhs.compiled_pattern_ != nullptr) {
        compiled_pattern_ = ::pcre2_code_copy_with_tables(rhs.compiled_pattern_);
        if (compiled_pattern_ == nullptr)
            throw std::bad_alloc();
    }
}


PerlCompatRegExp::~PerlCompatRegExp() {
    if (compiled_pattern_ != nullptr)
        ::pcre2_code_free(compiled_pattern_);
}


const PerlCompatRegExp &PerlCompatRegExp::operator=(const PerlCompatRegExp &rhs) {
    // Prevent self-assignment:
    if (this != &rhs) {
        PerlCompatRegExp copy(rhs);
        std::swap(subject_text_, copy.subject_text_);
        std::swap(compiled_pattern_, copy.compiled_pattern_);
        std::swap(substring_match_count_, copy.substring_match_count_);
        std::swap(pattern_, copy.pattern_);
        std::swap(processing_mode_, copy.processing_mode_);
        std::swap(options_, copy.options_);
        std::swap(offset_vector_, copy.offset_vector_);
    }

    return *this;
}


bool PerlCompatRegExp::safeResetPattern(const std::string &new_pattern, const ProcessingMode new_processing_mode, const int new_options) {
    std::string error_message;
    return internalResetPattern(new_pattern, new_processing_mode, new_options, &error_message);
}


void PerlCompatRegExp::resetPattern(const std::string &new_pattern, const ProcessingMode new_processing_mode, const int new_options) {
    std::string error_message;
    if (not internalResetPattern(new_pattern, new_processing_mode, new_options, &error_message))
        throw std::runtime_error("in PerlCompatRegExp::resetPattern: " + error_message);
}


bool PerlCompatRegExp::match(const std::string &subject_text, const size_t start_offset, size_t * const start_pos, size_t * const length,
                             const int options) const {
    assert(compiled_pattern_ != nullptr);

    subject_text_ = subject_text;
    substring_match_count_ = 0;
    offset_vector_.clear();
    if (compiled_pattern_ == nullptr or start_offset > subject_text_.size())
        return false;

    const std::unique_ptr<pcre2_match_data, decltype(&::pcre2_match_data_free)> match_data(
        ::pcre2_match_data_create_from_pattern(compiled_pattern_, nullptr), &::pcre2_match_data_free);
    if (not match_data)
        throw std::bad_alloc();
    const int match_count(::pcre2_match(compiled_pattern_, reinterpret_cast<PCRE2_SPTR>(subject_text_.data()), subject_text_.size(),
                                       start_offset, static_cast<uint32_t>(options), match_data.get(), nullptr));

    if (match_count < 1)
        return false;
    else {
        const PCRE2_SIZE *offsets = ::pcre2_get_ovector_pointer(match_data.get());
        offset_vector_.assign(offsets, offsets + 2 * match_count);
        substring_match_count_ = match_count - 1;
        if (start_pos != nullptr)
            *start_pos = offset_vector_[0];
        if (length != nullptr)
            *length = offset_vector_[1] - offset_vector_[0];
        return true;
    }
}


bool PerlCompatRegExp::Match(const std::string &pattern, const std::string &subject_text, const size_t start_offset,
                             size_t * const start_pos, size_t * const length, const int options) {
    PerlCompatRegExp perl_compat_reg_exp(pattern, DONT_OPTIMIZE_FOR_MULTIPLE_USE, options);
    return perl_compat_reg_exp.match(subject_text, start_offset, start_pos, length);
}


bool PerlCompatRegExp::multiMatch(const std::string &subject_text, std::vector<std::string> * const matched_substrings,
                                  const int options) const {
    matched_substrings->clear();

    size_t start_offset(0);
    size_t start_pos, match_length;
    while (match(subject_text, start_offset, &start_pos, &match_length, options)) {
        matched_substrings->push_back(subject_text.substr(start_pos, match_length));
        if (match_length == 0)
            ++match_length;
        start_offset = start_pos + match_length; // For the next match attempt.
    }

    return not matched_substrings->empty();
}


bool PerlCompatRegExp::MultiMatch(const std::string &pattern, const std::string &subject_text,
                                  std::vector<std::string> * const matched_substrings, const int options) {
    PerlCompatRegExp perl_compat_reg_exp(pattern, DONT_OPTIMIZE_FOR_MULTIPLE_USE, options);
    return perl_compat_reg_exp.multiMatch(subject_text, matched_substrings);
}


bool PerlCompatRegExp::Match(const std::string &pattern, const std::string &subject_text, const int options) {
    size_t start_pos, length;
    return Match(pattern, subject_text, 0, &start_pos, &length, options);
}


std::string PerlCompatRegExp::GenerateReplacementText(const PerlCompatRegExp &reg_exp, const std::string &replacement) {
    std::string replacement_text;
    bool escaped(false), scanning_reference(false);
    unsigned substring_reference(0);
    for (std::string::const_iterator ch(replacement.begin()); ch != replacement.end(); ++ch) {
        if (scanning_reference) {
            if (isdigit(*ch)) {
                substring_reference = 10 * substring_reference + (*ch - '0');
                continue;
            } else {
                std::string matched_substring;
                reg_exp.getMatchedSubstring(substring_reference, &matched_substring);
                substring_reference = 0;
                replacement_text += matched_substring;
                scanning_reference = false;
            }
        }

        if (escaped) {
            replacement_text += *ch;
            escaped = false;
        } else if (*ch == '\\')
            escaped = true;
        else if (*ch == '$')
            scanning_reference = true;
        else
            replacement_text += *ch;
    }
    if (escaped)
        throw std::runtime_error("in PerlCompatRegExp::GenerateReplacementText: trailing backslash escape in replacement!");
    if (scanning_reference) {
        std::string matched_substring;
        reg_exp.getMatchedSubstring(substring_reference, &matched_substring);
        replacement_text += matched_substring;
    }

    return replacement_text;
}


bool PerlCompatRegExp::getMatchedSubstring(unsigned index, std::string * const matched_substring) const {
    if (substring_match_count_ == 0)
        throw std::runtime_error("in PerlCompatRegExp::getMatchedSubstring: no matches available!");

    if (index == 0)
        throw std::runtime_error("in PerlCompatRegExp::getMatchedSubstring: index out of range (indexes start at 1)!");

    if (index > substring_match_count_)
        return false;

    index <<= 1; // Indexes come in pairs.
    if (offset_vector_[index] == PCRE2_UNSET) {
        matched_substring->clear();
        return true;
    }

    *matched_substring = subject_text_.substr(offset_vector_[index], offset_vector_[index + 1] - offset_vector_[index]);
    return true;
}


std::string PerlCompatRegExp::getMatchedSubstring(unsigned index) const {
    if (substring_match_count_ == 0)
        throw std::runtime_error("in PerlCompatRegExp::getMatchedSubstring: no matches available!");

    if (index == 0)
        throw std::runtime_error("in PerlCompatRegExp::getMatchedSubstring: index out of range (indexes start at 1)!");

    if (index > substring_match_count_)
        throw std::runtime_error("in PerlCompatRegExp::getMatchedSubstring: asked for substring beyond available count!");

    index <<= 1; // Indexes come in pairs.
    if (offset_vector_[index] == PCRE2_UNSET)
        return "";

    return subject_text_.substr(offset_vector_[index], offset_vector_[index + 1] - offset_vector_[index]);
}


std::string PerlCompatRegExp::Subst(const std::string &pattern, const std::string &replacement, const std::string &subject_text,
                                    const bool global, const int options) {
    PerlCompatRegExp reg_exp(pattern, global ? OPTIMIZE_FOR_MULTIPLE_USE : DONT_OPTIMIZE_FOR_MULTIPLE_USE, options);

    std::string ret_val;
    size_t start_pos, length;
    if (not reg_exp.match(subject_text, 0, &start_pos, &length))
        return subject_text;
    else
        ret_val = subject_text.substr(0, start_pos) + GenerateReplacementText(reg_exp, replacement);

    size_t last_end_pos(start_pos + length);
    while (global and reg_exp.match(subject_text, start_pos + length, &start_pos, &length)) {
        ret_val += subject_text.substr(last_end_pos, start_pos - last_end_pos) + GenerateReplacementText(reg_exp, replacement);
        last_end_pos = start_pos + length;
    }

    ret_val += subject_text.substr(last_end_pos);
    return ret_val;
}


std::string PerlCompatRegExp::subst(const std::string &replacement, const std::string &subject_text, const bool global) const {
    std::string ret_val;
    size_t start_pos, length;
    if (not match(subject_text, 0, &start_pos, &length))
        return subject_text;
    else
        ret_val = subject_text.substr(0, start_pos) + GenerateReplacementText(*this, replacement);

    size_t last_end_pos(start_pos + length);
    while (global and match(subject_text, start_pos + length, &start_pos, &length)) {
        ret_val += subject_text.substr(last_end_pos, start_pos - last_end_pos) + GenerateReplacementText(*this, replacement);
        last_end_pos = start_pos + length;
    }

    ret_val += subject_text.substr(last_end_pos);
    return ret_val;
}


std::string PerlCompatRegExp::Subst(const std::string &subst_expression, const std::string &subject_text, const int options) {
    if (subst_expression.length() < 3)
        throw std::runtime_error("in PerlCompatRegExp::Subst: subst_expression.length() < 3!");

    const char DELIMITER = subst_expression[0];

    std::string pattern, replacement;
    bool scanning_pattern(true), escaped(false);
    std::string::const_iterator ch(subst_expression.begin());
    for (++ch; /* forever */; ++ch) {
        if (ch == subst_expression.end())
            throw std::runtime_error("in PerlCompatRegExp::Subst: missing final delimiter!");
        else if (escaped) {
            escaped = false;
            if (scanning_pattern)
                pattern += *ch;
            else
                replacement += *ch;
        } else if (*ch == '\\') {
            escaped = true;
            if (scanning_pattern)
                pattern += *ch;
            else
                replacement += *ch;
        } else if (*ch == DELIMITER) {
            if (scanning_pattern)
                scanning_pattern = false;
            else {
                bool global(false);
                if (++ch != subst_expression.end()) {
                    if (*ch == 'g') {
                        global = true;
                        ++ch;
                    }
                }
                if (ch != subst_expression.end())
                    throw std::runtime_error("in PerlCompatRegExp::Subst: unexpected delimiter before end of replacement text!");

                return Subst(pattern, replacement, subject_text, global, options);
            }
        } else {
            if (scanning_pattern)
                pattern += *ch;
            else
                replacement += *ch;
        }
    }
}


bool PerlCompatRegExp::IsValid(const std::string &test_pattern) {
    int error_code;
    PCRE2_SIZE error_offset;
    return CompilePattern(test_pattern, 0, &error_code, &error_offset) != nullptr;
}


bool PerlCompatRegExp::internalResetPattern(const std::string &new_pattern, const ProcessingMode new_processing_mode, const int new_options,
                                            std::string * const error_message) {
    int error_code;
    PCRE2_SIZE error_offset;
    auto compiled = CompilePattern(new_pattern, static_cast<uint32_t>(new_options), &error_code, &error_offset);
    if (not compiled) {
        PCRE2_UCHAR error_text[256];
        const int error_length = ::pcre2_get_error_message(error_code, error_text, sizeof(error_text));
        const std::string description = error_length >= 0
                                           ? std::string(reinterpret_cast<const char *>(error_text), error_length)
                                           : "PCRE2 error " + std::to_string(error_code);
        *error_message = "error \"" + description + "\" while compiling pattern at offset " + std::to_string(error_offset)
                         + " (" + new_pattern.substr(error_offset) + ")!";
        return false;
    }

    pattern_ = new_pattern;
    processing_mode_ = new_processing_mode;
    options_ = new_options;
    ::pcre2_code_free(compiled_pattern_);
    compiled_pattern_ = compiled.release();
    substring_match_count_ = 0;
    subject_text_.clear();
    offset_vector_.clear();
    error_message->clear();
    return true;
}


bool PerlCompatRegExp::IsMetacharacter(const char ch) {
    return std::strchr("\\^$.[]()?*+{}", ch) != nullptr;
}


std::string PerlCompatRegExp::EscapeMetacharacters(const std::string &s) {
    std::string escaped_s;
    escaped_s.reserve(s.size());
    for (std::string::const_iterator ch(s.begin()); ch != s.end(); ++ch) {
        if (IsMetacharacter(*ch))
            escaped_s += '\\';
        escaped_s += *ch;
    }

    return escaped_s;
}


PerlCompatRegExps::PerlCompatRegExps(const PerlCompatRegExps &rhs)
    : processing_mode_(rhs.processing_mode_), options_(rhs.options_), patterns_(rhs.patterns_), reg_exps_(rhs.reg_exps_) {
}


bool PerlCompatRegExps::empty() const {
    for (std::list<PerlCompatRegExp>::const_iterator reg_exp(reg_exps_.begin()); reg_exp != reg_exps_.end(); ++reg_exp) {
        if (not reg_exp->empty())
            return false;
    }

    return true;
}


const PerlCompatRegExps &PerlCompatRegExps::operator=(const PerlCompatRegExps &rhs) {
    // Prevent self-assignment:
    if (this != &rhs) {
#if 0
        reg_exps_.clear();
        for (std::list<PerlCompatRegExp>::const_iterator reg_exp(rhs.reg_exps_.begin()); reg_exp != rhs.reg_exps_.end(); ++reg_exp)
            reg_exps_.push_back(*reg_exp);
#else
        processing_mode_ = rhs.processing_mode_;
        options_ = rhs.options_;
        patterns_ = rhs.patterns_;
        reg_exps_ = rhs.reg_exps_;
#endif
    }

    return *this;
}


bool PerlCompatRegExps::matchAny(const std::string &subject_text, const size_t start_offset, size_t * const start_pos,
                                 size_t * const length, const int options) const {
    for (std::list<PerlCompatRegExp>::const_iterator reg_exp(reg_exps_.begin()); reg_exp != reg_exps_.end(); ++reg_exp) {
        if (reg_exp->match(subject_text, start_offset, start_pos, length, options))
            return true;
    }

    return false;
}


bool PerlCompatRegExps::matchAny(const std::string &subject_text, const int options) const {
    for (std::list<PerlCompatRegExp>::const_iterator reg_exp(reg_exps_.begin()); reg_exp != reg_exps_.end(); ++reg_exp) {
        if (reg_exp->match(subject_text, options))
            return true;
    }

    return false;
}


bool PerlCompatRegExps::multiMatch(const std::string &subject_text, std::vector<std::string> * const matched_substrings,
                                   const int options) const {
    matched_substrings->clear();

    size_t start_offset(0);
    size_t start_pos, match_length;
    while (matchAny(subject_text, start_offset, &start_pos, &match_length, options)) {
        matched_substrings->push_back(subject_text.substr(start_pos, match_length));
        start_offset = start_pos + match_length; // For the next match attempt.
    }

    return not matched_substrings->empty();
}


PerlCompatSubst::PerlCompatSubst(const std::string &subst_expr): subst_expr_(subst_expr), global_(false), perl_compat_regexp_(nullptr) {
    if (subst_expr.length() < 3)
        throw std::runtime_error("in PerlCompatSubst::PerlCompatSubst: subst_expression.length() < 3!");

    const char DELIMITER = subst_expr[0];

    std::string pattern;
    bool scanning_pattern(true), escaped(false);
    std::string::const_iterator ch(subst_expr.begin());
    for (++ch; /* forever */; ++ch) {
        if (ch == subst_expr.end())
            throw std::runtime_error("in PerlCompatRegExp::PerlCompatRegExp: missing final delimiter!");
        else if (escaped) {
            escaped = false;
            if (scanning_pattern)
                pattern += *ch;
            else
                replacement_ += *ch;
        } else if (*ch == '\\') {
            escaped = true;
            if (scanning_pattern)
                pattern += *ch;
            else
                replacement_ += *ch;
        } else if (*ch == DELIMITER) {
            if (scanning_pattern)
                scanning_pattern = false;
            else {
                if (++ch != subst_expr.end()) {
                    if (*ch == 'g') {
                        global_ = true;
                        ++ch;
                    }
                }
                if (ch != subst_expr.end())
                    throw std::runtime_error(
                        "in PerlCompatSubst::PerlCompatSubst: unexpected delimiter before end of "
                        "replacement text!");

                perl_compat_regexp_ = new PerlCompatRegExp(pattern, PerlCompatRegExp::OPTIMIZE_FOR_MULTIPLE_USE);
                return;
            }
        } else {
            if (scanning_pattern)
                pattern += *ch;
            else
                replacement_ += *ch;
        }
    }
}


PerlCompatSubst::PerlCompatSubst(const PerlCompatSubst &rhs)
    : subst_expr_(rhs.subst_expr_), replacement_(rhs.replacement_), global_(rhs.global_),
      perl_compat_regexp_(new PerlCompatRegExp(*rhs.perl_compat_regexp_)) {
}


const PerlCompatSubst &PerlCompatSubst::operator=(const PerlCompatSubst &rhs) {
    // Prevent self-assignment:
    if (likely(this != &rhs)) {
        subst_expr_ = rhs.subst_expr_;
        replacement_ = rhs.replacement_;
        global_ = rhs.global_;
        delete perl_compat_regexp_;
        perl_compat_regexp_ = new PerlCompatRegExp(*rhs.perl_compat_regexp_);
    }

    return *this;
}


std::string PerlCompatSubst::subst(const std::string &subject_text) const {
    std::string ret_val;
    size_t start_pos, length;
    if (not perl_compat_regexp_->match(subject_text, 0, &start_pos, &length))
        return subject_text;
    else
        ret_val = subject_text.substr(0, start_pos) + PerlCompatRegExp::GenerateReplacementText(*perl_compat_regexp_, replacement_);

    size_t last_end_pos(start_pos + length);
    while (global_ and perl_compat_regexp_->match(subject_text, start_pos + length, &start_pos, &length)) {
        ret_val += subject_text.substr(last_end_pos, start_pos - last_end_pos)
                   + PerlCompatRegExp::GenerateReplacementText(*perl_compat_regexp_, replacement_);
        last_end_pos = start_pos + length;
    }

    ret_val += subject_text.substr(last_end_pos);
    return ret_val;
}


void PerlCompatSubsts::addSubstExpression(const std::string &new_subst_expression) {
    try {
        const PerlCompatSubst new_perl_compat_subst(new_subst_expression);
        perl_compat_substs_.push_back(new_perl_compat_subst);
    } catch (...) {
        throw std::runtime_error("in PerlCompatSubsts::addSubstExpression: \"" + new_subst_expression
                                 + "\" is not a valid substitution expression!");
    }
}


std::string PerlCompatSubsts::subst(const std::string &subject_text) const {
    std::string processed_text(subject_text);

    for (std::list<PerlCompatSubst>::const_iterator perl_compat_subst(perl_compat_substs_.begin());
         perl_compat_subst != perl_compat_substs_.end(); ++perl_compat_subst)
    {
        const std::string old_text(processed_text);
        processed_text = perl_compat_subst->subst(old_text);
        if (subst_strategy_ == SHORT_CIRCUIT and processed_text != old_text)
            break;
    }

    return processed_text;
}
