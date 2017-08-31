/** \file   BibleReferenceParser.cc
 *  \brief  Implementation of a bible reference parser that generates numeric code ranges.
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
#include "BibleReferenceParser.h"
#include <cctype>
#include "Locale.h"
#include "StringUtil.h"
#include "util.h"


namespace BibleReferenceParser {


namespace {
    

// Checks whether the new reference comes strictly after already existing references.
bool NewReferenceIsCompatibleWithExistingReferences(
        const std::pair<std::string, std::string> &new_ref,
        const std::set<std::pair<std::string, std::string>> &existing_refs)
{
    for (const auto existing_ref : existing_refs) {
        if (new_ref.first <= existing_ref.second)
            return false;
    }

    return true;
}
        

bool IsNumericString(const std::string &s) {
    for (const char ch : s) {
        if (not StringUtil::IsDigit(ch))
            return false;
    }

    return true;
}


bool ReferenceIsWellFormed(const std::string &bib_ref_candidate) {
    if (bib_ref_candidate.length() != BOOK_CODE_LENGTH + MAX_CHAPTER_LENGTH + MAX_VERSE_LENGTH)
        return false;

    for (const char ch : bib_ref_candidate) {
        if (not StringUtil::IsDigit(ch))
            return false;
    }

    return true;
}


bool RangesAreWellFormed(const std::set<std::pair<std::string, std::string>> &ranges) {
    for (const auto &range : ranges) {
        if (not ReferenceIsWellFormed(range.first) or not ReferenceIsWellFormed(range.second))
            return false;
    }

    return true;
}


std::string RangesToString(const std::set<std::pair<std::string, std::string>> &ranges) {
    std::string ranges_as_string;
    for (const auto &range : ranges) {
        if (not ranges_as_string.empty())
            ranges_as_string += ", ";
        ranges_as_string += range.first + ":" + range.second;
    }

    return ranges_as_string;
}


bool ParseRefWithDot(const std::string &bib_ref_candidate, const std::string &book_code,
                     std::set<std::pair<std::string, std::string>> * const start_end)
{
    std::set<std::pair<std::string, std::string>> new_start_end;

    size_t comma_or_colon_pos(bib_ref_candidate.find(','));
    if (comma_or_colon_pos == std::string::npos)
        comma_or_colon_pos = bib_ref_candidate.find(':');
    if (comma_or_colon_pos == std::string::npos) // We must have a comma or a colon!
        return false;

    const std::string chapter(StringUtil::PadLeading(bib_ref_candidate.substr(0, comma_or_colon_pos),
                                                     MAX_CHAPTER_LENGTH, '0'));
    if (chapter.length() != MAX_CHAPTER_LENGTH or not IsNumericString(chapter))
        return false;

    const std::string rest(bib_ref_candidate.substr(comma_or_colon_pos + 1));
    bool in_verse1(true);
    std::string verse1, verse2;
    for (const char ch : rest) {
        if (StringUtil::IsDigit(ch)) {
            if (in_verse1) {
                verse1 += ch;
                if (verse1.length() > MAX_VERSE_LENGTH)
                    return false;
            } else {
                verse2 += ch;
                if (verse2.length() > MAX_VERSE_LENGTH)
                    return false;
            }
        } else if (ch == '.') {
            if (in_verse1) {
                if (verse1.empty())
                    return false;
                verse1 = StringUtil::PadLeading(verse1, MAX_VERSE_LENGTH, '0');
                const std::pair<std::string, std::string> new_reference(
                    std::make_pair(book_code + chapter + verse1, book_code + chapter + verse1));
                if (not NewReferenceIsCompatibleWithExistingReferences(new_reference, new_start_end))
                    return false;
                new_start_end.insert(new_reference);
                verse1.clear();
            } else {
                if (verse2.empty())
                    return false;
                verse2 = StringUtil::PadLeading(verse2, MAX_VERSE_LENGTH, '0');
                if (verse2 <= verse1)
                    return false;
                const std::pair<std::string, std::string> new_reference(
                    std::make_pair(book_code + chapter + verse1, book_code + chapter + verse2));
                if (not NewReferenceIsCompatibleWithExistingReferences(new_reference, new_start_end))
                    return false;
                new_start_end.insert(new_reference);
                verse1.clear();
                verse2.clear();
                in_verse1 = true;
            }
        } else if (ch == '-') {
            if (not in_verse1 or verse1.empty())
                return false;
            verse1 = StringUtil::PadLeading(verse1, MAX_VERSE_LENGTH, '0');
            in_verse1 = false;
        } else if (islower(ch)) {
            if (in_verse1) {
                if (verse1.empty())
                    return false;
                verse1 = StringUtil::PadLeading(verse1, MAX_VERSE_LENGTH, '0');
            } else {
                if (verse2.empty())
                    return false;
                verse2 = StringUtil::PadLeading(verse2, MAX_VERSE_LENGTH, '0');
            }
        } else
            return false;
    }

    if (in_verse1) {
        if (verse1.empty())
            return false;
        verse1 = StringUtil::PadLeading(verse1, MAX_VERSE_LENGTH, '0');
        const std::pair<std::string, std::string> new_reference(
            std::make_pair(book_code + chapter + verse1, book_code + chapter + verse1));
        if (not NewReferenceIsCompatibleWithExistingReferences(new_reference, new_start_end))
            return false;
        new_start_end.insert(new_reference);
    } else {
        if (verse2.empty())
            return false;
        verse2 = StringUtil::PadLeading(verse2, MAX_VERSE_LENGTH, '0');
        if (verse2 <= verse1)
            return false;
        const std::pair<std::string, std::string> new_reference(
            std::make_pair(book_code + chapter + verse1, book_code + chapter + verse2));
        if (not NewReferenceIsCompatibleWithExistingReferences(new_reference, new_start_end))
            return false;
        new_start_end.insert(new_reference);
    }

    start_end->insert(new_start_end.cbegin(), new_start_end.cend());
    return true;
}


enum State { INITIAL, CHAPTER1, CHAPTER2, VERSE1, VERSE2 };
    

} // unnamed namespace


bool ParseBibleReference(std::string bib_ref_candidate, const std::string &book_code,
                         std::set<std::pair<std::string, std::string>> * const start_end)
{
    StringUtil::RemoveChars(" \t", &bib_ref_candidate); // Remove embedded spaces and tabs.
    if (bib_ref_candidate.empty()) {
        start_end->insert(std::make_pair(book_code + std::string(MAX_CHAPTER_LENGTH + MAX_VERSE_LENGTH, '0'),
                                         book_code + std::string(MAX_CHAPTER_LENGTH + MAX_VERSE_LENGTH, '9')));
        return true;
    }

    const Locale c_locale("C", LC_ALL); // We don't want islower() to accept characters w/ diacritical marks!

    if (bib_ref_candidate.find('.') != std::string::npos) {
        const bool parse_succeeded(ParseRefWithDot(bib_ref_candidate, book_code, start_end));
        if (parse_succeeded and not RangesAreWellFormed(*start_end))
            Error("Bad ranges (" + RangesToString(*start_end) + ") were generated in ParseBibleReference! (1)");
        return parse_succeeded;
    }

    State state(INITIAL);
    std::string accumulator, chapter1, verse1, chapter2, verse2;
    for (auto ch(bib_ref_candidate.cbegin()); ch != bib_ref_candidate.cend(); ++ch) {
        switch (state) {
        case INITIAL:
            if (StringUtil::IsDigit(*ch)) {
                accumulator += *ch;
                state = CHAPTER1;
            } else
                return false;
            break;
        case CHAPTER1:
            if (StringUtil::IsDigit(*ch)) {
                accumulator += *ch;
                if (accumulator.length() > MAX_CHAPTER_LENGTH)
                    return false;
            } else if (*ch == '-') {
                chapter1 = StringUtil::PadLeading(accumulator, MAX_CHAPTER_LENGTH, '0');
                accumulator.clear();
                state = CHAPTER2;
            } else if (*ch == ',' or *ch == ':') {
                chapter1 = StringUtil::PadLeading(accumulator, MAX_CHAPTER_LENGTH, '0');
                accumulator.clear();
                state = VERSE1;
            } else
                return false;
            break;
        case VERSE1:
            if (StringUtil::IsDigit(*ch)) {
                accumulator += *ch;
                if (accumulator.length() > MAX_VERSE_LENGTH)
                    return false;
            } else if (islower(*ch)) {
                if (accumulator.empty())
                    return false;
                accumulator = StringUtil::PadLeading(accumulator, MAX_VERSE_LENGTH, '0');
                // Ignore this non-standardised letter!
            } else if (*ch == '-') {
                if (accumulator.empty())
                    return false;
                verse1 = StringUtil::PadLeading(accumulator, MAX_VERSE_LENGTH, '0');
                accumulator.clear();

                // We need to differentiate between a verse vs. a chapter-hyphen:
                const std::string remainder(bib_ref_candidate.substr(ch - bib_ref_candidate.cbegin()));
                if (remainder.find(',') == std::string::npos and remainder.find(':') == std::string::npos) // => We have a verse hyphen!
                    state = VERSE2;
                else
                    state = CHAPTER2;
            } else
                return false;
            break;
        case CHAPTER2:
            if (StringUtil::IsDigit(*ch)) {
                accumulator += *ch;
                if (accumulator.length() > MAX_CHAPTER_LENGTH)
                    return false;
            } else if (*ch == ',' or *ch == ':') {
                if (accumulator.empty())
                    return false;
                chapter2 = StringUtil::PadLeading(accumulator, MAX_CHAPTER_LENGTH, '0');
                accumulator.clear();
                state = VERSE2;
            } else
                return false;
            break;
        case VERSE2:
            if (StringUtil::IsDigit(*ch)) {
                accumulator += *ch;
                if (accumulator.length() > MAX_VERSE_LENGTH)
                    return false;
            } else if (islower(*ch)) {
                if (accumulator.empty())
                    return false;
                accumulator = StringUtil::PadLeading(accumulator, MAX_VERSE_LENGTH, '0');
                // Ignore this non-standardised letter!
            } else
                return false;
            break;
        }
    }

    if (state == CHAPTER1) {
        chapter1 = book_code + StringUtil::PadLeading(accumulator, MAX_CHAPTER_LENGTH, '0');
        start_end->insert(std::make_pair(chapter1 + std::string(MAX_VERSE_LENGTH, '0'),
                                         chapter1 + std::string(MAX_VERSE_LENGTH, '9')));
    } else if (state == CHAPTER2) {
        if (accumulator.empty())
            return false;
        verse1 = StringUtil::PadLeading(verse1, MAX_VERSE_LENGTH, '0');
        verse2 = verse2.empty() ? std::string(MAX_VERSE_LENGTH, '9')
                                : StringUtil::PadLeading(verse2, MAX_VERSE_LENGTH, '0');
        const std::string chapter1_verse1(chapter1 + verse1);
        const std::string chapter2_verse2(StringUtil::PadLeading(accumulator, MAX_CHAPTER_LENGTH, '0') + verse2);
        if (chapter2_verse2 <= chapter1_verse1)
            return false;
        start_end->insert(std::make_pair(book_code + chapter1_verse1, book_code + chapter2_verse2));
    } else if (state == VERSE1) {
        verse1 = StringUtil::PadLeading(accumulator, MAX_VERSE_LENGTH, '0');
        accumulator = book_code + chapter1 + verse1;
        start_end->insert(std::make_pair(accumulator, accumulator));
    } else if (state == VERSE2) {
        if (accumulator.empty())
            return false;
        verse1 = StringUtil::PadLeading(verse1, MAX_VERSE_LENGTH, '0');
        verse2 = StringUtil::PadLeading(accumulator, MAX_VERSE_LENGTH, '0');
        const std::string start(book_code + chapter1 + verse1);
        const std::string end(book_code + (chapter2.empty() ? chapter1 : chapter2) + verse2);
        if (end <= start)
            return false;
        start_end->insert(std::make_pair(start, end));
    }

    if (not RangesAreWellFormed(*start_end))
        Error("Bad ranges (" + RangesToString(*start_end) + ") were generated in ParseBibleReference! (2)");
    return true;
}


bool CanParseBibleReference(const std::string &bib_ref_candidate) {
    std::set<std::pair<std::string, std::string>> start_end;
    return ParseBibleReference(bib_ref_candidate, std::string(BOOK_CODE_LENGTH, '0'), &start_end);
}


} // namespace BibleReferenceParser
