/** \brief A tool for adding keywords extracted from titles to MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
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

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <cstdlib>
#include <cstring>
#include "DirectoryEntry.h"
#include "Leader.h"
#include "MarcUtil.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TextUtil.h"
#include "util.h"


void Usage() {
    std::cerr << "Usage: " << progname << " [--verbose] master_marc_input marc_output [stopwords_files]\n";
    std::cerr << "       Stopword files must be named \"stopwords.xxx\" where xxx has to be a 3-letter\n";
    std::cerr << "       language code.\n";
    std::exit(EXIT_FAILURE);
}


void LoadStopwords(const bool verbose, FILE * const input,
                   std::unordered_set<std::string> * const stopwords_set)
{
    if (verbose)
        std::cout << "Starting loading of stopwords.\n";

    unsigned count(0);
    while (not std::feof(input)) {
        char buf[1024];
        if (std::fgets(buf, sizeof buf, input) == nullptr)
            break;
        if (buf[0] == '\0' or buf[0] == ';') // Empty or comment line?
            continue;

        size_t len(std::strlen(buf));
        if (buf[len - 1] == '\n')
            --len;

        stopwords_set->insert(StringUtil::ToLower(std::string(buf, len)));
        ++count;
    }

    if (std::ferror(input))
        Error("Read error while trying to read the stopwords file.");

    if (verbose)
        std::cerr << "Read " << count << " stopwords.\n";
}


void LowercaseSet(std::unordered_set<std::string> * const words) {
    std::unordered_set<std::string> lowercase_set;
    for (const auto &word : *words)
        lowercase_set.insert(StringUtil::ToLower(word));
    std::swap(lowercase_set, *words);
}


void FilterOutStopwords(const std::unordered_set<std::string> &stopwords,
                        std::unordered_set<std::string> * const words)
{
    for (std::unordered_set<std::string>::iterator word(words->begin()); word != words->end(); /* Empty! */) {
        if (stopwords.find(*word) != stopwords.end())
            word = words->erase(word);
        else
            ++word;
    }
}


std::string ConcatSet(const std::unordered_set<std::string> &words) {
    std::string retval;
    for (const auto &word : words)
        retval += word + " ";
    return retval;
}


void AugmentStopwordsWithTitleWords(
    const bool verbose, FILE * const input, FILE * const output,
    const std::map<std::string, std::unordered_set<std::string>> &language_codes_to_stopword_sets)
{
    if (verbose)
        std::cerr << "Starting augmentation of stopwords.\n";

    unsigned total_count(0), augment_count(0), title_count(0);
    while (std::feof(input) == 0) {
	const MarcUtil::Record record(input);
        ++total_count;

	const std::vector<DirectoryEntry> &dir_entries(record.getDirEntries());
        const auto entry_iterator(DirectoryEntry::FindField("245", dir_entries));
        if (entry_iterator == dir_entries.end()) {
	    record.write(output);
            continue;
        }

        const size_t title_index(entry_iterator - dir_entries.begin());
	const std::vector<std::string> &fields(record.getFields());
        Subfields subfields(fields[title_index]);
        if (not subfields.hasSubfield('a')) {
	    record.write(output);
            continue;
        }

        const auto begin_end_a = subfields.getIterators('a');
        std::string title(begin_end_a.first->second);
        const auto begin_end_b = subfields.getIterators('b');
        if (begin_end_b.first != begin_end_b.second) {
            title += " " + begin_end_b.first->second;
        }

        ++title_count;

        std::unordered_set<std::string> title_words;
        TextUtil::ChopIntoWords(title, &title_words, /* min_word_length = */ 3);
        LowercaseSet(&title_words);

        const std::string language_code(record.getLanguage());
        const auto code_and_stopwords(language_codes_to_stopword_sets.find(language_code));
        if (code_and_stopwords != language_codes_to_stopword_sets.end())
            FilterOutStopwords(code_and_stopwords->second, &title_words);
        if (language_code != "eng") // Hack because people suck at cataloging!
            FilterOutStopwords(language_codes_to_stopword_sets.find("eng")->second, &title_words);

        if (title_words.empty()) {
	    record.write(output);
            continue;
        }

        for (const auto &word : title_words)
            std::cout << word << ' ' << language_code << '\n';

        ++augment_count;
    }

    if (verbose) {
        std::cerr << title_count << " records had titles in 245a.\n";
        std::cerr << "Augmented " << augment_count << " records of " << total_count
                  << " records with title words.\n";
    }
}


int main(int argc, char **argv) {
    progname = argv[0];

    if (argc < 3)
        Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose and argc < 4)
        Usage();

    const std::string marc_input_filename(argv[verbose ? 2 : 1]);
    FILE *marc_input = std::fopen(marc_input_filename.c_str(), "rm");
    if (marc_input == nullptr)
        Error("can't open \"" + marc_input_filename + "\" for reading!");

    const std::string marc_output_filename(argv[verbose ? 3 : 2]);
    FILE *marc_output = std::fopen(marc_output_filename.c_str(), "wb");
    if (marc_output == nullptr)
        Error("can't open \"" + marc_output_filename + "\" for writing!");

    if (unlikely(marc_input_filename == marc_output_filename))
        Error("MARC input file name equals MARC output file name!");

    // Read optional stopword lists:
    std::map<std::string, std::unordered_set<std::string>> language_codes_to_stopword_sets;
    for (int arg_no(verbose ? 4 : 3); arg_no < argc; ++arg_no) {
        const std::string stopwords_filename(argv[arg_no]);
        if (stopwords_filename.length() != 13 or
            not StringUtil::StartsWith(stopwords_filename, "stopwords."))
            Error("Invalid stopwords filename \"" + stopwords_filename + "\"!");
        const std::string language_code(stopwords_filename.substr(10));
        FILE *stopwords = std::fopen(stopwords_filename.c_str(), "rm");
        if (stopwords == nullptr)
            Error("can't open \"" + stopwords_filename + "\" for reading!");
        std::unordered_set<std::string> stopwords_set;
        LoadStopwords(verbose, stopwords, &stopwords_set);
        language_codes_to_stopword_sets[language_code] = stopwords_set;
        std::fclose(stopwords);
    }

    // We always need English because librarians suck at specifying English:
    if (language_codes_to_stopword_sets.find("eng") == language_codes_to_stopword_sets.end())
        Error("You always need to provide \"stopwords.eng\"!");

    AugmentStopwordsWithTitleWords(verbose, marc_input, marc_output, language_codes_to_stopword_sets);

    std::fclose(marc_input);
    std::fclose(marc_output);
}
