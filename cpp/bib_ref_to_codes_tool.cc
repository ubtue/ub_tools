/** \file    bib_ref_to_codes_tool.cc
 *  \brief   A tool for converting bible references to numeric codes.
 *  \author  Dr. Johannes Ruscheinski
 */

/*
    Copyright (C) 2015-2020 Library of the University of Tübingen

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
#include "MapUtil.h"
#include "RangeUtil.h"
#include "RegexMatcher.h"
#include "StringUtil.h"
#include "TextUtil.h"
#include "util.h"


namespace {


[[noreturn]] void Usage() {
    ::Usage(
        "(--debug|--query|--date-query) [--map-file-directory=path] bible_reference_candidate\n"
        " bible_aliases_map books_of_the_bible_to_code_map\n"
        " books_of_the_bible_to_canonical_form_map pericopes_to_codes_map\n\n"
        " When --debug has been specified additional tracing output will be generated.\n"
        " When --query has been specified SOLR range search queries will be output.\n"
        " When --date-query has been specified SOLR date range search queries will be output.\n"
        " If --map-file-directory=... has been specified, the provided path will be prefixed to all\n"
        " map-file arguments and, if the map arguments are left off, the default names will be used.");
}


enum OutputType { DEBUG, RANGE_QUERY, DATE_RANGE_QUERY };


void HandlePericopes(const OutputType output_type, const std::string &bible_reference_candidate,
                     const std::string &pericopes_to_codes_map_filename) {
    if (output_type == DEBUG)
        std::cout << "Entering HandlePericopes().\n";

    std::unordered_multimap<std::string, std::string> pericopes_to_codes_map;
    MapUtil::DeserialiseMap(pericopes_to_codes_map_filename, &pericopes_to_codes_map);

    if (output_type == DEBUG)
        std::cout << "In HandlePericopes: looking for \"" << bible_reference_candidate << "\".\n";

    const auto begin_end(pericopes_to_codes_map.equal_range(bible_reference_candidate));
    if (begin_end.first != begin_end.second) {
        if (output_type == DEBUG)
            std::cerr << "Found a pericope to codes mapping.\n";

        std::string query;
        for (auto pair(begin_end.first); pair != begin_end.second; ++pair) {
            if (output_type != DEBUG) {
                if (not query.empty())
                    query += (output_type == RANGE_QUERY) ? " " : " OR ";
                query +=
                    (output_type == RANGE_QUERY) ? StringUtil::Map(pair->second, ':', '_') : RangeUtil::ConvertToDatesQuery(pair->second);
            } else
                std::cout << pair->second << '\n';
        }
        if (output_type != DEBUG)
            std::cout << query << '\n';

        std::exit(EXIT_SUCCESS);
    }
}


void HandleBookRanges(const OutputType output_type, const std::string &books_of_the_bible_to_canonical_form_map_filename,
                      const std::string &books_of_the_bible_to_code_map_filename, const std::string &bible_reference_candidate) {
    RegexMatcher * const matcher(RegexMatcher::RegexMatcherFactory("^([12])-([23])\\s*([A-ZÖa-zö]+)\\s*$"));
    if (not matcher->matched(bible_reference_candidate))
        return;

    unsigned starting_volume;
    if (not StringUtil::ToUnsigned((*matcher)[1], &starting_volume) or starting_volume > 2)
        LOG_ERROR("\"" + (*matcher)[1] + "\" is not a valid starting volume!");

    unsigned ending_volume;
    if (not StringUtil::ToUnsigned((*matcher)[2], &ending_volume) or ending_volume > 3 or ending_volume <= starting_volume)
        LOG_ERROR("\"" + (*matcher)[2] + "\" is not a valid ending volume!");

    const std::string non_canonical_book_name((*matcher)[3]);
    RangeUtil::BibleBookCanoniser bible_book_canoniser(books_of_the_bible_to_canonical_form_map_filename);
    const std::string starting_bible_book_candidate(
        bible_book_canoniser.canonise(std::to_string(starting_volume) + non_canonical_book_name, output_type == DEBUG));

    const std::string ending_bible_book_candidate(
        bible_book_canoniser.canonise(std::to_string(ending_volume) + non_canonical_book_name, output_type == DEBUG));

    if (output_type == DEBUG) {
        std::cout << "Identified a bible book range.  Starting volume " << starting_volume << ", ending volume " << ending_volume
                  << ", book is \"" << non_canonical_book_name << "\".\n";
    }

    RangeUtil::BibleBookToCodeMapper bible_book_to_code_mapper(books_of_the_bible_to_code_map_filename);
    const std::string first_book_code(bible_book_to_code_mapper.mapToCode(starting_bible_book_candidate, output_type == DEBUG));
    if (first_book_code.empty())
        LOG_ERROR("failed to map \"" + starting_bible_book_candidate + "\" to a bible code!");
    const std::string second_book_code(bible_book_to_code_mapper.mapToCode(ending_bible_book_candidate, output_type == DEBUG));
    if (second_book_code.empty())
        LOG_ERROR("failed to map \"" + ending_bible_book_candidate + "\" to a bible code!");

    const std::string query((first_book_code + std::string(RangeUtil::MAX_CHAPTER_LENGTH + RangeUtil::MAX_VERSE_LENGTH, '0'))
                            + (output_type == RANGE_QUERY ? '_' : ':')
                            + (second_book_code + std::string(RangeUtil::MAX_CHAPTER_LENGTH + RangeUtil::MAX_VERSE_LENGTH, '9')));
    std::cout << ((output_type == RANGE_QUERY) ? query : RangeUtil::ConvertToDatesQuery(query)) << '\n';

    std::exit(EXIT_SUCCESS);
}


void GenerateQuery(const OutputType output_type, std::string book_candidate, const std::string &chapters_and_verses_candidate,
                   const std::string &books_of_the_bible_to_code_map_filename,
                   const std::string &books_of_the_bible_to_canonical_form_map_filename) {
    RangeUtil::BibleBookCanoniser bible_book_canoniser(books_of_the_bible_to_canonical_form_map_filename);
    book_candidate = bible_book_canoniser.canonise(book_candidate, output_type == DEBUG);
    RangeUtil::BibleBookToCodeMapper bible_book_to_code_mapper(books_of_the_bible_to_code_map_filename);
    const std::string book_code(bible_book_to_code_mapper.mapToCode(book_candidate, output_type == DEBUG));
    if (book_code.empty())
        LOG_ERROR("failed to map \"" + book_candidate + "\" to a bible code!");
    if (output_type == DEBUG)
        std::cerr << "book code = \"" << book_code << "\"\n";

    const char separator(output_type == RANGE_QUERY ? '_' : ':');
    std::string query;
    if (chapters_and_verses_candidate.empty()) {
        query = book_code + std::string(RangeUtil::MAX_CHAPTER_LENGTH + RangeUtil::MAX_VERSE_LENGTH, '0') + separator + book_code
                + std::string(RangeUtil::MAX_CHAPTER_LENGTH + RangeUtil::MAX_VERSE_LENGTH, '9');
        std::cout << (output_type == DATE_RANGE_QUERY ? RangeUtil::ConvertToDatesQuery(query) : query) << '\n';

        std::exit(EXIT_SUCCESS);
    }

    std::set<std::pair<std::string, std::string>> start_end;
    if (not RangeUtil::ParseBibleReference(chapters_and_verses_candidate, book_code, &start_end)) {
        if (output_type == DEBUG)
            std::cerr << "The parsing of \"" << chapters_and_verses_candidate << "\" as chapters and verses failed!\n";
        std::exit(EXIT_FAILURE);
    }

    for (const auto &pair : start_end) {
        query = pair.first + separator + pair.second;
        std::cout << (output_type == DATE_RANGE_QUERY ? RangeUtil::ConvertToDatesQuery(query) : query) << '\n';
    }
}


void HandleOrdinaryReferences(const OutputType output_type, const std::string &bible_query_candidate,
                              const std::string &books_of_the_bible_to_code_map_filename,
                              const std::string &books_of_the_bible_to_canonical_form_map_filename) {
    LOG_DEBUG("Entering HandleOrdinaryReferences.");
    std::vector<std::string> book_candidate, chapters_and_verses_candidate;
    RangeUtil::SplitIntoBooksAndChaptersAndVerses(bible_query_candidate, &book_candidate, &chapters_and_verses_candidate);
    if (output_type == DEBUG) {
        for (unsigned i(0); i < book_candidate.size(); ++i) {
            std::cerr << "book_candidate[" << i << "] = \"" << book_candidate[i] << "\"\n";
            std::cerr << "chapters_and_verses_candidate[" << i << "] = \"" << chapters_and_verses_candidate[i] << "\"\n";
        }
    }

    for (unsigned i(0); i < book_candidate.size(); ++i)
        GenerateQuery(output_type, book_candidate[i], chapters_and_verses_candidate[i], books_of_the_bible_to_code_map_filename,
                      books_of_the_bible_to_canonical_form_map_filename);
}


} // unnamed namespace


int Main(int argc, char **argv) {
    if (argc < 3)
        Usage();

    OutputType output_type;
    if (std::strcmp(argv[1], "--debug") == 0) {
        output_type = DEBUG;
        ++argv, --argc;
    } else if (std::strcmp(argv[1], "--query") == 0) {
        output_type = RANGE_QUERY;
        ++argv, --argc;
    } else if (std::strcmp(argv[1], "--date-query") == 0) {
        output_type = DATE_RANGE_QUERY;
        ++argv, --argc;
    } else
        Usage();

    std::string map_file_path;
    if (StringUtil::StartsWith(argv[1], "--map-file-directory=")) {
        map_file_path = argv[1] + __builtin_strlen("--map-file-directory=");
        map_file_path += '/';
        ++argv, --argc;
    }

    if (argc != 6 and not(not map_file_path.empty() and argc == 2))
        Usage();

    std::string query(argv[1]);
    TextUtil::NormaliseDashes(&query);
    std::string bible_query_candidate(StringUtil::Trim(TextUtil::CollapseWhitespace(TextUtil::UTF8ToLower(query))));
    const std::string bible_aliases_map_filename(map_file_path + (argc == 2 ? "bible_aliases.map" : argv[2]));
    const std::string books_of_the_bible_to_code_map_filename(map_file_path + (argc == 2 ? "books_of_the_bible_to_code.map" : argv[3]));
    const std::string books_of_the_bible_to_canonical_form_map_filename(
        map_file_path + (argc == 2 ? "books_of_the_bible_to_canonical_form.map" : argv[4]));
    const std::string pericopes_to_codes_map_filename(map_file_path + (argc == 2 ? "pericopes_to_codes.map" : argv[5]));

    const RangeUtil::BibleAliasMapper alias_mapper(bible_aliases_map_filename);
    bible_query_candidate = alias_mapper.map(bible_query_candidate, output_type == DEBUG);

    HandleBookRanges(output_type, books_of_the_bible_to_canonical_form_map_filename, books_of_the_bible_to_code_map_filename,
                     bible_query_candidate);
    HandlePericopes(output_type, bible_query_candidate, pericopes_to_codes_map_filename);
    HandleOrdinaryReferences(output_type, bible_query_candidate, books_of_the_bible_to_code_map_filename,
                             books_of_the_bible_to_canonical_form_map_filename);

    return EXIT_SUCCESS;
}
