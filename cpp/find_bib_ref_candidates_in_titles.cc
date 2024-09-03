/** \file    find_bib_ref_candidates_in_titles.cc
 *  \brief   A tool for finding potential bible references in titles in MARC-21 datasets.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2019-2020, Library of the University of Tübingen

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <cstdlib>
#include <cstring>
#include "FileUtil.h"
#include "MARC.h"
#include "MapUtil.h"
#include "RangeUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("ix_theo_titles ix_theo_norm bib_ref_candidates_list");
}


std::unordered_set<std::string> LoadPPNExclusionSet() {
    std::unordered_set<std::string> excluded_ppns;

    const auto input(FileUtil::OpenInputFileOrDie(UBTools::GetTuelibPath() + "bib_ref_candidates_in_titles.exclusion_list"));
    while (not input->eof()) {
        const std::string line(input->getline());
        if (line.empty())
            continue;

        excluded_ppns.emplace(StringUtil::TrimWhite(line));
    }

    return excluded_ppns;
}


void LoadBibleOrderMap(File * const input, std::unordered_map<std::string, std::string> * const books_of_the_bible_to_code_map) {
    LOG_INFO("Started loading of the bible-order map.");

    unsigned line_no(0);
    while (not input->eof()) {
        const std::string line(input->getline());
        if (line.empty())
            continue;
        ++line_no;

        const size_t equal_pos(line.find('='));
        if (equal_pos == std::string::npos)
            LOG_ERROR("malformed line #" + std::to_string(line_no) + " in the bible-order map file!");
        (*books_of_the_bible_to_code_map)[TextUtil::UTF8ToLower(line.substr(0, equal_pos))] = line.substr(equal_pos + 1);
    }

    LOG_INFO("Loaded " + std::to_string(line_no) + " entries from the bible-order map file.");
}


/* Pericopes are found in 130$a if there are also bible references in the 430 field. You should therefore
   only call this after ascertaining that one or more 430 fields contain a bible reference. */
bool FindPericopes(const MARC::Record &record, const std::set<std::pair<std::string, std::string>> &ranges,
                   std::unordered_multimap<std::string, std::string> * const pericopes_to_ranges_map) {
    std::vector<std::string> pericopes;
    const auto field_130(record.findTag("130"));
    if (field_130 != record.end()) {
        const auto subfields(field_130->getSubfields());
        std::string a_subfield(subfields.getFirstSubfieldWithCode('a'));
        TextUtil::UTF8ToLower(&a_subfield);
        TextUtil::CollapseAndTrimWhitespace(&a_subfield);
        pericopes.push_back(a_subfield);
    }

    if (pericopes.empty())
        return false;

    for (const auto &pericope : pericopes) {
        for (const auto &range : ranges)
            pericopes_to_ranges_map->emplace(pericope, range.first + ":" + range.second);
    }

    return true;
}


inline bool IsValidSingleDigitArabicOrdinal(const std::string &ordinal_candidate) {
    return ordinal_candidate.length() == 2 and StringUtil::IsDigit(ordinal_candidate[0]) and ordinal_candidate[1] == '.';
}


/** We expect 1 or 2 $n subfields.  The case of having only one is trivial as there is nothing to sort.
 *  In the case of 2 subfields we expect that one of them contains an arabic ordinal number in one of the
 *  two subfield.  In that case we sort the two subfields such that the one with the ordinal comes first.
 */
bool OrderNSubfields(std::vector<std::string> * const n_subfield_values) {
    if (n_subfield_values->size() < 2)
        return true;

    if (IsValidSingleDigitArabicOrdinal((*n_subfield_values)[0]))
        return true;

    if (not IsValidSingleDigitArabicOrdinal((*n_subfield_values)[1]))
        return false; // Expected a period as part of one of the two values!
    (*n_subfield_values)[0].swap((*n_subfield_values)[1]);
    return true;
}


