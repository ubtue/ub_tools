/** \file    augment_bible_references.cc
 *  \brief   A tool for adding numeric bible references a.k.a. "bible ranges" to MARC-21 datasets.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015-2021, Library of the University of Tübingen

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
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <cstdlib>
#include <cstring>
#include "MARC.h"
#include "MapUtil.h"
#include "RangeUtil.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "UBTools.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage("ix_theo_titles ix_theo_norm augmented_ix_theo_titles");
}


/* Pericopes are found in 130$a if there are also bible references in the 430 field. You should therefore
   only call this after acertaining that one or more 430 fields contain a bible reference. */
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


/* Scans norm data for records that contain bible references.  Found references are converted to bible book
   ranges and will in a later processing phase be added to title data.  We also extract pericopes which will be
   saved to a file that maps periope names to bible ranges. */
void LoadNormData(const std::unordered_map<std::string, std::string> &bible_book_to_code_map, MARC::Reader * const authority_reader,
                  std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> * const gnd_codes_to_bible_ref_codes_map) {
    gnd_codes_to_bible_ref_codes_map->clear();
    LOG_INFO("Starting loading of norm data.");

    std::unordered_set<std::string> books_of_the_bible;
    ExtractBooksOfTheBible(bible_book_to_code_map, &books_of_the_bible);

    unsigned count(0), bible_ref_count(0), pericope_count(0);
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
            ++pericope_count;
        }

        gnd_codes_to_bible_ref_codes_map->emplace(gnd_code, ranges);
        ++bible_ref_count;
    }

    LOG_INFO("About to write \"pericopes_to_codes.map\".");
    MapUtil::SerialiseMap("pericopes_to_codes.map", pericopes_to_ranges_map);

    LOG_INFO("Read " + std::to_string(count) + " norm data record(s).");
    LOG_INFO("Found " + std::to_string(unknown_book_count) + " records w/ unknown bible books.");
    LOG_INFO("Found a total of " + std::to_string(bible_ref_count) + " bible reference records.");
    LOG_INFO("Found " + std::to_string(pericope_count) + " records w/ pericopes.");
}


bool FindGndCodes(const std::string &tags, const MARC::Record &record,
                  const std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> &gnd_codes_to_bible_ref_codes_map,
                  std::set<std::string> * const ranges) {
    ranges->clear();

    std::set<std::string> individual_tags;
    StringUtil::Split(tags, ':', &individual_tags, /* suppress_empty_components = */ true);

    bool found_at_least_one(false);
    for (const auto &gnd_code : record.getReferencedGNDNumbers(individual_tags)) {
        const auto gnd_code_and_ranges(gnd_codes_to_bible_ref_codes_map.find(gnd_code));
        if (gnd_code_and_ranges != gnd_codes_to_bible_ref_codes_map.end()) {
            found_at_least_one = true;
            for (const auto &range : gnd_code_and_ranges->second)
                ranges->insert(range.first + ":" + range.second);
        } else
            LOG_DEBUG(record.getControlNumber() + ": GND code \"" + gnd_code + "\" was not found in our map.");
    }

    return found_at_least_one;
}


/* Augments MARC title records that contain bible references by pointing at bible reference norm data records
   by adding a new MARC field with tag BIB_REF_RANGE_TAG.  This field is filled in with bible ranges. */
void AugmentBibleRefs(
    MARC::Reader * const marc_reader, MARC::Writer * const marc_writer,
    const std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> &gnd_codes_to_bible_ref_codes_map) {
    LOG_INFO("Starting augmentation of title records.");

    unsigned total_count(0), augment_count(0);
    while (MARC::Record record = marc_reader->read()) {
        try {
            ++total_count;

            // Make sure that we don't use a bible reference tag that is already in use for another
            // purpose:
            auto bible_reference_tag_field(record.findTag(RangeUtil::BIB_REF_RANGE_TAG));
            if (bible_reference_tag_field != record.end())
                LOG_ERROR("We need another bible reference tag than \"" + RangeUtil::BIB_REF_RANGE_TAG + "\"!");

            std::set<std::string> ranges;
            if (FindGndCodes("600:610:611:630:648:651:655:689", record, gnd_codes_to_bible_ref_codes_map, &ranges)) {
                ++augment_count;
                std::string range_string;
                for (auto &range : ranges) {
                    if (not range_string.empty())
                        range_string += ',';
                    range_string += StringUtil::Map(range, ':', '_');
                }

                // Put the data into the $a subfield:
                record.insertField(RangeUtil::BIB_REF_RANGE_TAG, { { 'a', range_string }, { 'b', "biblesearch" } });
            }

            marc_writer->write(record);
        } catch (const std::exception &x) {
            LOG_ERROR("caught exception for title record w/ PPN " + record.getControlNumber() + ": " + std::string(x.what()));
        }
    }

    LOG_INFO("Augmented the " + RangeUtil::BIB_REF_RANGE_TAG + "$a field of " + std::to_string(augment_count) + " records of a total of "
             + std::to_string(total_count) + " records.");
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 4)
        Usage();

    const std::string title_input_filename(argv[1]);
    const std::string authority_input_filename(argv[2]);
    const std::string title_output_filename(argv[3]);
    if (unlikely(title_input_filename == title_output_filename))
        LOG_ERROR("Title input file name equals title output file name!");
    if (unlikely(authority_input_filename == title_output_filename))
        LOG_ERROR("Norm data input file name equals title output file name!");

    auto title_reader(MARC::Reader::Factory(title_input_filename));
    auto authority_reader(MARC::Reader::Factory(authority_input_filename));
    auto title_writer(MARC::Writer::Factory(title_output_filename));

    const std::string books_of_the_bible_to_code_map_filename(UBTools::GetTuelibPath() + "bibleRef/books_of_the_bible_to_code.map");
    std::unordered_map<std::string, std::string> books_of_the_bible_to_code_map;
    MapUtil::DeserialiseMap(books_of_the_bible_to_code_map_filename, &books_of_the_bible_to_code_map);

    std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> gnd_codes_to_bible_ref_codes_map;
    LoadNormData(books_of_the_bible_to_code_map, authority_reader.get(), &gnd_codes_to_bible_ref_codes_map);
    AugmentBibleRefs(title_reader.get(), title_writer.get(), gnd_codes_to_bible_ref_codes_map);

    return EXIT_SUCCESS;
}
