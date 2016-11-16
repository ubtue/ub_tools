/** \file    augment_bible_references.cc
 *  \brief   A tool for adding numeric bible references a.k.a. "bible ranges" to MARC-21 datasets.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015-2016, Library of the University of Tübingen

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
#include "BibleReferenceParser.h"
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MapIO.h"
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "MarcUtil.h"
#include "MarcXmlWriter.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TextUtil.h"
#include "util.h"
#include "XmlWriter.h"


void Usage() {
    std::cerr << "Usage: " << ::progname
              << " [--verbose] ix_theo_titles ix_theo_norm augmented_ix_theo_titles\n";
    std::exit(EXIT_FAILURE);
}


const std::string BIB_REF_RANGE_TAG("801");
const std::string BIB_BROWSE_TAG("802");


void LoadBibleOrderMap(const bool verbose, File * const input,
                       std::unordered_map<std::string, std::string> * const books_of_the_bible_to_code_map)
{
    if (verbose)
        std::cerr << "Started loading of the bible-order map.\n";

    unsigned line_no(0);
    while (not input->eof()) {
        const std::string line(input->getline());
        if (line.empty())
            continue;
        ++line_no;

        const size_t equal_pos(line.find('='));
        if (equal_pos == std::string::npos)
            Error("malformed line #" + std::to_string(line_no) + " in the bible-order map file!");
        (*books_of_the_bible_to_code_map)[StringUtil::ToLower(line.substr(0, equal_pos))] =
            line.substr(equal_pos + 1);
    }

    if (verbose)
        std::cerr << "Loaded " << line_no << " entries from the bible-order map file.\n";
}


/** \brief True if a GND code was found in 035$a else false. */
bool GetGNDCode(const MarcRecord &record, std::string * const gnd_code)
{
    gnd_code->clear();

    const size_t _035_index(record.getFieldIndex("035"));
    if (_035_index == MarcRecord::FIELD_NOT_FOUND)
        return false;
    const Subfields _035_subfields(record.getSubfields(_035_index));
    const std::string _035a_field(_035_subfields.getFirstSubfieldValue('a'));
    if (not StringUtil::StartsWith(_035a_field, "(DE-588)"))
        return false;
    *gnd_code = _035a_field.substr(8);
    return not gnd_code->empty();
}


/* Pericopes are found in 130$a if there are also bible references in the 430 field. You should therefore
   only call this after acertaining that one or more 430 fields contain a bible reference. */