// Populates "numbered_books" based on "book_name_candidate" and the 0th entry in "*n_subfield_values".
// If there were one or more arabic numerals in "(*n_subfield_values)[0]" this entry will also be removed.
void CreateNumberedBooks(const std::string &book_name_candidate, std::vector<std::string> * const n_subfield_values,
                         std::vector<std::string> * const numbered_books) {
    numbered_books->clear();

    if (n_subfield_values->empty()) {
        numbered_books->emplace_back(book_name_candidate);
        return;
    }

    if (IsValidSingleDigitArabicOrdinal(n_subfield_values->front())) {
        numbered_books->emplace_back(std::string(1, n_subfield_values->front()[0]) + book_name_candidate);
        n_subfield_values->erase(n_subfield_values->begin());
        return;
    }

    if (n_subfield_values->front() == "1. 2." or n_subfield_values->front() == "1.-2.") {
        numbered_books->emplace_back("1" + book_name_candidate);
        numbered_books->emplace_back("2" + book_name_candidate);
        n_subfield_values->erase(n_subfield_values->begin());
        return;
    }

    if (n_subfield_values->front() == "2.-3.") {
        numbered_books->emplace_back("2" + book_name_candidate);
        numbered_books->emplace_back("3" + book_name_candidate);
        n_subfield_values->erase(n_subfield_values->begin());
        return;
    }

    if (n_subfield_values->front() == "1.-3.") {
        numbered_books->emplace_back("1" + book_name_candidate);
        numbered_books->emplace_back("2" + book_name_candidate);
        numbered_books->emplace_back("3" + book_name_candidate);
        n_subfield_values->erase(n_subfield_values->begin());
        return;
    }

    numbered_books->emplace_back(book_name_candidate);
}


bool HaveBibleBookCodes(const std::vector<std::string> &book_name_candidates,
                        const std::unordered_map<std::string, std::string> &bible_book_to_code_map) {
    for (const auto &book_name_candidate : book_name_candidates) {
        if (bible_book_to_code_map.find(book_name_candidate) == bible_book_to_code_map.cend())
            return false;
    }

    return true;
}


bool ConvertBooksToBookCodes(const std::vector<std::string> &books,
                             const std::unordered_map<std::string, std::string> &bible_book_to_code_map,
                             std::vector<std::string> * const book_codes) {
    book_codes->clear();

    for (const auto &book : books) {
        const auto book_and_code(bible_book_to_code_map.find(book));
        if (unlikely(book_and_code == bible_book_to_code_map.cend()))
            return false;
        book_codes->emplace_back(book_and_code->second);
    }

    return true;
}


// Extract the lowercase bible book names from "bible_book_to_code_map".
void ExtractBooksOfTheBible(const std::unordered_map<std::string, std::string> &bible_book_to_code_map,
                            std::unordered_set<std::string> * const books_of_the_bible) {
    books_of_the_bible->clear();

    for (const auto &book_and_code : bible_book_to_code_map) {
        books_of_the_bible->insert(StringUtil::IsDigit(book_and_code.first[0]) ? book_and_code.first.substr(1) : book_and_code.first);
    }
}


const std::map<std::string, std::string> book_alias_map{ { "jesus sirach", "sirach" },
                                                         { "offenbarung des johannes", "offenbarungdesjohannes" } };


static unsigned unknown_book_count;


/*  Possible fields containing bible references which will be extracted as bible ranges are 130 and 430
    (specified by "field_tag").  If one of these fields contains a bible reference, the subfield "a" must
    contain the text "Bible".  Subfield "p" must contain the name of a book of the bible.  Book ordinals and
    chapter and verse indicators would be in one or two "n" subfields.
 */
