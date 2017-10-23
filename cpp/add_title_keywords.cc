/** \brief A tool for adding keywords extracted from titles to MARC records.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015-2017 Universitätsbibliothek Tübingen.  All rights reserved.
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
#include "MarcReader.h"
#include "MarcRecord.h"
#include "MarcWriter.h"
#include "StringUtil.h"
#include "Subfields.h"
#include "TextUtil.h"
#include "util.h"
#include "XmlWriter.h"


void Usage() {
    std::cerr << "Usage: " << ::progname << " [--verbose] master_marc_input marc_output [stopwords_files]\n";
    std::cerr << "       Stopword files must be named \"stopwords.xxx\" where xxx has to be a 3-letter\n";
    std::cerr << "       language code.\n";
    std::exit(EXIT_FAILURE);
}


void LoadStopwords(const bool verbose, File * const input, std::unordered_set<std::string> * const stopwords_set) {
    if (verbose)
        std::cout << "Starting loading of stopwords.\n";

    unsigned count(0);
    while (not input->eof()) {
        const std::string line(input->getline());
        if (line.empty() or line[0] == ';') // Empty or comment line?
            continue;

        stopwords_set->insert(StringUtil::ToLower(line));
        ++count;
    }

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


bool HasExpertAssignedKeywords(const MarcRecord &record) {
    const std::vector<std::string> keyword_fields{ "600", "610", "611", "630", "648", "650", "651", "653", "655", "656", "689" };
    for (const auto &keyword_field : keyword_fields) {
        if (record.getFieldIndex(keyword_field) != MarcRecord::FIELD_NOT_FOUND)
            return true;
    }

    return false;
}


void AugmentKeywordsWithTitleWords(const bool verbose, MarcReader * const marc_reader,
    MarcWriter * const marc_writer,
    const std::map<std::string, std::unordered_set<std::string>> &language_codes_to_stopword_sets)
{
    if (verbose)
        std::cerr << "Starting augmentation of stopwords.\n";

    unsigned total_count(0), augment_count(0), title_count(0);
    while (MarcRecord record = marc_reader->read()) {
        ++total_count;

        // Do not attempt to generate title keywords if we have expert-assigned keywords:
        if (HasExpertAssignedKeywords(record)) {
            marc_writer->write(record);
            continue;
        }

        const size_t title_index(record.getFieldIndex("245"));
        if (title_index == MarcRecord::FIELD_NOT_FOUND) {
            marc_writer->write(record);
            continue;
        }

        const Subfields subfields(record.getSubfields(title_index));
        if (not subfields.hasSubfield('a')) {
            marc_writer->write(record);
            continue;
        }

        const auto begin_end_a = subfields.getIterators('a');
        std::string title(begin_end_a.first->value_);
        const auto begin_end_b = subfields.getIterators('b');
        if (begin_end_b.first != begin_end_b.second) {
            title += " " + begin_end_b.first->value_;
        }

        ++title_count;

        std::unordered_set<std::string> title_words;
        TextUtil::ChopIntoWords(title, &title_words, /* min_word_length = */ 3);
        LowercaseSet(&title_words);

        const std::string &language_code(record.getLanguage());
        const auto code_and_stopwords(language_codes_to_stopword_sets.find(language_code));
        if (code_and_stopwords != language_codes_to_stopword_sets.end())
            FilterOutStopwords(code_and_stopwords->second, &title_words);
        if (language_code != "eng") // Hack, because people suck at cataloging!
            FilterOutStopwords(language_codes_to_stopword_sets.find("eng")->second, &title_words);

        if (title_words.empty()) {
            marc_writer->write(record);
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
    ::progname = argv[0];

    if (argc < 3)
        Usage();

    const bool verbose(std::strcmp(argv[1], "--verbose") == 0);
    if (verbose and argc < 4)
        Usage();

    const std::string marc_input_filename(argv[verbose ? 2 : 1]);
    const std::string marc_output_filename(argv[verbose ? 3 : 2]);
    if (unlikely(marc_input_filename == marc_output_filename))
        logger->error("MARC input file name equals MARC output file name!");

    std::unique_ptr<MarcReader> marc_reader(MarcReader::Factory(marc_input_filename, MarcReader::BINARY));
    std::unique_ptr<MarcWriter> marc_writer(MarcWriter::Factory(marc_output_filename, MarcWriter::BINARY));

    // Read optional stopword lists:
    std::map<std::string, std::unordered_set<std::string>> language_codes_to_stopword_sets;
    for (int arg_no(verbose ? 4 : 3); arg_no < argc; ++arg_no) {
        const std::string stopwords_filename(argv[arg_no]);
        if (stopwords_filename.length() != 13 or
            not StringUtil::StartsWith(stopwords_filename, "stopwords."))
            logger->error("Invalid stopwords filename \"" + stopwords_filename + "\"!");
        const std::string language_code(stopwords_filename.substr(10));
        File stopwords(stopwords_filename, "r");
        if (not stopwords)
            logger->error("can't open \"" + stopwords_filename + "\" for reading!");
        std::unordered_set<std::string> stopwords_set;
        LoadStopwords(verbose, &stopwords, &stopwords_set);
        language_codes_to_stopword_sets[language_code] = stopwords_set;
    }

    // We always need English because librarians suck at specifying English:
    if (language_codes_to_stopword_sets.find("eng") == language_codes_to_stopword_sets.end())
        logger->error("You always need to provide \"stopwords.eng\"!");

    AugmentKeywordsWithTitleWords(verbose, marc_reader.get(), marc_writer.get(), language_codes_to_stopword_sets);
}
