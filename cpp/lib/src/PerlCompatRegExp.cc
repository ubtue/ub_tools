/** \file    PerlCompatRegExp.cc
 *  \brief   Implementation of class PerlCompatRegExp, a wrapper around libpcre.
 *  \author  Dr. Johannes Ruscheinski
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
#include <langinfo.h>
#include "StringUtil.h"


/**
 *  These globals need to be made into functions to fix a problem with static initialization order. Without this, declaring a static
 *  PerlCompatRegExp in another file crashes due to these values being unitialized.
 */
inline const unsigned char *&PerlCompatRegExp::GetCharacterTable() {
    static const unsigned char *character_table = nullptr;
    return character_table;
}


inline std::string &PerlCompatRegExp::GetCodeset() {
    static std::string codeset(::nl_langinfo(CODESET));
    return codeset;
}


PerlCompatRegExp::PerlCompatRegExp(const std::string &pattern, const ProcessingMode processing_mode, const int options)
        : compiled_pattern_(nullptr), extra_pattern_info_(nullptr), substring_match_count_(0)
{
    resetPattern(pattern, processing_mode, options);
}


PerlCompatRegExp::PerlCompatRegExp(const PerlCompatRegExp &rhs)
        : compiled_pattern_(nullptr), extra_pattern_info_(nullptr), substring_match_count_(rhs.substring_match_count_)
{
    resetPattern(rhs.pattern_, rhs.processing_mode_, rhs.options_);
}


PerlCompatRegExp::~PerlCompatRegExp() {
    if (compiled_pattern_ != nullptr)
        ::pcre_free(compiled_pattern_);
    if (extra_pattern_info_ != nullptr)
        ::pcre_free(extra_pattern_info_);
}


const PerlCompatRegExp &PerlCompatRegExp::operator=(const PerlCompatRegExp &rhs) {
    // Prevent self-assignment:
    if (this != &rhs) {
        substring_match_count_ = rhs.substring_match_count_;
        resetPattern(rhs.pattern_, rhs.processing_mode_, rhs.options_);
    }

    return *this;
}


bool PerlCompatRegExp::safeResetPattern(const std::string &new_pattern, const ProcessingMode new_processing_mode,
                                        const int new_options)
{
    std::string error_message;
    return internalResetPattern(new_pattern, new_processing_mode, new_options, &error_message);
}


void PerlCompatRegExp::resetPattern(const std::string &new_pattern, const ProcessingMode new_processing_mode,
                                    const int new_options)
{
    std::string error_message;
    if (not internalResetPattern(new_pattern, new_processing_mode, new_options, &error_message))
        throw std::runtime_error("in PerlCompatRegExp::resetPattern: " + error_message);
}


bool PerlCompatRegExp::match(const std::string &subject_text, const size_t start_offset, size_t * const start_pos,
                             size_t * const length, const int options) const
{
    assert(compiled_pattern_ != nullptr);

    subject_text_ = subject_text;
    const int match_count(::pcre_exec(compiled_pattern_, extra_pattern_info_, subject_text_.c_str(),
                                      static_cast<int>(subject_text_.length()), static_cast<int>(start_offset),
                                      options, offset_vector_, OFFSET_VECTOR_SIZE));
    if (match_count > 1)
        substring_match_count_ = match_count - 1;
    else
        substring_match_count_ = 0;

    if (match_count < 1)
        return false;
    else {
        if (start_pos != nullptr)
            *start_pos = offset_vector_[0];
        if (length != nullptr)
            *length    = offset_vector_[1] - offset_vector_[0];
        return true;
    }
}


bool PerlCompatRegExp::Match(const std::string &pattern, const std::string &subject_text, const size_t start_offset,
                             size_t * const start_pos, size_t * const length, const int options)
{
    PerlCompatRegExp perl_compat_reg_exp(pattern, DONT_OPTIMIZE_FOR_MULTIPLE_USE, options);
    return perl_compat_reg_exp.match(subject_text, start_offset, start_pos, length);
}


bool PerlCompatRegExp::multiMatch(const std::string &subject_text, std::vector<std::string> * const matched_substrings,
                                  const int options) const
{
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
                                  std::vector<std::string> * const matched_substrings, const int options)
{
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
            }
            else {
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
        }
        else if (*ch == '\\')
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
    if (offset_vector_[index] == -1) {
        matched_substring->clear();
        return true;
    }

    *matched_substring = subject_text_.substr(offset_vector_[index], offset_vector_[index+1]-offset_vector_[index]);
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
    if (offset_vector_[index] == -1)
        return "";

    return subject_text_.substr(offset_vector_[index], offset_vector_[index+1]-offset_vector_[index]);
}