bool GetBibleRanges(const std::string &field_tag, const MARC::Record &record, const std::unordered_set<std::string> &books_of_the_bible,
                    const std::unordered_map<std::string, std::string> &bible_book_to_code_map,
                    std::set<std::pair<std::string, std::string>> * const ranges) {
    ranges->clear();

    bool found_at_least_one(false);
    for (const auto &field : record.getTagRange(field_tag)) {
        const auto subfields(field.getSubfields());
        const bool esra_special_case(subfields.getFirstSubfieldWithCode('a') == "Esra"
                                     or subfields.getFirstSubfieldWithCode('a') == "Esdras");
        const bool maccabee_special_case(subfields.getFirstSubfieldWithCode('a') == "Makkabäer");
        if (not(subfields.getFirstSubfieldWithCode('a') == "Bibel" and subfields.hasSubfield('p')) and not esra_special_case
            and not maccabee_special_case)
            continue;

        std::string book_name_candidate;
        if (esra_special_case)
            book_name_candidate = "esra";
        else if (maccabee_special_case) {
            // If this is a maccabee bible book record, subfield 9 must contain "g:Buch" as there are also
            // records that are about the person/author Maccabee.
            if (subfields.hasSubfieldWithValue('9', "g:Buch"))
                book_name_candidate = "makkabäer";
        } else
            book_name_candidate = TextUtil::UTF8ToLower(subfields.getFirstSubfieldWithCode('p'));

        const auto pair(book_alias_map.find(book_name_candidate));
        if (pair != book_alias_map.cend())
            book_name_candidate = pair->second;
        if (books_of_the_bible.find(book_name_candidate) == books_of_the_bible.cend()) {
            LOG_WARNING(record.getControlNumber() + ": unknown bible book: "
                        + (esra_special_case ? "esra" : (maccabee_special_case ? "makkabäer" : subfields.getFirstSubfieldWithCode('p'))));
            ++unknown_book_count;
            continue;
        }

        std::vector<std::string> n_subfield_values(subfields.extractSubfields('n'));
        if (n_subfield_values.size() > 2) {
            LOG_WARNING("More than 2 $n subfields for PPN " + record.getControlNumber() + "!");
            continue;
        }

        if (not OrderNSubfields(&n_subfield_values)) {
            LOG_WARNING("Don't know what to do w/ the $n subfields for PPN " + record.getControlNumber() + "! ("
                        + StringUtil::Join(n_subfield_values, ", ") + ")");
            continue;
        }

        std::vector<std::string> books;
        CreateNumberedBooks(book_name_candidate, &n_subfield_values, &books);

        // Special processing for 2 Esdras, 5 Esra and 6 Esra
        for (auto &book : books)
            RangeUtil::EsraSpecialProcessing(&book, &n_subfield_values);

        if (not HaveBibleBookCodes(books, bible_book_to_code_map)) {
            LOG_WARNING(record.getControlNumber() + ": found no bible book code for \"" + book_name_candidate + "\"! ("
                        + StringUtil::Join(n_subfield_values, ", ") + ")");
            continue;
        }

        std::vector<std::string> book_codes;
        if (not ConvertBooksToBookCodes(books, bible_book_to_code_map, &book_codes)) {
            LOG_WARNING(record.getControlNumber()
                        + ": can't convert one or more of these books to book codes: " + StringUtil::Join(books, ", ") + "!");
            continue;
        }

        if (book_codes.size() > 1 or n_subfield_values.empty())
            ranges->insert(
                std::make_pair(book_codes.front() + std::string(RangeUtil::MAX_CHAPTER_LENGTH + RangeUtil::MAX_VERSE_LENGTH, '0'),
                               book_codes.back() + std::string(RangeUtil::MAX_CHAPTER_LENGTH + RangeUtil::MAX_VERSE_LENGTH, '9')));
        else if (not RangeUtil::ParseBibleReference(n_subfield_values.front(), book_codes[0], ranges)) {
            LOG_WARNING(record.getControlNumber() + ": failed to parse bible references (1): " + n_subfield_values.front());
            continue;
        }

        found_at_least_one = true;
    }

    return found_at_least_one;
}


/* Scans authority MARC records for records that contain bible references including pericopes. */
void LoadNormData(const std::unordered_map<std::string, std::string> &bible_book_to_code_map, MARC::Reader * const authority_reader,
                  std::vector<std::vector<std::string>> * const pericopes,
                  std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> * const gnd_codes_to_bible_ref_codes_map) {
    gnd_codes_to_bible_ref_codes_map->clear();
    LOG_INFO("Starting loading of norm data.");

    std::unordered_set<std::string> books_of_the_bible;
    ExtractBooksOfTheBible(bible_book_to_code_map, &books_of_the_bible);

    unsigned count(0);
    std::unordered_multimap<std::string, std::string> pericopes_to_ranges_map;
    while (const MARC::Record record = authority_reader->read()) {
        ++count;

        std::string gnd_code;
        if (not MARC::GetGNDCode(record, &gnd_code))
            continue;

        std::set<std::pair<std::string, std::string>> ranges;
        if (not GetBibleRanges("130", record, books_of_the_bible, bible_book_to_code_map, &ranges)) {
            if (not GetBibleRanges("430", record, books_of_the_bible, bible_book_to_code_map, &ranges))
                continue;
            if (not FindPericopes(record, ranges, &pericopes_to_ranges_map))
                continue;
        }

        gnd_codes_to_bible_ref_codes_map->emplace(gnd_code, ranges);
    }

    // Chop up the pericopes:
    for (const auto &pericope_and_ranges : pericopes_to_ranges_map) {
        std::vector<std::string> words;
        StringUtil::Split(pericope_and_ranges.first, ' ', &words);
        pericopes->emplace_back(std::move(words));
    }

    LOG_INFO("Read " + std::to_string(count) + " norm data records.");
}


