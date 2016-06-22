/** \file    augment_bible_references.cc
 *  \brief   A tool for adding numeric bible references to MARC-21 datasets.
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
#include "MarcUtil.h"
#include "MarcXmlWriter.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TextUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname
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
bool GetGNDCode(const std::vector<DirectoryEntry> &dir_entries, const std::vector<std::string> &field_data,
                std::string * const gnd_code)
{
    gnd_code->clear();

    const auto _035_iter(DirectoryEntry::FindField("035", dir_entries));
    if (_035_iter == dir_entries.end())
        return false;
    const Subfields _035_subfields(field_data[_035_iter - dir_entries.begin()]);
    const std::string _035a_field(_035_subfields.getFirstSubfieldValue('a'));
    if (not StringUtil::StartsWith(_035a_field, "(DE-588)"))
        return false;
    *gnd_code = _035a_field.substr(8);
    return not gnd_code->empty();
}


bool FindPericopes(const std::string &book_name, const std::vector<DirectoryEntry> &dir_entries,
                   const std::vector<std::string> &field_data,
                   const std::set<std::pair<std::string, std::string>> &ranges,
                   std::unordered_multimap<std::string, std::string> * const pericopes_to_ranges_map)
{
    static const std::string PERICOPE_FIELD("430");
    std::vector<std::string> pericopes;
    auto field_iter(DirectoryEntry::FindField(PERICOPE_FIELD, dir_entries));
    while (field_iter != dir_entries.end() and field_iter->getTag() == PERICOPE_FIELD) {
        const Subfields subfields(field_data[field_iter - dir_entries.begin()]);
        std::string a_subfield(subfields.getFirstSubfieldValue('a'));
        StringUtil::ToLower(&a_subfield);
	StringUtil::CollapseAndTrimWhitespace(&a_subfield);
        if (a_subfield != book_name)
            pericopes.push_back(a_subfield);
        ++field_iter;
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


void CreateNumberedBooks(const std::string &book_name_candidate,
                         const std::vector<std::string> &n_subfield_values,
                         std::vector<std::string> * const numbered_books)
{
    numbered_books->clear();

    if (n_subfield_values.empty()) {
        numbered_books->emplace_back(book_name_candidate);
        return;
    }

    if (IsValidSingleDigitArabicOrdinal(n_subfield_values[0])) {
        numbered_books->emplace_back(std::string(1, n_subfield_values[0][0]) + book_name_candidate);
        return;
    }

    if (n_subfield_values[0] == "1. 2.") {
        numbered_books->emplace_back("1" + book_name_candidate);
        numbered_books->emplace_back("2" + book_name_candidate);
        return;
    }

    if (n_subfield_values[0] == "2.-3.") {
        numbered_books->emplace_back("2" + book_name_candidate);
        numbered_books->emplace_back("3" + book_name_candidate);
        return;
    }

    if (n_subfield_values[0] == "1.-3.") {
        numbered_books->emplace_back("1" + book_name_candidate);
        numbered_books->emplace_back("2" + book_name_candidate);
        numbered_books->emplace_back("3" + book_name_candidate);
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


bool ConvertNumberedBooksToBookCodes(const std::vector<std::string> &numbered_books,
                                     const std::unordered_map<std::string, std::string> &bible_book_to_code_map,
                                     std::vector<std::string> * const book_codes)
{
    book_codes->clear();

    for (const auto &numbered_book : numbered_books) {
        const auto book_and_code(bible_book_to_code_map.find(numbered_book));
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


void LoadNormData(const bool verbose, const std::unordered_map<std::string, std::string> &bible_book_to_code_map,
		  File * const norm_input,
                  std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>> * const
                      gnd_codes_to_bible_ref_codes_map)
{
    gnd_codes_to_bible_ref_codes_map->clear();
    if (verbose)
        std::cerr << "Starting loading of norm data.\n";

    std::unordered_set<std::string> books_of_the_bible;
    ExtractBooksOfTheBible(bible_book_to_code_map, &books_of_the_bible);

    unsigned count(0), bible_ref_count(0), pericope_count(0), unknown_book_count(0);
    std::unordered_multimap<std::string, std::string> pericopes_to_ranges_map;
    while (const MarcUtil::Record record = MarcUtil::Record::XmlFactory(norm_input)) {
        ++count;

        const ssize_t _130_index(record.getFieldIndex("130"));
        if (_130_index == -1)
            continue;

        const std::vector<std::string> &fields(record.getFields());
        const Subfields _130_subfields(fields[_130_index]);
        if (_130_subfields.getFirstSubfieldValue('a') != "Bibel")
            continue;

        if (not _130_subfields.hasSubfield('p'))
            continue;

        std::string book_name_candidate(StringUtil::ToLower(_130_subfields.getFirstSubfieldValue('p')));
        const auto pair(book_alias_map.find(book_name_candidate));
        if (pair != book_alias_map.cend())
            book_name_candidate = pair->second;
        if (books_of_the_bible.find(book_name_candidate) == books_of_the_bible.cend()) {
            std::cerr << fields[0] << ": unknown bible book: " << _130_subfields.getFirstSubfieldValue('p') << '\n';
            ++unknown_book_count;
            continue;
        }

        std::vector<std::string> n_subfield_values;
        _130_subfields.extractSubfields("n", &n_subfield_values);
        if (n_subfield_values.size() > 2) {
            std::cerr << "More than 2 $n subfields for PPN " << fields[0] << "!\n";
            continue;
        }

        if (not OrderNSubfields(&n_subfield_values)) {
            std::cerr << "Don't know what to do w/ the $n subfields for PPN " << fields[0] << "! ("
                      << StringUtil::Join(n_subfield_values, ", ") << ")\n";
            continue;
        }

        std::vector<std::string> numbered_books;
        CreateNumberedBooks(book_name_candidate, n_subfield_values, &numbered_books);
        if (not HaveBibleBookCodes(numbered_books, bible_book_to_code_map)) {
            std::cerr << fields[0] << ": found no bible book code for \"" << book_name_candidate
                      << "\"! (" << StringUtil::Join(n_subfield_values, ", ") << ")\n";
            continue;
        }

        std::vector<std::string> book_codes;
        if (not ConvertNumberedBooksToBookCodes(numbered_books, bible_book_to_code_map, &book_codes)) {
            std::cerr << fields[0] << ": can't convert one or more of these books to book codes: "
                      << StringUtil::Join(numbered_books, ", ") << "!\n";
            continue;
        }

        std::set<std::pair<std::string, std::string>> ranges;
        if (n_subfield_values.size() == 2) {
            if (book_codes.size() != 1)  {
                std::cerr << fields[0] << ": this should never happen: "
                          << "n_subfield_values.size() == 2 AND book_codes.size() != 1\n";
                continue;   
            }
            if (not ParseBibleReference(n_subfield_values[1], book_codes[0], &ranges)) {
                std::cerr << fields[0] << ": failed to parse bible references (1): " << n_subfield_values[1] << '\n';
                continue;
            }
        } else if (book_codes.size() == 1) {
            if (n_subfield_values.empty() or StringUtil::IsDigit(book_codes[0][0]))
                ranges.insert(std::make_pair(book_codes[0] + "00000", book_codes[0] + "99999"));
            else if (not ParseBibleReference(n_subfield_values[0], book_codes[0], &ranges)) {
                std::cerr << fields[0] << ": failed to parse bible references (2): $p=" << numbered_books[0]
                          << ", $n=" << n_subfield_values[0] << '\n';
                continue;   
            }
        } else if (book_codes.size() == 3)
            ranges.insert(std::make_pair(book_codes[0] + "00000", book_codes[2] + "99999"));
        else { // Assume book_codes.size() == 2.
            if (book_codes.size() != 2) {
                std::cerr << fields[0] << "expected 2 book codes but found \"" << StringUtil::Join(book_codes, ", ")
                          << "\"!\n";
                continue;
            }
            ranges.insert(std::make_pair(book_codes[0] + "00000", book_codes[1] + "99999"));
        }

        const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
        std::string gnd_code;
        if (not GetGNDCode(dir_entries, fields, &gnd_code))
            continue;

        if (FindPericopes(book_name_candidate, dir_entries, fields, ranges, &pericopes_to_ranges_map)) {
            ++pericope_count;
        }

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


bool FindGndCodes(const std::string &tags, const MarcUtil::Record &record,
                  const std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>>
                  &gnd_codes_to_bible_ref_codes_map, std::set<std::string> * const ranges)
{
    ranges->clear();

    std::vector<std::string> individual_tags;
    StringUtil::Split(tags, ':', &individual_tags);

    bool found_at_least_one(false);
    for (const auto &tag : individual_tags) {
        const ssize_t first_index(record.getFieldIndex(tag));
        if (first_index == -1)
            continue;

	const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
        for (size_t index(first_index); index < dir_entries.size() and dir_entries[index].getTag() == tag; ++index) {
	    const std::vector<std::string> &fields(record.getFields());
            const Subfields subfields(fields[index]);
            const std::string subfield2(subfields.getFirstSubfieldValue('2'));
            if (subfield2.empty() or subfield2 != "gnd")
                continue;

            const auto begin_end(subfields.getIterators('0'));
            for (auto subfield0(begin_end.first); subfield0 != begin_end.second; ++subfield0) {
                if (not StringUtil::StartsWith(subfield0->second, "(DE-588)"))
                    continue;

                const std::string gnd_code(subfield0->second.substr(8));
                const auto gnd_code_and_ranges(gnd_codes_to_bible_ref_codes_map.find(gnd_code));
                if (gnd_code_and_ranges != gnd_codes_to_bible_ref_codes_map.end()) {
                    found_at_least_one = true;
                    for (const auto &range : gnd_code_and_ranges->second)
                        ranges->insert(range.first + ":" + range.second);
                }
            }
        }
    }

    return found_at_least_one;
}


void AugmentBibleRefs(const bool verbose, File * const input, File * const output,
                      const std::unordered_map<std::string, std::set<std::pair<std::string, std::string>>>
                          &gnd_codes_to_bible_ref_codes_map)
{
    if (verbose)
        std::cerr << "Starting augmentation of title records.\n";

    MarcXmlWriter xml_writer(output);
    unsigned total_count(0), augment_count(0);
    while (MarcUtil::Record record = MarcUtil::Record::XmlFactory(input)) {
	record.setRecordWillBeWrittenAsXml(true);
        ++total_count;

        // Make sure that we don't use a bible reference tag that is already in use for another
        // purpose:
	const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
        const auto bib_ref_begin_end(DirectoryEntry::FindFields(BIB_REF_RANGE_TAG, dir_entries));
        if (bib_ref_begin_end.first != bib_ref_begin_end.second)
            Error("We need another bible reference tag than \"" + BIB_REF_RANGE_TAG + "\"!");

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
            range_string = "  ""\x1F""a" + range_string;
            record.insertField(BIB_REF_RANGE_TAG, range_string);
        }

	record.write(&xml_writer);
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
    File title_input(title_input_filename, "r");
    if (not title_input)
        Error("can't open \"" + title_input_filename + "\" for reading!");

    const std::string norm_input_filename(argv[verbose ? 3 : 2]);
    File norm_input(norm_input_filename, "r");
    if (not norm_input)
        Error("can't open \"" + norm_input_filename + "\" for reading!");

    const std::string title_output_filename(argv[verbose ? 4 : 3]);
    File title_output(title_output_filename, "w");
    if (not title_output)
        Error("can't open \"" + title_output_filename + "\" for writing!");

    if (unlikely(title_input_filename == title_output_filename))
        Error("Title input file name equals title output file name!");

    if (unlikely(norm_input_filename == title_output_filename))
        Error("Norm data input file name equals title output file name!");

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
	LoadNormData(verbose, books_of_the_bible_to_code_map, &norm_input, &gnd_codes_to_bible_ref_codes_map);
	AugmentBibleRefs(verbose, &title_input, &title_output, gnd_codes_to_bible_ref_codes_map);
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}
