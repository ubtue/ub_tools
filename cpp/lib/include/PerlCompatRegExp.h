/** \file    PerlCompatRegExp.h
 *  \brief   Declaration of class PerlCompatRegExp, a wrapper around libpcre.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2003-2008 Project iVia.
 *  Copyright 2003-2008 The Regents of The University of California.
 *  Copyright 2006 Dr. Johannes Ruscheinski
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

#ifndef PERL_COMPAT_REG_EXP_H
#define PERL_COMPAT_REG_EXP_H


#include <list>
#include <stdexcept>
#include <string>
#include <vector>
#include <pcre.h>


/** \class  PerlCompatRegExp
 *  \brief  A convenience wrapper around the pcre(3) API.
 */
class PerlCompatRegExp {
        mutable std::string subject_text_;      // The "subject text" to be processed.
        pcre *compiled_pattern_;
        pcre_extra *extra_pattern_info_;
        mutable unsigned substring_match_count_;
        std::string pattern_;
public:
        enum ProcessingMode { DONT_OPTIMIZE_FOR_MULTIPLE_USE, OPTIMIZE_FOR_MULTIPLE_USE };
private:
        ProcessingMode processing_mode_;
        unsigned options_;
        static const int OFFSET_VECTOR_SIZE = 300; // Must be a multiple of 3!
        mutable int offset_vector_[OFFSET_VECTOR_SIZE];
        static const unsigned char *&GetCharacterTable();
        static std::string &GetCodeset();
public:
        PerlCompatRegExp(): compiled_pattern_(nullptr), extra_pattern_info_(nullptr) { }
        PerlCompatRegExp(const PerlCompatRegExp &rhs);

        /** \note  See pcre_compile(3) for which options are available.
         */
        PerlCompatRegExp(const std::string &pattern, const ProcessingMode processing_mode = DONT_OPTIMIZE_FOR_MULTIPLE_USE, const int options = 0);

        ~PerlCompatRegExp();

        const PerlCompatRegExp &operator=(const PerlCompatRegExp &rhs);

        bool empty() const { return pattern_.empty(); }

        /** Specifies a new pattern to match against. */
        bool safeResetPattern(const std::string &new_pattern, const ProcessingMode new_processing_mode = DONT_OPTIMIZE_FOR_MULTIPLE_USE,
                              const int new_options = 0);

        /** Specifies a new pattern to match against.  (Throws exceptions for bad patterns. */
        void resetPattern(const std::string &new_pattern, const ProcessingMode new_processing_mode = DONT_OPTIMIZE_FOR_MULTIPLE_USE,
                          const int new_options = 0);

        /** Returns the pattern that was passed into the constructor. */
        const std::string &getPattern() const { return pattern_; }

        // Necessary for putting these into maps
        bool operator<(const PerlCompatRegExp &rhs) { return pattern_ < rhs.pattern_; }

        /** \brief  Match "subject_text " against this regexp.
         *  \param  subject_text  The string to match against.  May legitimately contain zero bytes!
         *  \param  start_offset  Where to start scanning for a match.
         *  \param  start_pos     If we have a match this is the (zero-based) index of the start of the match.
         *  \param  length        If we have a match this is the length of the matching section of "subject_string".
         *  \param  options       See pcre_compile(3) for which options are available.
         *  \return True if "subject_text" is a match for the pattern, else false.  Please note that a successful return does not imply that there are any
         *          substring matches.
         */
        bool match(const std::string &subject_text, const size_t start_offset, size_t * const start_pos, size_t * const length,
                   const int options = 0) const;

        /** \note  Takes the same arguments as "match" in addition to "pattern" but requires no PerlCompatRegExp object.
         *         The intended use of this function as for one-shot matching.  If you want to repeatedly match against
         *         the same pattern it is probably better to create a PerlCompatRegExp object with a processing mode of
         *         OPTIMIZE_FOR_MULTIPLE_USE.
         */
        static bool Match(const std::string &pattern, const std::string &subject_text, const size_t start_offset, size_t * const start_pos,
                          size_t * const length, const int options = 0);