bool FindGndCodes(const std::string &tags, const MARC::Record &record,
                  const std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> &gnd_codes_to_bible_ref_codes_map) {
    std::set<std::string> individual_tags;
    StringUtil::Split(tags, ':', &individual_tags, /* suppress_empty_components = */ true);

    for (const auto &gnd_code : record.getReferencedGNDNumbers(individual_tags)) {
        const auto gnd_code_and_ranges(gnd_codes_to_bible_ref_codes_map.find(gnd_code));
        if (gnd_code_and_ranges != gnd_codes_to_bible_ref_codes_map.end())
            return true;
    }

    return false;
}


std::vector<std::string> TokenizeText(std::string text) {
    TextUtil::NormaliseDashes(&text);

    std::vector<std::string> tokens;
    std::string current_token;
    for (const char ch : text) {
        if ((not current_token.empty() and StringUtil::IsDigit(current_token.back()) and (ch == 'a' or ch == 'b' or ch == 'c' or ch == ','))
            or (ch == ' ' or ch == '(' or ch == ')' or ch == ';'))
        {
            if (not current_token.empty())
                tokens.emplace_back(current_token);
            current_token.clear();
        } else
            current_token += ch;
    }
    if (not current_token.empty())
        tokens.emplace_back(current_token);

    return tokens;
}


inline bool IsPossibleBookNumeral(const std::string &book_numeral_candidate) {
    if (book_numeral_candidate.length() != 1 or (book_numeral_candidate.length() == 2 and book_numeral_candidate[1] != '.'))
        return false;
    return book_numeral_candidate[0] >= '1' and book_numeral_candidate[0] <= '6';
}


inline bool IsValidBibleBook(const std::string &bible_book_candidate, const RangeUtil::BibleBookCanoniser &bible_book_canoniser,
                             const RangeUtil::BibleBookToCodeMapper &bible_book_to_code_mapper) {
    return not bible_book_to_code_mapper.mapToCode(bible_book_canoniser.canonise(TextUtil::UTF8ToLower(bible_book_candidate))).empty();
}


bool FoundTokenSubstring(const std::vector<std::string> &needle, const std::vector<std::string> &haystack) {
    const size_t haystack_size(haystack.size());
    const size_t needle_size(needle.size());
    if (haystack_size < needle_size)
        return false;

    for (unsigned i(0); i <= haystack_size - needle_size; ++i) {
        for (unsigned k(0); k < needle_size; ++k) {
            if (likely(haystack[i + k] != needle[k]))
                goto outer_loop;
        }

        return true;
outer_loop:
    /* Intentionally empty! */;
    }

    return false;
}


bool ConsistsEntirelyOfLettersFollowedByAnOptionalPeriod(const std::string &utf8_string) {
    if (utf8_string.length() > 1 and utf8_string[utf8_string.length() - 1] == '.')
        return TextUtil::ConsistsEntirelyOfLetters(utf8_string.substr(utf8_string.length() - 1));

    return TextUtil::ConsistsEntirelyOfLetters(utf8_string);
}


const std::set<std::string> FRENCH_MONTHS{ "janvier", "février", "mars",      "avril",   "mai",      "juin",
                                           "juillet", "août",    "septembre", "octobre", "novembre", "décembre" };
const std::vector<std::string> GERMAN_MONTHS_ABBREVS{ "jan", "feb", "mär", "apr", "mai", "jun", "jul", "aug", "sep", "okt", "nov", "dez" };


bool IsFrenchMonth(const std::string &word) {
    const auto month_candidate(TextUtil::UTF8ToLower(word));
    for (const auto &french_month : FRENCH_MONTHS) {
        if (french_month == month_candidate)
            return true;
    }

    return false;
}


