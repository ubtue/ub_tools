/** \file   BibleUtil.h
 *  \brief  Declaration of bits related to our bible reference parser.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2014-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#ifndef BIBLE_UTIL_H
#define BIBLE_UTIL_H


#include <set>
#include <string>
#include <unordered_map>
#include <utility>


namespace BibleUtil {


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


void SplitIntoBookAndChaptersAndVerses(const std::string &bible_reference_candidate,
                                       std::string * const book_candidate,
                                       std::string * const chapters_and_verses_candidate);


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

    /** \brief Map from noncanonical bible book forms to the canonical ones.
     *  \return The mapped name or, if no mapping was found, the empty string.
     */
    std::string mapToCode(const std::string &bible_book_candidate, const bool verbose = false) const;
};


} // namespace BibleUtil


#endif // ifndef BIBLE_UTIL_H