        /** \brief  Match "subject_text " against this regexp.
         *  \param  subject_text  The string to match against.  May legitimately contain zero bytes!
         *  \param  options       See pcre_compile(3) for which options are available.
         *  \return True if "subject_text" is a match for the pattern, else false.  Please note that a successful return does not imply that there are any
         *          substring matches.
         */
        bool match(const std::string &subject_text, const int options = 0) const
            { return match(subject_text, 0, nullptr, nullptr, options); }

        /** \note  Takes the same arguments as "match()" in addition to "pattern" but requires no PerlCompatRegExp object.  The intended use of this
         *         function as for one-shot matching.  If you want to repeatedly match against the same pattern it is probably better to create a
         *         PerlCompatRegExp object with a processing mode of OPTIMIZE_FOR_MULTIPLE_USE.
         *  \param  pattern       The regexp to match against.
         *  \param  subject_text  The string to match against.  May legitimately contain zero bytes!
         *  \param  options       See pcre_compile(3) for which options are available.
         *  \return True if "subject_text" matched the regexp "pattern", otherwise false.
         */
        static bool Match(const std::string &pattern, const std::string &subject_text, int options = 0);

        /** \brief  Attempts to extract all matches of the pattern that has been specified via the constructor.
         *  \param  subject_text        The string to match against.  May legitimately contain zero bytes!
         *  \param  matched_substrings  Upon a successful
         *  \param  options             See pcre_compile(3) for which options are available.
         *  \return True if at least one match was found, otherwise false.
         */
        bool multiMatch(const std::string &subject_text, std::vector<std::string> * const matched_substrings, const int options = 0) const;

        /** \brief  Attempts to extract all matches of the pattern that has been specified via the constructor.
         *  \note   Takes the same arguments as "multiMatch()" in addition to "pattern" but requires no PerlCompatRegExp object.  The intended use of this
         *          function as for one-shot matching.  If you want to repeatedly match against the same pattern it is probably better to create a
         *          PerlCompatRegExp object with a processing mode of OPTIMIZE_FOR_MULTIPLE_USE.
         *  \param  pattern             The regexp to match against.
         *  \param  subject_text        The string to match against.  May legitimately contain zero bytes!
         *  \param  matched_substrings  Upon a successful
         *  \param  options             See pcre_compile(3) for which options are available.
         *  \return True if at least one match was found, otherwise false.
         */
        static bool MultiMatch(const std::string &pattern, const std::string &subject_text, std::vector<std::string> * const matched_substrings, const int options = 0);

        unsigned getSubstringMatchCount() const { return substring_match_count_; }

        /** \brief  Used to retrieve matched substrings.
         *  \param  index              Stands for the n-th substring pattern and should be between 1 and the return result of
         *                             getSubstringMatchCount() inclusive.
         *  \param  matched_substring  Where to store the matched substring.  Caution: based on the options passed into "match", this may
         *                             also be the empty string (in fact it's the default), even if this function returns true!
         *  \return True if the requested substring pattern identified by the index resulted in a match (could be empty!). else false.
         */
        bool getMatchedSubstring(unsigned index, std::string * const matched_substring) const;

        /** Similar to above but returns the string. Returns "" if the substring was not extracted. */
        std::string getMatchedSubstring(unsigned index) const;

        /** \brief  Provides Perl style text substitutions with back references.
         *  \param  pattern       A Perl regular expression.
         *  \param  replacement   The replacement text which may contain numbered backreferences of the form "$N" where N is an integer
         *                        starting at 1 and refers to the N-th matched substring of the pattern.
         *  \param  subject_text  The text that the pattern gets applied to.
         *  \param  global        If true perform a global search-and-replace.
         *  \param  options       See pcre_compile(3) for which options are available.
         *  \return The substituted string.
         */
        static std::string Subst(const std::string &pattern, const std::string &replacement, const std::string &subject_text,
                                 const bool global = true, const int options = 0);