bool StartsWithGermanMonthAbbrev(const std::string &word) {
    if (word.length() < 3)
        return false;

    const auto first3(TextUtil::UTF8ToLower(TextUtil::UTF8Substr(word, 0, 3)));
    for (const auto &german_month_abbrev : GERMAN_MONTHS_ABBREVS) {
        if (german_month_abbrev == first3)
            return true;
    }

    return false;
}


std::vector<std::string> ExtractBibleReferenceCandidates(const std::vector<std::string> &tokens,
                                                         const std::vector<std::vector<std::string>> &pericopes,
                                                         const RangeUtil::BibleBookCanoniser &bible_book_canoniser,
                                                         const RangeUtil::BibleBookToCodeMapper &bible_book_to_code_mapper) {
    // See https://www.messiah.edu/download/downloads/id/1647/bible_cite.pdf and
    // https://www.kath-theologie.uni-osnabrueck.de/fileadmin/PDF/Bibelstellen.pdf to understand the regex.
    static const auto chapter_and_verses_matcher(
        RegexMatcher::RegexMatcherFactoryOrDie(
            /*english*/ "^(\\d{1,2}-\\d{1,2}|\\d{1,3}[.:]\\d{1,3}|\\d{1,2}[.:]\\d{1,3}-\\d{1,3}|\\d{1,2}[.:]\\d{1,3}(,\\d{1,3})+"
                        /*german*/ "|\\d{1,2}(;\\s?\\d{1,2}(-\\d{1,2})?)*|\\d{1,2},\\d{1,3}([-.]\\d{1,3})f{0,2}+)$"));

    bool possible_book_seen(false), check_for_french_date(false), check_for_german_date(false);
    std::vector<std::string> bible_reference_candidates;
    std::string bible_reference_candidate_prefix;
    for (auto token(tokens.cbegin()); token != tokens.cend(); ++token) {
        if (possible_book_seen) {
            possible_book_seen = false;
            if (chapter_and_verses_matcher->matched(*token)) {
                if (token == tokens.cend() - 1 or (not check_for_french_date and not check_for_german_date))
                    bible_reference_candidates.emplace_back(bible_reference_candidate_prefix + *token);
                else {
                    if ((check_for_french_date and IsFrenchMonth(*(token + 1)))
                        or (check_for_german_date and StartsWithGermanMonthAbbrev(*(token + 1))))
                        /* not a bible reference */;
                    else
                        bible_reference_candidates.emplace_back(bible_reference_candidate_prefix + *token);
                }
            }
            check_for_french_date = false;
            check_for_german_date = false;
            bible_reference_candidate_prefix.clear();
            continue;
        }

        if (IsPossibleBookNumeral(*token))
            bible_reference_candidate_prefix = *token;
        else if (ConsistsEntirelyOfLettersFollowedByAnOptionalPeriod(*token)) {
            const auto canonized_token(bible_book_canoniser.canonise(*token));
            if (bible_reference_candidate_prefix.empty() or IsPossibleBookNumeral(bible_reference_candidate_prefix))
                bible_reference_candidate_prefix += canonized_token;
            else
                bible_reference_candidate_prefix = canonized_token;
            if (IsValidBibleBook(bible_reference_candidate_prefix, bible_book_canoniser, bible_book_to_code_mapper)) {
                possible_book_seen = true;
                if (*token == "le")
                    check_for_french_date = true;
                else if (*token == "am")
                    check_for_german_date = true;
            } else {
                check_for_french_date = false;
                bible_reference_candidate_prefix.clear();
            }
        }
    }

    std::vector<std::string> lc_tokens;
    lc_tokens.reserve(tokens.size());
    for (const auto &token : tokens)
        lc_tokens.emplace_back(TextUtil::UTF8ToLower(token));

    for (const auto &pericope : pericopes) {
        if (FoundTokenSubstring(pericope, lc_tokens))
            bible_reference_candidates.emplace_back(StringUtil::Join(pericope, ' '));
    }

    return bible_reference_candidates;
}


bool HasGNDBibleRef(
    const MARC::Record &record,
    const std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> &gnd_codes_to_bible_ref_codes_map) {
    return FindGndCodes("600:610:611:630:648:651:655:689", record, gnd_codes_to_bible_ref_codes_map);
}