bool FindPericopes(const MarcRecord &record, const std::set<std::pair<std::string, std::string>> &ranges,
                   std::unordered_multimap<std::string, std::string> * const pericopes_to_ranges_map)
{
    static const std::string PERICOPE_FIELD("130");
    std::vector<std::string> pericopes;
    for (size_t index(record.getFieldIndex(PERICOPE_FIELD)); index < record.getNumberOfFields() and record.getTag(index) == PERICOPE_FIELD; ++index) {
        const Subfields subfields(record.getSubfields(index));
        std::string a_subfield(subfields.getFirstSubfieldValue('a'));
        StringUtil::ToLower(&a_subfield);
        StringUtil::CollapseAndTrimWhitespace(&a_subfield);
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
    return ordinal_candidate.length() == 2 and StringUtil::IsDigit(ordinal_candidate[0])
           and ordinal_candidate[1] == '.';
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
void CreateNumberedBooks(const std::string &book_name_candidate,
                         std::vector<std::string> * const n_subfield_values,
                         std::vector<std::string> * const numbered_books)
{
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
                        const std::unordered_map<std::string, std::string> &bible_book_to_code_map)
{
    for (const auto &book_name_candidate : book_name_candidates) {
        if (bible_book_to_code_map.find(book_name_candidate) == bible_book_to_code_map.cend())
            return false;
    }

    return true;
}


bool ConvertBooksToBookCodes(const std::vector<std::string> &books,
                             const std::unordered_map<std::string, std::string> &bible_book_to_code_map,
                             std::vector<std::string> * const book_codes)
{
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
                            std::unordered_set<std::string> * const books_of_the_bible)
{
    books_of_the_bible->clear();

    for (const auto &book_and_code : bible_book_to_code_map) {
        books_of_the_bible->insert(StringUtil::IsDigit(book_and_code.first[0])
                                   ? book_and_code.first.substr(1) : book_and_code.first);
    }
}


const std::map<std::string, std::string> book_alias_map {
    { "jesus sirach", "sirach" },
    { "offenbarung des johannes", "offenbarungdesjohannes" }      
};


static unsigned unknown_book_count;


std::string RangesToString(const std::set<std::pair<std::string, std::string>> &ranges) {
    std::string ranges_as_string;
    for (const auto &start_and_end : ranges) {
        if (not ranges_as_string.empty())
            ranges_as_string += ", ";
        ranges_as_string += start_and_end.first + ":" + start_and_end.second;
    }
    return ranges_as_string;
}


/*  Possible fields containing bible references which will be extracted as bible ranges are 130 and 430
    (specified by "field_tag").  If one of these fields contains a bible reference, the subfield "a" must
    contain the text "Bible".  Subfield "p" must contain the name of a book of the bible.  Book ordinals and
    chapter and verse indicators would be in one or two "n" subfields.
 */
bool GetBibleRanges(const std::string &field_tag, const MarcRecord &record,
                    const std::unordered_set<std::string> &books_of_the_bible,
                    const std::unordered_map<std::string, std::string> &bible_book_to_code_map,
                    std::set<std::pair<std::string, std::string>> * const ranges)
{
    ranges->clear();

    size_t index(record.getFieldIndex(field_tag));
    if (index == MarcRecord::FIELD_NOT_FOUND)
        return false;

    bool found_at_least_one(false);
    for (/* Intentionally empty! */; index < record.getNumberOfFields() and record.getTag(index) == field_tag; ++index)
    {
        const Subfields subfields(record.getSubfields(index));
        if (subfields.getFirstSubfieldValue('a') != "Bibel")
            continue;
        if (not subfields.hasSubfield('p'))
            continue;

        std::string book_name_candidate(StringUtil::ToLower(subfields.getFirstSubfieldValue('p')));
        const auto pair(book_alias_map.find(book_name_candidate));
        if (pair != book_alias_map.cend())
            book_name_candidate = pair->second;
        if (books_of_the_bible.find(book_name_candidate) == books_of_the_bible.cend()) {
            std::cerr << record.getControlNumber() << ": unknown bible book: " << subfields.getFirstSubfieldValue('p') << '\n';
            ++unknown_book_count;
            continue;
        }

        std::vector<std::string> n_subfield_values;
        subfields.extractSubfields("n", &n_subfield_values);
        if (n_subfield_values.size() > 2) {
            std::cerr << "More than 2 $n subfields for PPN " << record.getControlNumber() << "!\n";
            continue;
        }

        if (not OrderNSubfields(&n_subfield_values)) {
            std::cerr << "Don't know what to do w/ the $n subfields for PPN " << record.getControlNumber() << "! ("
                      << StringUtil::Join(n_subfield_values, ", ") << ")\n";
            continue;
        }

        std::vector<std::string> books;
        CreateNumberedBooks(book_name_candidate, &n_subfield_values, &books);
        if (not HaveBibleBookCodes(books, bible_book_to_code_map)) {
            std::cerr << record.getControlNumber() << ": found no bible book code for \"" << book_name_candidate
                      << "\"! (" << StringUtil::Join(n_subfield_values, ", ") << ")\n";
            continue;
        }

        std::vector<std::string> book_codes;
        if (not ConvertBooksToBookCodes(books, bible_book_to_code_map, &book_codes)) {
            std::cerr << record.getControlNumber() << ": can't convert one or more of these books to book codes: "
                      << StringUtil::Join(books, ", ") << "!\n";
            continue;
        }

        if (book_codes.size() > 1 or n_subfield_values.empty())
            ranges->insert(
                std::make_pair(
                    book_codes.front()
                    + std::string(BibleReferenceParser::MAX_CHAPTER_LENGTH + BibleReferenceParser::MAX_VERSE_LENGTH,
                                  '0'),
                    book_codes.back()
                    + std::string(BibleReferenceParser::MAX_CHAPTER_LENGTH + BibleReferenceParser::MAX_VERSE_LENGTH,
                                  '9')
                )
            );
        else if (not BibleReferenceParser::ParseBibleReference(n_subfield_values.front(), book_codes[0], ranges)) {
            std::cerr << record.getControlNumber() << ": failed to parse bible references (1): "
                      << n_subfield_values.front() << '\n';
            continue;
        }

        found_at_least_one = true;
    }

    return found_at_least_one;
}


/* Scans norm data for records that contain bible references.  Found references are converted to bible book
   ranges and will in a later processing phase be added to title data.  We also extract pericopes which will be
   saved to a file that maps periope names to bible ranges. */
void LoadNormData(const bool verbose, const std::unordered_map<std::string, std::string> &bible_book_to_code_map,
                  MarcReader * const authority_reader,
                  std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> * const
                      gnd_codes_to_bible_ref_codes_map)
{
    gnd_codes_to_bible_ref_codes_map->clear();
    if (verbose)
        std::cerr << "Starting loading of norm data.\n";

    std::unordered_set<std::string> books_of_the_bible;
    ExtractBooksOfTheBible(bible_book_to_code_map, &books_of_the_bible);

    unsigned count(0), bible_ref_count(0), pericope_count(0);
    std::unordered_multimap<std::string, std::string> pericopes_to_ranges_map;
    while (const MarcRecord record = authority_reader->read()) {
        ++count;

        std::string gnd_code;
        if (not MarcUtil::GetGNDCode(record, &gnd_code))
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

    if (verbose)
        std::cerr << "About to write \"pericopes_to_codes.map\".\n";
    MapIO::SerialiseMap("pericopes_to_codes.map", pericopes_to_ranges_map);

    if (verbose) {
        std::cerr << "Read " << count << " norm data records.\n";
        std::cerr << "Found " << unknown_book_count << " records w/ unknown bible books.\n";
        std::cerr << "Found a total of " << bible_ref_count << " bible reference records.\n";
        std::cerr << "Found " << pericope_count << " records w/ pericopes.\n";
    }
}


bool FindGndCodes(const bool verbose, const std::string &tags, const MarcRecord &record,
                  const std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>>
                  &gnd_codes_to_bible_ref_codes_map, std::set<std::string> * const ranges)
{
    ranges->clear();

    std::vector<std::string> individual_tags;
    StringUtil::Split(tags, ':', &individual_tags);

    bool found_at_least_one(false);
    for (const auto &tag : individual_tags) {
        for (size_t index(record.getFieldIndex(tag)); index < record.getNumberOfFields() and record.getTag(index) == tag; ++index) {
            const Subfields subfields(record.getSubfields(index));
            const std::string subfield2(subfields.getFirstSubfieldValue('2'));
            if (subfield2.empty() or subfield2 != "gnd")
                continue;

            const auto begin_end(subfields.getIterators('0'));
            for (auto subfield0(begin_end.first); subfield0 != begin_end.second; ++subfield0) {
                if (not StringUtil::StartsWith(subfield0->value_, "(DE-588)"))
                    continue;

                const std::string gnd_code(subfield0->value_.substr(8));
                const auto gnd_code_and_ranges(gnd_codes_to_bible_ref_codes_map.find(gnd_code));
                if (gnd_code_and_ranges != gnd_codes_to_bible_ref_codes_map.end()) {
                    found_at_least_one = true;
                    for (const auto &range : gnd_code_and_ranges->second)
                        ranges->insert(range.first + ":" + range.second);
                } else if (verbose)
                    std::cerr << record.getControlNumber() << ": GND code \"" << gnd_code
                              << "\" was not found in our map.\n";
            }
        }
    }

    return found_at_least_one;
}


/* Augments MARC title records that contain bible references by pointing at bible reference norm data records
   by adding a new MARC field with tag BIB_REF_RANGE_TAG.  This field is filled in with bible ranges. */
void AugmentBibleRefs(const bool verbose, MarcReader * const marc_reader, MarcWriter * const marc_writer,
                      const std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>>
                          &gnd_codes_to_bible_ref_codes_map)
{
    if (verbose)
        std::cerr << "Starting augmentation of title records.\n";

    unsigned total_count(0), augment_count(0);
    while (MarcRecord record = marc_reader->read()) {
        ++total_count;

        // Make sure that we don't use a bible reference tag that is already in use for another
        // purpose:
        const size_t bib_ref_index(record.getFieldIndex(BIB_REF_RANGE_TAG));
        if (bib_ref_index != MarcRecord::FIELD_NOT_FOUND)
            Error("We need another bible reference tag than \"" + BIB_REF_RANGE_TAG + "\"!");

        std::set<std::string> ranges;
        if (FindGndCodes(verbose, "600:610:611:630:648:651:655:689", record, gnd_codes_to_bible_ref_codes_map,
                         &ranges))
        {
            ++augment_count;
             std::string range_string;
            for (auto &range : ranges) {
                if (not range_string.empty())
                    range_string += ',';
                range_string += StringUtil::Map(range, ':', '_');
            }

            // Put the data into the $a subfield:
            record.insertSubfield(BIB_REF_RANGE_TAG, 'a', range_string);
        }

        marc_writer->write(record);
    }

    if (verbose)
        std::cerr << "Augmented the " << BIB_REF_RANGE_TAG << "$a field of " << augment_count
                  << " records of a total of " << total_count << " records.\n";
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc != 4 and argc != 5)
        Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose ? (argc != 5) : (argc != 4))
        Usage();

    const std::string title_input_filename(argv[verbose ? 2 : 1]);
    const std::string authority_input_filename(argv[verbose ? 3 : 2]);
    const std::string title_output_filename(argv[verbose ? 4 : 3]);
    if (unlikely(title_input_filename == title_output_filename))
        Error("Title input file name equals title output file name!");
    if (unlikely(authority_input_filename == title_output_filename))
        Error("Norm data input file name equals title output file name!");

    std::unique_ptr<MarcReader> title_reader(MarcReader::Factory(title_input_filename, MarcReader::BINARY));
    std::unique_ptr<MarcReader> authority_reader(MarcReader::Factory(authority_input_filename, MarcReader::BINARY));
    std::unique_ptr<MarcWriter> title_writer(MarcWriter::Factory(title_output_filename, MarcWriter::BINARY));

    const std::string books_of_the_bible_to_code_map_filename(
        "/var/lib/tuelib/bibleRef/books_of_the_bible_to_code.map");
    File books_of_the_bible_to_code_map_file(books_of_the_bible_to_code_map_filename, "r");
    if (not books_of_the_bible_to_code_map_file)
        Error("can't open \"" + books_of_the_bible_to_code_map_filename + "\" for reading!");

    try {
        std::unordered_map<std::string, std::string> books_of_the_bible_to_code_map;
        LoadBibleOrderMap(verbose, &books_of_the_bible_to_code_map_file, &books_of_the_bible_to_code_map);

        std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>>
            gnd_codes_to_bible_ref_codes_map;
        LoadNormData(verbose, books_of_the_bible_to_code_map, authority_reader.get(),
                     &gnd_codes_to_bible_ref_codes_map);
        AugmentBibleRefs(verbose, title_reader.get(), title_writer.get(), gnd_codes_to_bible_ref_codes_map);
    } catch (const std::exception &x) {
        Error("caught exception: " + std::string(x.what()));
    }
}
