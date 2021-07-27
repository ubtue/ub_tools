/** \file    RangeUtil.h
 *  \brief   Declarations of miscellaneous hierachical range search functions.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
 *  Copyright 2014-2021 Universitätsbibliothek Tübingen.
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


#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


namespace RangeUtil {


static const std::string BIB_REF_RANGE_TAG("BIR");


const unsigned BOOK_CODE_LENGTH(2);
const unsigned MAX_CHAPTER_LENGTH(3);
const unsigned MAX_VERSE_LENGTH(3);


/** \brief Parses bible references into ranges.
 *  \param bib_ref_candidate  The hopefully syntactically correct bible chapter(s)/verse(s) reference(s).
 *  \param book_code          A two-digit code indicating the book of the bible.  Will be prepended to any
 *                            recognised chapter/verse references returned in "start_end".
 *  \param start_end          The successfully extracted bible ranges.
 *  \return If the parse succeded or not.
 */
bool ParseBibleReference(std::string bib_ref_candidate, const std::string &book_code,
                         std::set<std::pair<std::string, std::string>> * const start_end);


/** \brief Tests the validity of a possible chapter/verse reference. */
bool CanParseBibleReference(const std::string &bib_ref_candidate);


// Parses an OR'ed together sequence of simple bible references.
bool SplitIntoBooksAndChaptersAndVerses(const std::string &bible_reference_query,
                                        std::vector<std::string> * const book_candidates,
                                        std::vector<std::string> * const chapters_and_verses_candidates);


class BibleBookCanoniser {
    std::unordered_map<std::string, std::string> books_of_the_bible_to_canonical_form_map_;
public:
    explicit BibleBookCanoniser(const std::string &books_of_the_bible_to_canonical_form_map_filename);

    /** \brief Map from noncanonical bible book forms to the canonical ones.
     *  \return The mapped name or, if no mapping was found, "bible_book_candidate".
     */
    std::string canonise(const std::string &bible_book_candidate, const bool verbose = false) const;
};


class BibleBookToCodeMapper {
    std::unordered_map<std::string, std::string> bible_books_to_codes_map_;
public:
    explicit BibleBookToCodeMapper(const std::string &books_of_the_bible_to_code_map_filename);

    /** \brief Map from canonical forms to codes.
     *  \return The mapped name or, if no mapping was found, the empty string.
     */
    std::string mapToCode(const std::string &bible_book_candidate, const bool verbose = false) const;
};


class BibleAliasMapper {
    std::unordered_map<std::string, std::string> aliases_to_canonical_forms_map_;
public:
    explicit BibleAliasMapper(const std::string &bible_aliases_map_filename);

    /** \brief Map noncanonical forms to the canonical ones.
     *  \return The canonical mapped version if found, else the original "bible_reference_candidate".
     */
    std::string map(const std::string &bible_reference_candidate, const bool verbose = false) const;
};


bool ParseCanonLawRanges(const std::string &ranges, unsigned * const range_start, unsigned * const range_end);


/**
 * \param special_case_centuries If true, ranges like 1800-1900 will be interpreted as 1800-1899.  This only
 *                               happens if both years of a range are evenly divisible by 100.
 * \return True if "text" contained a valid time range, o/w false.
 */
bool ConvertTextToTimeRange(std::string text, std::string * const range, const bool special_case_centuries = false);


std::string ConvertTimeRangeToText(const std::string &range);


/** \brief Special processing for 2 Esdras, 5 Esra and 6 Esra
 *
 *  The following rules are currently implemented:
 *
 *  1) if "book" is "5esra" we replace it with "4esra", if "chapters_and_verses" is empty we insert "1-2" o/w we keep it as is
 *  2) if "book" is "6esra" we set it to "4esra", if "chapters_and_verses" is empty we insert "15-16" o/w we abort w/ an error message
 *  3) if "book" is "2esdras" we set it to "4esra".  We don't modify "chapters_and_verses".
 *  4) If "book" is none of the above, neither "book " nor "chapters_and_verses" will be modified
 */
void EsraSpecialProcessing(std::string * const book, std::vector<std::string> * const chapters_and_verses);


/** \brief Convert one or more ranges to a Solr date-range query.
 *  \param ranges_str a list of single-space separated ranges of the form XXX:YYY where XXX and YYY are unsigned integers.
 */
std::string ConvertToDatesQuery(const std::string &ranges_str);


} // namespace RangeUtil