std::string PerlCompatRegExp::Subst(const std::string &pattern, const std::string &replacement, const std::string &subject_text,
                                    const bool global, const int options)
{
    PerlCompatRegExp reg_exp(pattern, global ? OPTIMIZE_FOR_MULTIPLE_USE : DONT_OPTIMIZE_FOR_MULTIPLE_USE, options);

    std::string ret_val;
    size_t start_pos, length;
    if (not reg_exp.match(subject_text, 0, &start_pos, &length))
        return subject_text;
    else
        ret_val = subject_text.substr(0, start_pos) + GenerateReplacementText(reg_exp, replacement);

    size_t last_end_pos(start_pos + length);
    while (global and reg_exp.match(subject_text, start_pos + length, &start_pos, &length)) {
        ret_val += subject_text.substr(last_end_pos, start_pos - last_end_pos)
            + GenerateReplacementText(reg_exp, replacement);
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
        ret_val += subject_text.substr(last_end_pos, start_pos - last_end_pos)
            + GenerateReplacementText(*this, replacement);
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
    const char *err_ptr;
    int err_offset;
    UpdateCharacterTable();
    pcre *compiled_pattern = ::pcre_compile(test_pattern.c_str(), 0, &err_ptr, &err_offset, PerlCompatRegExp::GetCharacterTable());
    if (compiled_pattern == nullptr)
        return false;
    else {
        ::pcre_free(compiled_pattern);
        return true;
    }
}


bool PerlCompatRegExp::internalResetPattern(const std::string &new_pattern, const ProcessingMode new_processing_mode,
                                            const int new_options, std::string * const error_message)
{
    pattern_         = new_pattern;
    processing_mode_ = new_processing_mode;
    options_         = new_options;

    if (compiled_pattern_ != nullptr)
        ::pcre_free(compiled_pattern_);
    if (extra_pattern_info_ != nullptr)
        ::pcre_free(extra_pattern_info_);

    const char *err_ptr;
    int err_offset;
    UpdateCharacterTable();
    compiled_pattern_ = ::pcre_compile(new_pattern.c_str(), new_options, &err_ptr, &err_offset, PerlCompatRegExp::GetCharacterTable());
    if (compiled_pattern_ == nullptr) {
        *error_message = StringUtil::Format("error \"%s\" while compiling pattern at offset %u (%s)!", err_ptr, err_offset,
                                            new_pattern.c_str() + err_offset);
        return false;
    }

#if 0 // The "studying" seems to currently be broken for the general case!
    if (new_processing_mode == OPTIMIZE_FOR_MULTIPLE_USE) {
        extra_pattern_info_ = ::pcre_study(compiled_pattern_, 0, &err_ptr);
        if (extra_pattern_info_ == nullptr) {
            *error_message = "error while optimizing the pattern \"";
            *error_message += pattern;
            *error_message += "\" (";
            *error_message += err_ptr;
            *error_message += ")!";
            return false;
        }
    }
#endif

    return true;
}


void PerlCompatRegExp::UpdateCharacterTable() {
    const std::string current_codeset(::nl_langinfo(CODESET));
    if (PerlCompatRegExp::GetCodeset() != current_codeset) {
        PerlCompatRegExp::GetCodeset() = current_codeset;
        if (GetCharacterTable() != nullptr)
            ::pcre_free(reinterpret_cast<void *>(const_cast<unsigned char *>(PerlCompatRegExp::GetCharacterTable())));
        PerlCompatRegExp::GetCharacterTable() = ::pcre_maketables();
    }
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
        : processing_mode_(rhs.processing_mode_), options_(rhs.options_), patterns_(rhs.patterns_), reg_exps_(rhs.reg_exps_)
{
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
        options_         = rhs.options_;
        patterns_        = rhs.patterns_;
        reg_exps_        = rhs.reg_exps_;
#endif
    }

    return *this;
}


bool PerlCompatRegExps::matchAny(const std::string &subject_text, const size_t start_offset, size_t * const start_pos,
                                 size_t * const length, const int options) const
{
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
                                   const int options) const
{
    matched_substrings->clear();

    size_t start_offset(0);
    size_t start_pos, match_length;
    while (matchAny(subject_text, start_offset, &start_pos, &match_length, options)) {
        matched_substrings->push_back(subject_text.substr(start_pos, match_length));
        start_offset = start_pos + match_length; // For the next match attempt.
    }

    return not matched_substrings->empty();
}


PerlCompatSubst::PerlCompatSubst(const std::string &subst_expr)
    : subst_expr_(subst_expr), global_(false), perl_compat_regexp_(nullptr)
{
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
                    throw std::runtime_error("in PerlCompatSubst::PerlCompatSubst: unexpected delimiter before end of "
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
      perl_compat_regexp_(new PerlCompatRegExp(*rhs.perl_compat_regexp_))
{
}


const PerlCompatSubst &PerlCompatSubst::operator=(const PerlCompatSubst &rhs) {
    // Prevent self-assignment:
    if (likely(this != &rhs)) {
        subst_expr_  = rhs.subst_expr_;
        replacement_ = rhs.replacement_;
        global_      = rhs.global_;
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
        ret_val = subject_text.substr(0, start_pos)
                  + PerlCompatRegExp::GenerateReplacementText(*perl_compat_regexp_, replacement_);

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