std::string ExtractBook(std::string reference) {
    StringUtil::TrimWhite(&reference);
    TextUtil::UTF8ToLower(&reference);

    // Skip leading digits:
    auto ch(reference.cbegin());
    while (ch != reference.cend() and StringUtil::IsDigit(*ch))
        ++ch;

    // Extract book:
    std::string book;
    while (ch != reference.cend() and not StringUtil::IsDigit(*ch))
        book += *ch++;

    return book;
}


void FindBibRefCandidates(
    MARC::Reader * const marc_reader, File * const output, const std::vector<std::vector<std::string>> &pericopes,
    const std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> &gnd_codes_to_bible_ref_codes_map) {
    const auto excluded_ppns(LoadPPNExclusionSet());

    RangeUtil::BibleBookCanoniser bible_book_canoniser(UBTools::GetTuelibPath() + "bibleRef/books_of_the_bible_to_canonical_form.map");
    RangeUtil::BibleBookToCodeMapper bible_book_to_code_mapper(UBTools::GetTuelibPath() + "bibleRef/books_of_the_bible_to_code.map");
    unsigned addition_title_reference_count(0);
    while (const MARC::Record record = marc_reader->read()) {
        if (excluded_ppns.find(record.getControlNumber()) != excluded_ppns.cend())
            continue;

        if (not HasGNDBibleRef(record, gnd_codes_to_bible_ref_codes_map)) {
            const auto candidates(ExtractBibleReferenceCandidates(TokenizeText(record.getCompleteTitle()), pericopes, bible_book_canoniser,
                                                                  bible_book_to_code_mapper));
            if (not candidates.empty()) {
                ++addition_title_reference_count;
                *output << TextUtil::CSVEscape(ExtractBook(candidates.front()), /* add_quotes= */ false) << ','
                        << TextUtil::CSVEscape(record.getControlNumber(), /* add_quotes= */ false) << ','
                        << TextUtil::CSVEscape(record.getCompleteTitle(), /* add_quotes= */ false) << '\n';
            }
        }
    }

    LOG_INFO("Found " + std::to_string(addition_title_reference_count) + " titles w/ possible bible references.");
}


void LoadEnglishPericopes(std::vector<std::vector<std::string>> * const pericopes) {
    const auto initial_size(pericopes->size());
    for (auto line : FileUtil::ReadLines(UBTools::GetTuelibPath() + "bibleRef/engish_pericopes")) {
        std::vector<std::string> tokens;
        TextUtil::UTF8ToLower(&line);
        if (StringUtil::SplitThenTrimWhite(line, ' ', &tokens) > 0)
            pericopes->emplace_back(std::move(tokens));
    }
    LOG_INFO("loaded " + std::to_string(pericopes->size() - initial_size) + " English pericopes.");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc != 4)
        Usage();

    const std::string title_input_filename(argv[1]);
    const std::string authority_input_filename(argv[2]);
    const std::string bib_ref_candidates_list_filename(argv[3]);

    auto title_reader(MARC::Reader::Factory(title_input_filename));
    auto authority_reader(MARC::Reader::Factory(authority_input_filename));
    auto bib_ref_candidates_writer(FileUtil::OpenOutputFileOrDie(bib_ref_candidates_list_filename));

    const std::string books_of_the_bible_to_code_map_filename(UBTools::GetTuelibPath() + "bibleRef/books_of_the_bible_to_code.map");
    File books_of_the_bible_to_code_map_file(books_of_the_bible_to_code_map_filename, "r");
    if (not books_of_the_bible_to_code_map_file)
        LOG_ERROR("can't open \"" + books_of_the_bible_to_code_map_filename + "\" for reading!");

    std::unordered_map<std::string, std::string> books_of_the_bible_to_code_map;
    LoadBibleOrderMap(&books_of_the_bible_to_code_map_file, &books_of_the_bible_to_code_map);

    std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> gnd_codes_to_bible_ref_codes_map;
    std::vector<std::vector<std::string>> pericopes;
    LoadNormData(books_of_the_bible_to_code_map, authority_reader.get(), &pericopes, &gnd_codes_to_bible_ref_codes_map);
    LoadEnglishPericopes(&pericopes);
    FindBibRefCandidates(title_reader.get(), bib_ref_candidates_writer.get(), pericopes, gnd_codes_to_bible_ref_codes_map);

    return EXIT_SUCCESS;
}
