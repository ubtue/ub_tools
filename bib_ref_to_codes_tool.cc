#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <cctype>
#include <cstdlib>
#include "BibleReferenceParser.h"
#include "StringUtil.h"
#include "util.h"


// Expects an input file with lines of the form XXX=YYY.  All embedded spaces are significant.
void LoadMapFromFile(const std::string &input_filename,
		     std::unordered_map<std::string, std::string> * const map)
{
    map->clear();

    std::ifstream input(input_filename, std::ofstream::in);
    if (input.fail())
	Error("Failed to open \"" + input_filename + "\" for reading!");

    unsigned line_no(0);
    for (std::string line; std::getline(input, line); /* Intentionally empty! */) {
	++line_no;
	const size_t equal_pos(line.find('='));
	if (equal_pos == std::string::npos)
	    Error("Missing equal-sign in \"" + input_filename + "\" on line " + std::to_string(line_no) + "!");
	const std::string book_name(line.substr(0, equal_pos));
	const std::string code(line.substr(equal_pos + 1));
	if (book_name.empty() or code.empty())
	    Error("Bad input in \"" + input_filename + "\" on line " + std::to_string(line_no) + "!");
	(*map)[book_name] = code;
    }
}
	

// Expects an input file with lines of the form XXX=AAA;BBB;CCC.  All embedded spaces are significant.
// Embedded slashes, equal-signs, and semicolons are expected to be escaped with a leading slash.
void LoadMultimapFromFile(const std::string &input_filename,
			  std::unordered_multimap<std::string, std::string> * const multimap)
{
    multimap->clear();

    std::ifstream input(input_filename, std::ofstream::in);
    if (input.fail())
	Error("Failed to open \"" + input_filename + "\" for reading!");

    unsigned line_no(0);
    for (std::string line; std::getline(input, line); /* Intentionally empty! */) {
	++line_no;

	std::string key, value;
	bool in_key(true), escaped(false);
	for (const char ch : line) {
	    if (escaped) {
		escaped = false;
		if (in_key)
		    key += ch;
		else
		    value += ch;
	    } else if (ch == '\\')
		escaped = true;
	    else if (ch == '=') {
		if (key.empty())
		    Error("Missing key in \"" + input_filename + "\" on line " + std::to_string(line_no) + "!");
		in_key = false;
	    } else if (in_key)
		  key += ch;
	    else if (ch == ';') {
		if (value.empty())
		    Error("Ilegal empty value in \"" + input_filename + "\" on line " + std::to_string(line_no)
			  + "!");
		multimap->emplace(key, value);
		value.clear();
	    } else
		value += ch;
	}
	if (key.empty() or value.empty())
	    Error("Bad input in \"" + input_filename + "\" on line " + std::to_string(line_no) + "!");
    }
}


void SplitIntoBookAndChaptersAndVerses(const std::string &bib_ref_candidate, std::string * const book_candidate,
				       std::string * const chapters_and_verses_candidate)
{
    book_candidate->clear();
    chapters_and_verses_candidate->clear();

    const size_t len(bib_ref_candidate.length());
    if (len <= 3)
	*book_candidate = bib_ref_candidate;
    else if (isdigit(bib_ref_candidate[len - 1])
	     or (isalpha(bib_ref_candidate[len - 1]) and isdigit(bib_ref_candidate[len - 2])))
    {
	const size_t last_space_pos(bib_ref_candidate.rfind(' '));
	if (last_space_pos == std::string::npos)
	    *book_candidate = bib_ref_candidate;
	else {
	    *book_candidate = bib_ref_candidate.substr(0, last_space_pos);
	    *chapters_and_verses_candidate = bib_ref_candidate.substr(last_space_pos + 1);
	}
    } else
	*book_candidate = bib_ref_candidate;
}


void Usage() {
    std::cerr << "usage: " << progname << " bible_reference_candidate books_of_the_bible_to_code_map\n";
    std::cerr << "          books_of_the_bible_to_canonical_form_map pericopes_to_codes_map\n";
    std::exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    progname = argv[0];

    if (argc != 5)
	Usage();

    //
    // Deal with pericopes first...
    //

    std::unordered_multimap<std::string, std::string> pericopes_to_codes_map;
    LoadMultimapFromFile(argv[4], &pericopes_to_codes_map);

    std::string bib_ref_candidate(StringUtil::Trim(StringUtil::ToLower(argv[1])));
    StringUtil::CollapseWhitespace(&bib_ref_candidate);

    const auto begin_end(pericopes_to_codes_map.equal_range(bib_ref_candidate));
    if (begin_end.first != begin_end.second) {
	for (auto pair(begin_end.first); pair != begin_end.second; ++pair)
	    std::cout << pair->second << '\n';
	return EXIT_SUCCESS;
    }

    //
    // ...now deal w/ ordinary references.
    //

    std::string book_candidate, chapters_and_verses_candidate;
    SplitIntoBookAndChaptersAndVerses(bib_ref_candidate, &book_candidate, &chapters_and_verses_candidate);

    // Map from noncanonical bible book forms to the canonical ones:
    std::unordered_map<std::string, std::string> books_of_the_bible_to_canonical_form_map;
    LoadMapFromFile(argv[3], &books_of_the_bible_to_canonical_form_map);
    const auto non_canonical_form_and_canonical_form(books_of_the_bible_to_canonical_form_map.find(book_candidate));
    if (non_canonical_form_and_canonical_form != books_of_the_bible_to_canonical_form_map.end())
	book_candidate = non_canonical_form_and_canonical_form->second;

    std::unordered_map<std::string, std::string> bible_books_to_codes_map;
    LoadMapFromFile(argv[2], &bible_books_to_codes_map);
    const auto bible_book_and_code(bible_books_to_codes_map.find(book_candidate));
    if (bible_book_and_code == bible_books_to_codes_map.end())
	return EXIT_FAILURE; // Unknown bible book!

    const std::string book_code(bible_book_and_code->second);
    if (chapters_and_verses_candidate.empty()) {
	std::cout << book_code << "00000:" << book_code << "99999";
	return EXIT_SUCCESS;
    }

    std::set<std::pair<std::string, std::string>> start_end;
    if (not ParseBibleReference(bib_ref_candidate, book_code, &start_end))
	return EXIT_FAILURE;

    for (const auto &pair : start_end)
	std::cout << pair.first << ':' << pair.second << '\n';
}
