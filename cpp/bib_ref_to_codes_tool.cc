/** \file    bib_ref_to_codes_tool.cc
 *  \brief   A tool for converting bible references to numeric codes.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015-2017, Library of the University of Tübingen

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
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include "BibleUtil.h"
#include "MapIO.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "util.h"


void HandlePericopes(const bool verbose, const bool generate_solr_query, const std::string &bible_reference_candidate,
                     const std::string &pericopes_to_codes_map_filename)
{
    std::unordered_multimap<std::string, std::string> pericopes_to_codes_map;
    MapIO::DeserialiseMap(pericopes_to_codes_map_filename, &pericopes_to_codes_map);

    const auto begin_end(pericopes_to_codes_map.equal_range(bible_reference_candidate));
    if (begin_end.first != begin_end.second) {
        if (verbose)
            std::cerr << "Found a pericope to codes mapping.\n";
        std::string query;
        for (auto pair(begin_end.first); pair != begin_end.second; ++pair) {
            if (generate_solr_query) {
                if (not query.empty())
                    query += ' ';
                query += StringUtil::Map(pair->second, ':', '_');
            } else
                std::cout << pair->second << '\n';
        }
        if (generate_solr_query)
            std::cout << query << '\n';

        std::exit(EXIT_SUCCESS);
    }
}


void HandleBookRanges(const bool verbose, const bool generate_solr_query,
                      const std::string &books_of_the_bible_to_canonical_form_map_filename,
                      const std::string &books_of_the_bible_to_code_map_filename,
                      const std::string &bible_reference_candidate)
{
    const RegexMatcher *matcher(RegexMatcher::RegexMatcherFactory("^([12])-([23])\\s*([A-ZÖa-zö]+)\\s*$"));
    if (not matcher->matched(bible_reference_candidate))
        return;

    unsigned starting_volume;
    if (not StringUtil::ToUnsigned((*matcher)[1], &starting_volume) or starting_volume > 2)
        Error("\"" + (*matcher)[1] + "\" is not a valid starting volume!");

    unsigned ending_volume;
    if (not StringUtil::ToUnsigned((*matcher)[2], &ending_volume) or ending_volume > 3
        or ending_volume <= starting_volume)
        Error("\"" + (*matcher)[2] + "\" is not a valid ending volume!");

    const std::string non_canonical_book_name((*matcher)[3]);
    BibleUtil::BibleBookCanoniser bible_book_canoniser(books_of_the_bible_to_canonical_form_map_filename);
    const std::string starting_bible_book_candidate(bible_book_canoniser.canonise(
        std::to_string(starting_volume) + non_canonical_book_name, verbose));
                                     
    const std::string ending_bible_book_candidate(bible_book_canoniser.canonise(
        std::to_string(ending_volume) + non_canonical_book_name, verbose));
                                     
    if (verbose) {
        std::cout << "Identified a bible book range.  Starting volume " << starting_volume << ", ending volume "
                  << ending_volume << ", book is \"" << non_canonical_book_name << "\".\n";
    }

    BibleUtil::BibleBookToCodeMapper bible_book_to_code_mapper(books_of_the_bible_to_code_map_filename);
    const std::string first_book_code(bible_book_to_code_mapper.mapToCode(starting_bible_book_candidate, verbose));
    const std::string second_book_code(bible_book_to_code_mapper.mapToCode(ending_bible_book_candidate, verbose));

    std::cout << (first_book_code + std::string(BibleUtil::MAX_CHAPTER_LENGTH + BibleUtil::MAX_VERSE_LENGTH, '0'))
              << (generate_solr_query ? '_' : ':')
              << (second_book_code + std::string(BibleUtil::MAX_CHAPTER_LENGTH + BibleUtil::MAX_VERSE_LENGTH, '9'))
              << '\n';

    std::exit(EXIT_SUCCESS);
}


void HandleOrdinaryReferences(const bool verbose, const bool generate_solr_query,
                              const std::string &bible_reference_candidate,
                              const std::string &books_of_the_bible_to_code_map_filename,
                              const std::string &books_of_the_bible_to_canonical_form_map_filename)
{
    std::string book_candidate, chapters_and_verses_candidate;
    BibleUtil::SplitIntoBookAndChaptersAndVerses(
        bible_reference_candidate, &book_candidate, &chapters_and_verses_candidate);
    if (verbose) {
        std::cerr << "book_candidate = \"" << book_candidate << "\"\n";
        std::cerr << "chapters_and_verses_candidate = \"" << chapters_and_verses_candidate << "\"\n";
    }

    BibleUtil::BibleBookCanoniser bible_book_canoniser(books_of_the_bible_to_canonical_form_map_filename);
    book_candidate = bible_book_canoniser.canonise(book_candidate, verbose);
    BibleUtil::BibleBookToCodeMapper bible_book_to_code_mapper(books_of_the_bible_to_code_map_filename);
    const std::string book_code(bible_book_to_code_mapper.mapToCode(book_candidate, verbose));
    if (verbose)
        std::cerr << "book code = \"" << book_code << "\"\n";
    if (chapters_and_verses_candidate.empty()) {
        std::cout << book_code
                  << std::string(BibleUtil::MAX_CHAPTER_LENGTH + BibleUtil::MAX_VERSE_LENGTH, '0')
                  << (generate_solr_query ? '_' : ':') << book_code
                  << std::string(BibleUtil::MAX_CHAPTER_LENGTH + BibleUtil::MAX_VERSE_LENGTH, '9')
                  << '\n';

        std::exit(EXIT_SUCCESS);
    }

    std::set<std::pair<std::string, std::string>> start_end;
    if (not BibleUtil::ParseBibleReference(chapters_and_verses_candidate, book_code, &start_end)) {
        if (verbose)
            std::cerr << "The parsing of \"" << chapters_and_verses_candidate
                      << "\" as chapters and verses failed!\n";
        std::exit(EXIT_FAILURE);
    }

    std::string query;
    for (const auto &pair : start_end) {
        if (generate_solr_query) {
            if (not query.empty())
                query += ' ';
            query += pair.first + "_" + pair.second;
        } else
            std::cout << pair.first << ':' << pair.second << '\n';
    }
    if (generate_solr_query)
        std::cout << query << '\n';
}


static void Usage() __attribute__((noreturn));


static void Usage() {
    std::cerr << "usage: " << progname << " [--debug|--query] bible_reference_candidate\n";
    std::cerr << "          books_of_the_bible_to_code_map\n";
    std::cerr << "          books_of_the_bible_to_canonical_form_map pericopes_to_codes_map\n";
    std::cerr << '\n';
    std::cerr << "          When --debug has been specified additional tracing output will be generated.\n";
    std::cerr << "          When --query has been specified SOLR search queries will be output.\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char **argv) {
    ::progname = argv[0];

    bool verbose(false), generate_solr_query(false);

    if (argc == 6) {
        if (std::strcmp(argv[1], "--debug") == 0)
            verbose = true;
        else if (std::strcmp(argv[1], "--query") == 0)
            generate_solr_query = true;
        else
            Usage();
        ++argv, --argc;
    }

    if (argc != 5)
        Usage();

    const std::string books_of_the_bible_to_code_map_filename(argv[2]);
    const std::string books_of_the_bible_to_canonical_form_map_filename(argv[3]);
    std::string bible_reference_candidate(StringUtil::Trim(StringUtil::ToLower(argv[1])));
    StringUtil::CollapseWhitespace(&bible_reference_candidate);

    HandleBookRanges(verbose, generate_solr_query, books_of_the_bible_to_canonical_form_map_filename,
                     books_of_the_bible_to_code_map_filename, bible_reference_candidate);
    HandlePericopes(verbose, generate_solr_query, bible_reference_candidate, argv[4]);
    HandleOrdinaryReferences(verbose, generate_solr_query, bible_reference_candidate,
                             books_of_the_bible_to_code_map_filename,
                             books_of_the_bible_to_canonical_form_map_filename);
}