        /** \brief  Provides Perl style text substitutions with back references.
         *  \param  replacement   The replacement text which may contain numbered backreferences of the form "$N" where N is an integer
         *                        starting at 1 and refers to the N-th matched substring of the pattern.
         *  \param  subject_text  The text that the pattern gets applied to.
         *  \param  global        If true perform a global search-and-replace.
         *  \note                 This is a member function so the same regex can be used over and over again without the overhead of
         *                        compiling it.
         *  \return The substituted string.
         */
        std::string subst(const std::string &replacement, const std::string &subject_text, const bool global = true) const;


        /** \brief   Provides Perl style text substitutions with back references.
         *  \param   subst_expression  A Perl substitution expression expression of the form "/pattern/replacement/" where pattern is a Perl
         *                             regular expression and "replacement" may contain numbered backreferences of the form "$N" where N is
         *                             an integer starting at 1 and refers to the N-th matched substring of the pattern.  Note that any other
         *                             character besides slash can be used as a delimiter.  To remove the special meaning of a character you
         *                             must escape it with a backslash.
         *  \param   subject_text      The text that the pattern gets applied to.
         *  \param   options           See pcre_compile(3) for which options are available.
         *  \return  The substituted string.
         */
        static std::string Subst(const std::string &subst_expression, const std::string &subject_text, const int options = 0);

        /**  \brief   Tests whether "test_pattern" is a valid Perl-compatible regexp or not.
         *   \param   test_pattern  The string we want to test for validity as a Perl-compatible regexp.
         *   \return  'True' if "test_pattern" is a valid Perl-compatible regexp, otherwise 'false'.
         */
        static bool IsValid(const std::string &test_pattern);


        /* \brief  Generates the replacement text for a single substitution.  The information about the match is stored in the internal state of "reg_exp".
         *         This includes processing of backreferences like "$1", "$2", etc.
         */
        static std::string GenerateReplacementText(const PerlCompatRegExp &reg_exp, const std::string &replacement);

        static bool IsMetacharacter(const char ch);

        /** \brief  Places a slash in front of all metacharacters. */
        static std::string EscapeMetacharacters(const std::string &s);
private:
        /** Specifies a new pattern to match against. */
        bool internalResetPattern(const std::string &new_pattern, const ProcessingMode new_processing_mode, const int new_options,
                                  std::string * const error_message);

        /** Makes "character_table_" current.  Must be called before pcre_compile(3). */
        static void UpdateCharacterTable();
};


/** \class  PerlCompatRegExps
 *  \brief  Allows matching against a list of patterns.
 */
class PerlCompatRegExps {
        PerlCompatRegExp::ProcessingMode processing_mode_;
        int options_;
        std::list<std::string> patterns_;
        std::list<PerlCompatRegExp> reg_exps_;
public:
        typedef std::list<std::string>::const_iterator const_iterator;
public:
        PerlCompatRegExps(const PerlCompatRegExp::ProcessingMode processing_mode = PerlCompatRegExp::DONT_OPTIMIZE_FOR_MULTIPLE_USE, const int options = 0)
                : processing_mode_(processing_mode), options_(options) { }
        PerlCompatRegExps(const PerlCompatRegExps &rhs);

        /** Returns how many patterns the current object represents. */
        size_t size() const { return patterns_.size(); }

        /** Returns "true" if all patterns are empty otherwise returns "false". */
        bool empty() const;

        /** Remove all patterns. */
        void clear() { reg_exps_.clear(); }

        const PerlCompatRegExps &operator=(const PerlCompatRegExps &rhs);

        /** Returns a const_iterator to the first pattern */
        const_iterator begin() { return patterns_.begin(); }

        const_iterator end() { return patterns_.end(); }

        /** \brief  Add a new pattern to match against.
         *  \param  new_pattern  The new pattern.
         */
        void addPattern(const std::string &new_pattern) throw(std::exception)
                {
                        patterns_.push_back(new_pattern);
                        reg_exps_.push_back(PerlCompatRegExp(new_pattern, processing_mode_, options_));
                }

        /** \brief  Match "subject_text " against all regexps.
         *  \param  subject_text  The string to match against.  May legitimately contain zero bytes!
         *  \param  start_offset  Where to start scanning for a match.
         *  \param  start_pos     If we have a match this is the (zero-based) index of the start of the match.
         *  \param  length        If we have a match this is the length of the matching section of "subject_string".
         *  \param  options       See pcre_compile(3) for which options are available.
         *  \return True if "subject_text" is a match for at least one of the patterns, else false.
         *          Please note that a successful return does not imply that there are any substring matches.
         *  \note   See the description of pcre_exec in pcre(3) for which options are available.
         */
        bool matchAny(const std::string &subject_text, const size_t start_offset, size_t * const start_pos, size_t * const length, const int options = 0)
                const;

        /** \brief  Match "subject_text " against all regexps.
         *  \param  subject_text  The string to match against.  May legitimately contain zero bytes!
         *  \param  options       See pcre_compile(3) for which options are available.
         *  \return True if "subject_text" is a match for at least one of the pattern, else false.
         *          Please note that a successful return does not imply that there are any substring
         *          matches.
         *  \note   See the description of pcre_exec in pcre(3) for which options are available.
         */
        bool matchAny(const std::string &subject_text, const int options = 0) const;

        /** \brief  Attempts to extract all matches of the pattern that has been specified via calls to addPattern().
         *  \param  subject_text        The string to match against.  May legitimately contain zero bytes!
         *  \param  matched_substrings  Upon a successful
         *  \param  options             See pcre_compile(3) for which options are available.
         *  \return True if at least one match was found, otherwise false.
         */
        bool multiMatch(const std::string &subject_text, std::vector<std::string> * const matched_substrings, const int options = 0) const;

        /** Returns the patterns that were passed into addPattern(). */
        const std::list<std::string> &getPatterns() const { return patterns_; }
};


class PerlCompatSubst {
        std::string subst_expr_;
        std::string replacement_;
        bool global_;
        PerlCompatRegExp *perl_compat_regexp_;
public:
        /** Constructs an object from expressions like "/fred/bob/g". */
        explicit PerlCompatSubst(const std::string &subst_expr);

        PerlCompatSubst(const PerlCompatSubst &rhs);
        const PerlCompatSubst &operator=(const PerlCompatSubst &rhs);

        ~PerlCompatSubst() { delete perl_compat_regexp_; }

        const std::string &getSubstExpression() const { return subst_expr_; }

        /** Replaces substrings of "subject_text" according to the expression passed into the current objects
            constructor. */
        std::string subst(const std::string &subject_text) const;
};


class PerlCompatSubsts {
public:
        enum SubstStrategy {
                APPLY_ALL,    //< Apply all substitution expressions.
                SHORT_CIRCUIT //< Stop after the first subst expression that mutates.
        };
private:
        SubstStrategy subst_strategy_;
        std::list<PerlCompatSubst> perl_compat_substs_;
public:
        explicit PerlCompatSubsts(const SubstStrategy subst_strategy = APPLY_ALL): subst_strategy_(subst_strategy) { }
        explicit PerlCompatSubsts(const std::string &subst_expression, const SubstStrategy subst_strategy = APPLY_ALL): subst_strategy_(subst_strategy)
                { addSubstExpression(subst_expression); }

        void addSubstExpression(const std::string &new_subst_expression);

        /** Replaces substrings of "subject_text" according to the expression passed into the current objects
            constructor. */
        std::string subst(const std::string &subject_text) const;
};


#endif // ifndef PERL_COMPAT_REG_EXP_H
